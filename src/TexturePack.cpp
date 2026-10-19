#include "TexturePack.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <iostream>

namespace {
constexpr uint32_t kCacheVersion=6;
// Colormap tints (the pack ships grayscale grass / foliage / water that vanilla tints per biome).  alphaSet<0 = keep the texture alpha.
struct Tint{const char*name;int r,g,b;int alphaSet,alphaCap;};
const Tint kTints[]={
 {"grass_block_top",145,189,89,-1,255},
 {"oak_leaves",95,165,45,-1,255},{"jungle_leaves",95,165,45,-1,255},{"acacia_leaves",95,165,45,-1,255},
 {"dark_oak_leaves",95,165,45,-1,255},{"mangrove_leaves",95,165,45,-1,255},
 {"birch_leaves",128,167,85,-1,255},{"spruce_leaves",97,153,97,-1,255},
 {"short_grass",145,189,89,-1,255},{"fern",145,189,89,-1,255},{"sugar_cane",145,189,89,-1,255},
 {"water_still",63,118,228,-1,255},          // translucent, see-through water like vanilla
 {"tinted_glass",160,160,160,-1,150},         // 160 = no colour change; always see-through
};
const Tint* findTint(const std::string&k){ for(auto&t:kTints)if(k==t.name)return &t; return nullptr; }

// Box-filters frame `frame` of a (possibly animated strip) image down to a tinted 16x16 tile.
void makeTile(const AtlasImage&img,uint32_t frame,const Tint*tint,uint8_t*out){
 const uint32_t tile=std::min(img.width,img.height),oy0=frame*tile;
 for(uint32_t y=0;y<16;y++)for(uint32_t x=0;x<16;x++){
  const uint32_t x0=x*tile/16,x1=std::max(x0+1,(x+1)*tile/16),y0=oy0+y*tile/16,y1=std::max(y0+1,oy0+(y+1)*tile/16);
  uint32_t acc[4]={0,0,0,0},n=0,aw=0;
  for(uint32_t sy=y0;sy<y1&&sy<img.height;sy++)for(uint32_t sx=x0;sx<x1&&sx<img.width;sx++){const uint8_t*s=&img.rgba[((size_t)sy*img.width+sx)*4];acc[0]+=s[0]*s[3];acc[1]+=s[1]*s[3];acc[2]+=s[2]*s[3];acc[3]+=s[3];aw+=s[3];n++;}
  uint8_t px[4]={0,0,0,0}; if(n){ if(aw){px[0]=(uint8_t)(acc[0]/aw);px[1]=(uint8_t)(acc[1]/aw);px[2]=(uint8_t)(acc[2]/aw);} px[3]=(uint8_t)(acc[3]/n); }
  uint8_t*dst=out+((size_t)y*16+x)*4; std::copy(px,px+4,dst);
  if(tint){
   dst[0]=(uint8_t)std::min(255,dst[0]*tint->r/160); dst[1]=(uint8_t)std::min(255,dst[1]*tint->g/160); dst[2]=(uint8_t)std::min(255,dst[2]*tint->b/160);
   if(tint->alphaSet>=0)dst[3]=(uint8_t)tint->alphaSet; dst[3]=(uint8_t)std::min<int>(dst[3],tint->alphaCap);
  }
 }
}
// minimal reader for the "animation" block of a .png.mcmeta file
bool readMcmeta(const std::filesystem::path&p,int&frametime,bool&interp,std::vector<std::pair<int,int>>&frames){
 std::ifstream f(p); if(!f)return false; std::string s((std::istreambuf_iterator<char>(f)),std::istreambuf_iterator<char>());
 if(s.find("\"animation\"")==std::string::npos)return false;
 auto numAfter=[&](size_t from,const char*key,int def)->int{ size_t k=s.find(key,from); if(k==std::string::npos)return def; k=s.find(':',k); if(k==std::string::npos)return def; k++; while(k<s.size()&&s[k]==' ')k++; return std::atoi(s.c_str()+k); };
 frametime=std::max(1,numAfter(0,"\"frametime\"",1));
 { size_t k=s.find("\"interpolate\""); interp=false; if(k!=std::string::npos){ k=s.find(':',k); if(k!=std::string::npos){ k++; while(k<s.size()&&s[k]==' ')k++; interp=s.compare(k,4,"true")==0; } } }
 frames.clear(); size_t k=s.find("\"frames\"");
 if(k!=std::string::npos){ k=s.find('[',k); if(k!=std::string::npos){
   for(k++;k<s.size()&&s[k]!=']';){
    if(s[k]=='{'){ const size_t e=s.find('}',k); if(e==std::string::npos)break; const std::string o=s.substr(k,e-k); size_t i=o.find("\"index\""),t=o.find("\"time\"");
     int idx=0,tm=-1; if(i!=std::string::npos){i=o.find(':',i);idx=std::atoi(o.c_str()+i+1);} if(t!=std::string::npos){t=o.find(':',t);tm=std::atoi(o.c_str()+t+1);} frames.push_back({idx,tm}); k=e+1; }
    else if(s[k]>='0'&&s[k]<='9'){ frames.push_back({std::atoi(s.c_str()+k),-1}); while(k<s.size()&&s[k]>='0'&&s[k]<='9')k++; }
    else k++;
   } } }
 return true;
}
}

bool TexturePack::readCache(const std::filesystem::path&f,uint64_t sig){
 std::ifstream in(f,std::ios::binary); if(!in)return false;
 char magic[4]; in.read(magic,4); if(!in||std::memcmp(magic,"FVKB",4)!=0)return false;
 uint32_t ver=0,cols=0,count=0; uint64_t s=0;
 in.read((char*)&ver,4); in.read((char*)&s,8); in.read((char*)&cols,4); in.read((char*)&count,4);
 if(!in||ver!=kCacheVersion||s!=sig||cols!=kCols||count>100000)return false;
 std::unordered_map<std::string,uint16_t> ids;
 for(uint32_t i=0;i<count;i++){unsigned char l=0;in.read((char*)&l,1);std::string n(l,'\0');in.read(&n[0],l);uint16_t id=0;in.read((char*)&id,2);if(!in)return false;ids.emplace(n,id);}
 AtlasImage img; img.width=img.height=atlasSize(); img.rgba.resize((size_t)img.width*img.height*4);
 in.read((char*)img.rgba.data(),(std::streamsize)img.rgba.size()); if(!in)return false;
 m_ids=std::move(ids); m_atlas=std::move(img); return true;
}
void TexturePack::writeCache(const std::filesystem::path&f,uint64_t sig)const{
 std::error_code ec; std::filesystem::create_directories(f.parent_path(),ec);
 const auto tmp=std::filesystem::path(f).concat(".tmp");
 { std::ofstream out(tmp,std::ios::binary|std::ios::trunc); if(!out)return;
   const uint32_t ver=kCacheVersion,cols=kCols,count=(uint32_t)m_ids.size();
   out.write("FVKB",4); out.write((const char*)&ver,4); out.write((const char*)&sig,8); out.write((const char*)&cols,4); out.write((const char*)&count,4);
   for(auto&kv:m_ids){unsigned char l=(unsigned char)std::min<size_t>(kv.first.size(),255);out.write((const char*)&l,1);out.write(kv.first.data(),l);out.write((const char*)&kv.second,2);}
   out.write((const char*)m_atlas.rgba.data(),(std::streamsize)m_atlas.rgba.size()); if(!out)return; }
 std::filesystem::rename(tmp,f,ec); if(ec){std::filesystem::remove(f,ec);std::filesystem::rename(tmp,f,ec);}
}

bool TexturePack::load(const AssetPaths&assets,const std::filesystem::path&cacheDir){
 namespace fs=std::filesystem;
 m_assets=assets; m_ids.clear(); m_atlas={}; m_fromCache=false;
 if(m_assets.base.empty()||!fs::exists(m_assets.base/"block"))return false;
 const uint64_t sig=m_assets.signature(kCacheVersion);
 const fs::path cacheFile=cacheDir/("blocks_"+safeFileName(assets.packName)+".cache");
 if(readCache(cacheFile,sig)){m_fromCache=true;loadAnimations();return true;}

 std::vector<fs::path> files;
 for(auto&e:fs::recursive_directory_iterator(m_assets.base/"block"))if(e.is_regular_file()&&e.path().extension()==".png")files.push_back(e.path());
 std::sort(files.begin(),files.end());
 const uint32_t size=atlasSize(), cols=kCols, maxTiles=cols*cols;
 if(files.size()>maxTiles)return false;
 m_atlas.width=m_atlas.height=size; m_atlas.rgba.assign((size_t)size*size*4,0);
 uint32_t i=0;
 for(auto&p:files){
  const std::string k=p.stem().string();
  AtlasImage img; if(!readPng(m_assets.resolve("block/"+k+".png"),img)||!img.width||!img.height){ if(!readPng(p,img)||!img.width||!img.height)continue; }
  const uint16_t id=(uint16_t)i++;
  const Tint*tint=findTint(k);
  if(m_ids.find(k)==m_ids.end())m_ids.emplace(k,id);
  const uint32_t ox=(id%cols)*16, oy=(id/cols)*16; uint8_t tilePx[16*16*4]; makeTile(img,0,tint,tilePx);   // animated strips: first frame here, the rest in loadAnimations()
  for(uint32_t y=0;y<16;y++)std::copy(tilePx+(size_t)y*64,tilePx+(size_t)y*64+64,&m_atlas.rgba[((size_t)(oy+y)*size+ox)*4]);
 }
 if(m_ids.empty())return false;
 if(m_ids.find("missing")==m_ids.end()&&i<maxTiles){   // built-in MISSING tile: magenta / black checkerboard
  const uint32_t id=i++,ox=(id%cols)*16,oy=(id/cols)*16; m_ids.emplace("missing",(uint16_t)id);
  for(uint32_t y=0;y<16;y++)for(uint32_t x=0;x<16;x++){ const bool m=((x/8)+(y/8))%2==0; uint8_t*d=&m_atlas.rgba[((size_t)(oy+y)*size+ox+x)*4]; d[0]=m?248:0;d[1]=0;d[2]=m?248:0;d[3]=255; }
 }
 writeCache(cacheFile,sig);
 loadAnimations();
 return true;
}
const uint8_t* TexturePack::tilePixels(uint16_t id)const{
 const uint32_t size=atlasSize(); return &m_atlas.rgba[((size_t)(id/kCols)*16*size+(size_t)(id%kCols)*16)*4];
}
bool TexturePack::has(const std::string&texture)const{return m_ids.find(texture)!=m_ids.end();}
uint16_t TexturePack::id(const std::string&texture)const{auto it=m_ids.find(texture);return it==m_ids.end()?0:it->second;}

void TexturePack::loadAnimations(){
 namespace fs=std::filesystem; m_anims.clear();
 for(auto&kv:m_ids){
  const std::string&k=kv.first; const fs::path meta=m_assets.resolve("block/"+k+".png.mcmeta"); std::error_code ec; if(!fs::exists(meta,ec))continue;
  int ft=1; bool interp=false; std::vector<std::pair<int,int>> fr; if(!readMcmeta(meta,ft,interp,fr))continue;
  AtlasImage img; if(!readPng(m_assets.resolve("block/"+k+".png"),img)||!img.width||img.height<img.width*2)continue;
  const uint32_t n=img.height/img.width; const Tint*tint=findTint(k);
  TileAnim a; a.tile=kv.second; a.interp=interp; a.frames.resize(n);
  for(uint32_t i=0;i<n;i++)makeTile(img,i,tint,a.frames[i].data());
  if(fr.empty())for(uint32_t i=0;i<n;i++)a.seq.push_back({(int)i,ft});
  else for(auto&f:fr)a.seq.push_back({(int)((uint32_t)std::max(0,f.first)%n),f.second>0?f.second:ft});
  for(auto&s:a.seq)a.total+=s.second; if(a.total<=0||a.seq.size()<2)continue;
  m_anims.push_back(std::move(a));
 }
}
void TexturePack::collectAnimationUpdates(double ticks,std::vector<TileUpdate>&out){
 for(TileAnim&a:m_anims){
  const double t=std::fmod(ticks,(double)a.total); int acc=0; size_t si=0; for(;si<a.seq.size();si++){ if(t<acc+a.seq[si].second)break; acc+=a.seq[si].second; } if(si>=a.seq.size())si=a.seq.size()-1;
  const int cur=a.seq[si].first;
  if(!a.interp){ if(a.lastKey==cur)continue; a.lastKey=cur; TileUpdate u; u.id=a.tile; std::copy(a.frames[cur].begin(),a.frames[cur].end(),u.rgba); out.push_back(u); }
  else{
   const int key=(int)std::floor(t); if(a.lastKey==key)continue; a.lastKey=key;      // blend towards the next frame, updated every tick
   const int nxt=a.seq[(si+1)%a.seq.size()].first; const float f=float(t-acc)/float(a.seq[si].second);
   TileUpdate u; u.id=a.tile; for(int i=0;i<1024;i++)u.rgba[i]=(uint8_t)std::lround(a.frames[cur][i]*(1.f-f)+a.frames[nxt][i]*f); out.push_back(u);
  }
 }
}
