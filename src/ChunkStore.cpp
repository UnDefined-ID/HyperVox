#include "ChunkStore.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <unordered_set>
#include <ctime>
#include <fstream>
#include "Anvil.hpp"
#include "Perf.hpp"
namespace fs=std::filesystem;
namespace{
constexpr int kDataVersion=4671;
uint64_t rkey(int rx,int rz){return (uint64_t(uint32_t(rx))<<32)|uint32_t(rz);}
bool endsWith(const std::string&s,const char*e){const size_t l=std::strlen(e);return s.size()>=l&&s.compare(s.size()-l,l,e)==0;}
bool has(const std::string&s,const char*p){return s.find(p)!=std::string::npos;}

// Minecraft block name -> our block.  Things we cannot draw as cubes get the closest cube, plants/decorations vanish, the rest is MISSING.
uint16_t resolveName(std::string n){
 const Registry&R=Registry::get();
 if(n.compare(0,10,"minecraft:")==0)n=n.substr(10);
 if(n=="air"||n=="cave_air"||n=="void_air")return 0;
 if(n=="enchanting_table")n="enchanting_table_block"; if(n=="magma_block")n="magma"; if(n.compare(0,7,"potted_")==0)return 0;
 if(uint16_t id=R.shapeHead(n))return id;        // slabs, stairs, carpets, torches, plants, chests ...
 if(uint16_t id=R.blockByName(n))return id;
 auto tryN=[&](const std::string&s)->uint16_t{return R.blockByName(s);};
 // shapes -> their material
 for(const char*suf:{"_stairs","_slab","_wall","_fence_gate","_fence"}){
  if(!endsWith(n,suf))continue; std::string b=n.substr(0,n.size()-std::strlen(suf));
  for(const std::string&c:{b,b+"s",b+"_planks",b+"_block",b+"_bricks","cut_"+b,b+"_wool"})if(uint16_t id=tryN(c))return id;
  if(b=="stone_brick"||b=="mossy_stone_brick"||b=="nether_brick"||b=="brick")if(uint16_t id=tryN(b+"s"))return id;
 }
 if(endsWith(n,"_carpet")){ const std::string b=n.substr(0,n.size()-7); if(uint16_t id=tryN(b+"_wool"))return id; return 0; }
 if(endsWith(n,"_glass_pane")||n=="glass_pane"){ if(uint16_t id=tryN(n.substr(0,n.size()-5)))return id; }
 if(endsWith(n,"_wood")||endsWith(n,"_hyphae")){ const std::string b=n.substr(0,n.size()-(endsWith(n,"_wood")?5:7)); if(uint16_t id=tryN(b+(endsWith(n,"_wood")?"_log":"_stem")))return id; }
 if(n.compare(0,9,"stripped_")==0)if(uint16_t id=tryN(n))return id;
 // non-solid decoration: leave it out instead of covering the terrain with MISSING
 static const char*deco[]={"grass","fern","flower","tulip","dandelion","poppy","orchid","allium","bluet","cornflower","lily","rose","peony","sapling","torch","vine","lichen","snow","sign","button","lever","rail","redstone_wire","tripwire","ladder","bush","kelp","seagrass","sugar_cane","bamboo","cobweb","dripstone","propagule","roots","spore_blossom","azalea","candle","head","banner","carpet","pot","lantern","fire","bubble_column","light","coral_fan","pickle","sprouts","fungus","crop","wheat","potatoes","carrots","beetroots","stem","berry","berries","dead_bush","daisy","hanging","glow_lichen","door","trapdoor","bars","pressure_plate","frogspawn","egg","amethyst_cluster","bud"};
 for(const char*d:deco)if(has(n,d))return 0;
 { static std::mutex mu; static std::unordered_set<std::string> seen; std::lock_guard<std::mutex> g(mu); if(seen.insert(n).second)std::fprintf(stderr,"[hypervox] unsupported block 'minecraft:%s' -> shown as MISSING\n",n.c_str()); }
 return R.missing;
}
uint16_t mapBlock(const std::string&name,const std::string&axis,const std::string&facing,const std::string&half,const std::string&type){
 static thread_local std::unordered_map<std::string,uint16_t> cache; const std::string key=name+'|'+axis+'|'+facing+'|'+half+'|'+type; auto it=cache.find(key); if(it!=cache.end())return it->second;
 uint16_t id=resolveName(name); if(id)id=Registry::get().withState(id,axis,facing,half,type); cache.emplace(key,id); return id;
}
std::string mcName(const std::string&n){return "minecraft:"+(n=="enchanting_table_block"?std::string("enchanting_table"):n);}

bool nbtToChunk(const Nbt&root,Chunk&c){
 const Nbt*secs=root.get("sections"); if(!secs)return false;
 for(auto&sec:secs->list){
  const Nbt*yy=sec.get("Y"); const Nbt*bs=sec.get("block_states"); if(!yy||!bs)continue;
  const int sy=(int)yy->i+Chunk::Y_OFFSET/16; if(sy<0||sy>=Chunk::HEIGHT/16)continue;
  const Nbt*pl=bs->get("palette"); if(!pl||pl->list.empty())continue;
  std::vector<uint16_t> ids; ids.reserve(pl->list.size());
  for(auto&pe:pl->list){ const Nbt*nm=pe.get("Name"); std::string ax,fc,hf,ty; if(const Nbt*pr=pe.get("Properties")){ if(const Nbt*a=pr->get("axis"))ax=a->s; if(const Nbt*f=pr->get("facing"))fc=f->s; if(const Nbt*q=pr->get("half"))hf=q->s; if(const Nbt*q=pr->get("type"))ty=q->s; } ids.push_back(nm?mapBlock(nm->s,ax,fc,hf,ty):uint16_t(0)); }
  const Nbt*da=bs->get("data"); int bits=4; while((1<<bits)<(int)ids.size())bits++; const int per=64/bits;
  for(int i=0;i<4096;i++){
   size_t k=0; if(ids.size()>1&&da){const size_t li=(size_t)(i/per); if(li>=da->la.size())break; k=(size_t)(((uint64_t)da->la[li]>>((i%per)*bits))&((1ull<<bits)-1));}
   if(k>=ids.size())continue; const uint16_t id=ids[k]; if(!id)continue;
   const int x=i&15,z=(i>>4)&15,gy=sy*16+(i>>8); c.at(x,gy,z)=id; if(gy>c.maxY)c.maxY=gy;
  }
 }
 return true;
}
Nbt chunkNbt(int cx,int cz,const Chunk&c){
 Nbt ch=Nbt::Comp_(); const Registry&R=Registry::get();
 ch.set("DataVersion",Nbt::Int_(kDataVersion)); ch.set("xPos",Nbt::Int_(cx)); ch.set("zPos",Nbt::Int_(cz)); ch.set("yPos",Nbt::Int_(-4)); ch.set("Status",Nbt::Str_("minecraft:full"));
 ch.set("LastUpdate",Nbt::Long_(0)); ch.set("InhabitedTime",Nbt::Long_(0)); ch.set("isLightOn",Nbt::Byte_(0));
 Nbt secs=Nbt::List_(Nbt::Comp);
 for(int sy=0;sy<Chunk::HEIGHT/16;sy++){
  if(sy*16>c.maxY)break;
  std::vector<uint16_t> pal; std::vector<int> idx(4096); bool any=false;
  for(int y=0;y<16;y++)for(int z=0;z<16;z++)for(int x=0;x<16;x++){
   uint16_t id=c.at(x,sy*16+y,z); if(id==R.missing&&id)id=0; if(id)any=true;
   int k=-1; for(size_t i=0;i<pal.size();i++)if(pal[i]==id){k=(int)i;break;} if(k<0){k=(int)pal.size();pal.push_back(id);} idx[(y*16+z)*16+x]=k; }
  if(!any)continue;                                           // all-air sections are simply left out
  Nbt sec=Nbt::Comp_(); sec.set("Y",Nbt::Byte_(sy-Chunk::Y_OFFSET/16));
  Nbt bsn=Nbt::Comp_(); Nbt pl=Nbt::List_(Nbt::Comp);
  for(uint16_t id:pal){ Nbt e=Nbt::Comp_(); std::string bn; std::vector<std::pair<std::string,std::string>> pp;
   if(id&&R.stateOf(id,bn,pp)){ e.set("Name",Nbt::Str_(mcName(bn))); if(!pp.empty()){ Nbt pr=Nbt::Comp_(); for(auto&kv:pp)pr.set(kv.first,Nbt::Str_(kv.second)); e.set("Properties",std::move(pr)); } }
   else e.set("Name",Nbt::Str_(id?mcName(R.blockName(id)):"minecraft:air"));
   pl.list.push_back(std::move(e)); }
  bsn.set("palette",std::move(pl));
  if(pal.size()>1){
   int bits=4; while((1<<bits)<(int)pal.size())bits++; const int per=64/bits; std::vector<int64_t> data((4096+per-1)/per,0);
   for(int i=0;i<4096;i++)data[i/per]|=(int64_t)((uint64_t)idx[i]<<((i%per)*bits));
   bsn.set("data",Nbt::LongArr_(std::move(data)));
  }
  sec.set("block_states",std::move(bsn));
  Nbt bio=Nbt::Comp_(); Nbt bp=Nbt::List_(Nbt::Str); bp.list.push_back(Nbt::Str_("minecraft:plains")); bio.set("palette",std::move(bp)); sec.set("biomes",std::move(bio));
  secs.list.push_back(std::move(sec));
 }
 ch.set("sections",std::move(secs)); ch.set("block_entities",Nbt::List_(Nbt::Comp)); ch.set("Heightmaps",Nbt::Comp_()); ch.set("structures",Nbt::Comp_());
 return ch;
}
uint32_t be32(const uint8_t*p){return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|p[3];}
void putBe32(uint8_t*p,uint32_t v){p[0]=uint8_t(v>>24);p[1]=uint8_t(v>>16);p[2]=uint8_t(v>>8);p[3]=uint8_t(v);}
}

std::shared_ptr<ChunkStore::Region> ChunkStore::region(int rx,int rz) const{
 std::lock_guard<std::mutex> g(m_mu); auto it=m_regions.find(rkey(rx,rz)); if(it!=m_regions.end())return it->second;
 auto r=std::make_shared<Region>(); r->path=m_dir/("r."+std::to_string(rx)+"."+std::to_string(rz)+".mca");
 std::ifstream f(r->path,std::ios::binary); if(f){ r->header.resize(8192); f.read((char*)r->header.data(),8192); r->exists=bool(f)&&f.gcount()==8192; }
 m_regions.emplace(rkey(rx,rz),r); return r;
}

bool ChunkStore::load(int cx,int cz,Chunk&out) const{
 auto r=region(cx>>5,cz>>5); if(!r->exists)return false;
 const int idx=((cz&31)<<5)|(cx&31); std::vector<uint8_t> raw; int comp=0;
 { std::lock_guard<std::mutex> g(r->mu);
   const uint32_t loc=be32(&r->header[idx*4]); if(!loc)return false;
   std::ifstream f(r->path,std::ios::binary); if(!f)return false;
   f.seekg((std::streamoff)(loc>>8)*4096); uint8_t h[5]; f.read((char*)h,5); if(!f)return false;
   const uint32_t len=be32(h); comp=h[4]; if(len<1||len>(1u<<27))return false;
   raw.resize(len-1); f.read((char*)raw.data(),(std::streamsize)raw.size()); if(!f)return false; }
 std::vector<uint8_t> nbt;
 if(comp==1||comp==2){ if(!inflateAny(raw.data(),raw.size(),nbt))return false; } else if(comp==3)nbt=std::move(raw); else return false;
 Nbt root; if(!nbtRead(nbt.data(),nbt.size(),root))return false;
 out=Chunk(); if(!nbtToChunk(root,out))return false;
 return true;
}

bool ChunkStore::save(const World::DirtyList&dirty){
 PerfScope ps("saving chunks (background)",2000);
 std::error_code ec; fs::create_directories(m_dir,ec);
 std::unordered_map<uint64_t,std::vector<const World::DirtyList::value_type*>> byRegion;
 for(auto&d:dirty){ const int cx=(int)(int32_t)(d.first>>32),cz=(int)(int32_t)(d.first&0xFFFFFFFFu); byRegion[rkey(cx>>5,cz>>5)].push_back(&d); }
 bool ok=true;
 for(auto&kv:byRegion){
  const int rx=(int)(int32_t)(kv.first>>32),rz=(int)(int32_t)(kv.first&0xFFFFFFFFu); auto r=region(rx,rz);
  // compress everything first, outside the lock
  struct Rec{int idx;std::vector<uint8_t> data;}; std::vector<Rec> recs;
  for(auto*d:kv.second){
   const int cx=(int)(int32_t)(d->first>>32),cz=(int)(int32_t)(d->first&0xFFFFFFFFu);
   std::vector<uint8_t> nbt,z; nbtWrite(nbt,chunkNbt(cx,cz,*d->second)); if(!gzipCompress(nbt,z,false)){ok=false;continue;}
   Rec rc; rc.idx=((cz&31)<<5)|(cx&31); rc.data.resize(5+z.size()); putBe32(rc.data.data(),(uint32_t)z.size()+1); rc.data[4]=2; std::memcpy(rc.data.data()+5,z.data(),z.size()); recs.push_back(std::move(rc));
  }
  std::lock_guard<std::mutex> g(r->mu);
  if(!r->exists){ r->header.assign(8192,0); std::ofstream c(r->path,std::ios::binary|std::ios::trunc); c.write((const char*)r->header.data(),8192); if(!c){ok=false;continue;} r->exists=true; }
  std::fstream f(r->path,std::ios::in|std::ios::out|std::ios::binary); if(!f){ok=false;continue;}
  f.seekg(0,std::ios::end); std::streamoff fileSize=f.tellg(); if(fileSize<8192)fileSize=8192;
  const uint32_t now=(uint32_t)std::time(nullptr);
  for(auto&rc:recs){
   const uint32_t need=(uint32_t)((rc.data.size()+4095)/4096); const uint32_t old=be32(&r->header[rc.idx*4]);
   uint32_t start; if(old&&(old&255)>=need)start=old>>8; else { start=(uint32_t)((fileSize+4095)/4096); fileSize=(std::streamoff)(start+need)*4096; }
   std::vector<uint8_t> pad((size_t)need*4096,0); std::memcpy(pad.data(),rc.data.data(),rc.data.size());
   f.seekp((std::streamoff)start*4096); f.write((const char*)pad.data(),(std::streamsize)pad.size());
   putBe32(&r->header[rc.idx*4],(start<<8)|(need&255)); putBe32(&r->header[4096+rc.idx*4],now);
  }
  f.seekp(0); f.write((const char*)r->header.data(),8192); f.flush(); if(!f)ok=false;
 }
 return ok;
}
