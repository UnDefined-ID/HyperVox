#include "World.hpp"
#include "SteelClient.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <mutex>
#include "Perf.hpp"
#include <cstring>

// ------------------------------------------------------------------ noise
namespace {
inline uint32_t hash32(uint32_t x){x^=x>>16;x*=0x7feb352dU;x^=x>>15;x*=0x846ca68bU;x^=x>>16;return x;}
inline uint32_t hash2(int x,int z,uint32_t seed){return hash32(hash32(static_cast<uint32_t>(x)+seed*0x9E3779B1u)+static_cast<uint32_t>(z)*0x85EBCA6Bu);}
inline uint32_t hash3(int x,int y,int z,uint32_t seed){return hash32(hash2(x,z,seed)+static_cast<uint32_t>(y)*0xC2B2AE35u);}
inline float lattice(int x,int z,uint32_t seed){return float(hash2(x,z,seed)&0xFFFFFFu)/16777215.0f;}
inline float lattice3(int x,int y,int z,uint32_t seed){return float(hash3(x,y,z,seed)&0xFFFFFFu)/16777215.0f;}
inline float sm(float t){return t*t*(3.f-2.f*t);}
float vnoise(float x,float z,uint32_t seed){
 const int xi=(int)std::floor(x),zi=(int)std::floor(z); const float fx=x-float(xi),fz=z-float(zi);
 const float sx=sm(fx),sz=sm(fz);
 const float a=lattice(xi,zi,seed),b=lattice(xi+1,zi,seed),c=lattice(xi,zi+1,seed),d=lattice(xi+1,zi+1,seed);
 const float t=a+(b-a)*sx,u=c+(d-c)*sx; return t+(u-t)*sz;
}
float vnoise3(float x,float y,float z,uint32_t seed){
 const int xi=(int)std::floor(x),yi=(int)std::floor(y),zi=(int)std::floor(z); const float fx=sm(x-float(xi)),fy=sm(y-float(yi)),fz=sm(z-float(zi));
 auto L=[&](int dx,int dy,int dz){return lattice3(xi+dx,yi+dy,zi+dz,seed);};
 const float x00=L(0,0,0)+(L(1,0,0)-L(0,0,0))*fx,x10=L(0,1,0)+(L(1,1,0)-L(0,1,0))*fx,x01=L(0,0,1)+(L(1,0,1)-L(0,0,1))*fx,x11=L(0,1,1)+(L(1,1,1)-L(0,1,1))*fx;
 const float y0=x00+(x10-x00)*fy,y1=x01+(x11-x01)*fy; return y0+(y1-y0)*fz;
}
float fbm(float x,float z,uint32_t seed,int oct){
 float sum=0,amp=1,norm=0,f=1; for(int i=0;i<oct;i++){sum+=amp*vnoise(x*f,z*f,seed+uint32_t(i)*101u);norm+=amp;amp*=0.5f;f*=2.f;} return sum/norm;
}

}

// ------------------------------------------------------------------ packed chunk storage
namespace{
void encodeSection(const uint16_t*v,Chunk::Section&s){
 s=Chunk::Section{};
 const uint16_t first=v[0]; size_t i=1; while(i<4096&&v[i]==first)i++;
 if(i==4096){ s.single=first; return; }
 static thread_local std::vector<uint16_t> lut; if(lut.empty())lut.assign(65536,0);
 uint16_t pal[256]; int np=0; bool over=false;
 for(size_t k=0;k<4096;k++){ const uint16_t b=v[k]; if(!lut[b]){ if(np==256){over=true;break;} pal[np]=b; lut[b]=uint16_t(++np); } }
 if(over){ for(int k=0;k<np;k++)lut[pal[k]]=0; s.bits=16; s.data.resize(8192); std::memcpy(s.data.data(),v,8192); return; }
 s.palette.assign(pal,pal+np);
 if(np<=16){ s.bits=4; s.data.assign(2048,uint8_t(0)); for(size_t k=0;k<4096;k++)s.data[k>>1]|=uint8_t((lut[v[k]]-1)<<((k&1)*4)); }
 else { s.bits=8; s.data.resize(4096); for(size_t k=0;k<4096;k++)s.data[k]=uint8_t(lut[v[k]]-1); }
 for(int k=0;k<np;k++)lut[pal[k]]=0;
 s.palette.shrink_to_fit(); s.data.shrink_to_fit();
}
void decodeSection(const Chunk::Section&s,uint16_t*out){
 switch(s.bits){
  case 0: std::fill(out,out+4096,s.single); break;
  case 4: for(int k=0;k<2048;k++){ const uint8_t d=s.data[size_t(k)]; out[2*k]=s.palette[d&15]; out[2*k+1]=s.palette[d>>4]; } break;
  case 8: for(int k=0;k<4096;k++)out[k]=s.palette[s.data[size_t(k)]]; break;
  default: std::memcpy(out,s.data.data(),8192); break;
 }
}
}
uint16_t Chunk::at(int x,int y,int z)const{
 if(secs.empty()){ const size_t i=(static_cast<size_t>(y)*SIZE+z)*SIZE+x; return i<blocks.size()?blocks[i]:uint16_t(0); }
 if(static_cast<unsigned>(y)>=static_cast<unsigned>(HEIGHT))return 0;
 const Section&s=secs[static_cast<size_t>(y>>4)]; const int i=((y&15)*16+z)*16+x;
 switch(s.bits){
  case 0: return s.single;
  case 4: return s.palette[(s.data[static_cast<size_t>(i>>1)]>>((i&1)*4))&15];
  case 8: return s.palette[s.data[static_cast<size_t>(i)]];
  default: { uint16_t v; std::memcpy(&v,&s.data[static_cast<size_t>(i)*2],2); return v; }
 }
}
void Chunk::pack(){
 if(!secs.empty())return;
 secs.assign(SECTIONS,Section{}); uint16_t tmp[4096];
 for(int s=0;s<SECTIONS;s++){
  const size_t base=static_cast<size_t>(s)*4096; if(base>=blocks.size())continue;                    // above the highest layer: empty
  const size_t n=std::min<size_t>(4096,blocks.size()-base); std::memcpy(tmp,blocks.data()+base,n*2); if(n<4096)std::memset(tmp+n,0,(4096-n)*2);
  encodeSection(tmp,secs[static_cast<size_t>(s)]);
 }
 std::vector<uint16_t>().swap(blocks);
}
void Chunk::put(int x,int y,int z,uint16_t id){
 if(secs.empty()){ at(x,y,z)=id; return; }
 Section&s=secs[static_cast<size_t>(y>>4)]; uint16_t tmp[4096]; decodeSection(s,tmp); tmp[((y&15)*16+z)*16+x]=id; encodeSection(tmp,s);
}
void Chunk::unpack(uint16_t*dst,int layers)const{
 const int ns=(layers+15)>>4;
 if(secs.empty()){ for(int s=0;s<ns;s++){ const size_t base=static_cast<size_t>(s)*4096,n=blocks.size()>base?std::min<size_t>(4096,blocks.size()-base):0; if(n)std::memcpy(dst+base,blocks.data()+base,n*2); if(n<4096)std::memset(dst+base+n,0,(4096-n)*2); } return; }
 for(int s=0;s<ns;s++)decodeSection(secs[static_cast<size_t>(s)],dst+static_cast<size_t>(s)*4096);
}
size_t Chunk::bytes()const{ size_t n=blocks.capacity()*2+sizeof(Chunk); for(auto&s:secs)n+=sizeof(Section)+s.palette.capacity()*2+s.data.capacity(); return n; }

World::World(){}
std::vector<std::array<int,3>> World::takeChanges(){ std::lock_guard<std::mutex> lg(m_lootMu); std::vector<std::array<int,3>> r; r.swap(m_changes); return r; }
std::vector<std::array<int,3>> World::takeNewLootChests(){ std::lock_guard<std::mutex> lg(m_lootMu); std::vector<std::array<int,3>> r; r.swap(m_newLoot); return r; }
void World::reset(uint64_t seed){std::unique_lock<std::shared_mutex> l(m_mu);m_chunks.clear();m_dirty.clear();m_generated.clear();m_loader=nullptr;m_seed64=seed;m_seed=uint32_t(seed)^(uint32_t(seed>>32)*2654435761u); }
void World::setLoader(Loader l){std::unique_lock<std::shared_mutex> g(m_mu);m_loader=std::move(l);}

// y of the first non-air block from the top (trees and water included); the chunk is generated on demand
int World::terrainHeight(int wx,int wz)const{
 const int cx=wx>>4,cz=wz>>4; const auto c=acquire(cx,cz,true); const int lx=wx&15,lz=wz&15; for(int y=std::min(c->maxY,Chunk::HEIGHT-1);y>=0;y--)if(c->at(lx,y,lz))return y; return 0;
}

// Debug world: one of EVERY block variant (all directions / halves / open states / hinges ...) in a grid, then galleries of the neighbour-dependent shapes:
// every fence / wall / pane / bars type with all 16 connection patterns, and stairs in all 40 facing / half / corner-shape combinations.
namespace{
struct DebugPlan{ std::unordered_map<uint64_t,std::vector<std::array<int,4>>> byChunk; };
const DebugPlan& debugPlan(){
 static const DebugPlan plan=[]{
  DebugPlan p; const Registry&R=Registry::get(); const int y=Chunk::Y_OFFSET+70;
  auto put=[&](int x,int yy,int z,uint16_t id){ if(x<0||z<0||!id)return; p.byChunk[(uint64_t(uint32_t(x>>4))<<32)|uint32_t(z>>4)].push_back({x,yy,z,(int)id}); };
  std::vector<uint16_t> ids; for(uint16_t i=1;i<R.blockCount();i++){ if(i==R.missing||R.doorBottomOf(i))continue; ids.push_back(i); }   // upper door halves come with their lower half
  const int gw=32; int n=0; for(uint16_t id:ids){ const int x=(n%gw)*2,z=(n/gw)*2; put(x,y,z,id); if(const BlockId top=R.doorTopFor(id))put(x,y+1,z,top); n++; }
  int z0=((n+gw-1)/gw)*2+8;
  // connecting shapes: neighbours are cobblestone cubes (fences / walls / panes / bars connect to those)
  for(uint16_t id=1;id<R.blockCount();id++){ const BlockInfo&bi=R.info(id); if(R.baseOf(id)!=id||bi.shape<Registry::SH_FENCE||bi.shape>Registry::SH_PANE)continue;
   for(int mask=0;mask<16;mask++){ const int cx=2+mask*4,cz=z0+2; put(cx,y,cz,id); if(mask&1)put(cx,y,cz-1,R.cobble); if(mask&2)put(cx+1,y,cz,R.cobble); if(mask&4)put(cx,y,cz+1,R.cobble); if(mask&8)put(cx-1,y,cz,R.cobble); }
   z0+=5; }
  // stairs: facing x half x shape (straight, outer left / right, inner left / right)
  for(const char*sn:{"oak_stairs","stone_stairs","cobblestone_stairs"}){ const auto it=R.blockIdOrZero(sn); if(!it)continue; z0+=3;
   for(int half=0;half<2;half++)for(int f=0;f<4;f++)for(int shape=0;shape<5;shape++){
    static const int dx[4]={0,0,1,-1},dz[4]={-1,1,0,0};                       // facing 0..3 = north south east west
    auto ccw=[&](int d){ static const int m[4]={3,2,0,1}; return m[d]; };    // north->west, south->east, east->north, west->south
    auto cwd=[&](int d){ static const int m[4]={2,3,1,0}; return m[d]; };
    const int cx=3+(f*5+shape)*5,cz=z0+half*6; put(cx,y,cz,R.variantOf(it,half*4+f));
    if(shape==0)continue; const bool outer=shape<=2,left=(shape&1)==1; const int nd=left?ccw(f):cwd(f); const int sx=outer?dx[f]:-dx[f],sz=outer?dz[f]:-dz[f];
    put(cx+sx,y,cz+sz,R.variantOf(it,half*4+nd)); }
   z0+=12; }
  return p; }();
 return plan; }
}
void World::generateDebug(int cx,int cz,Chunk&c)const{
 const auto&p=debugPlan(); const auto it=p.byChunk.find((uint64_t(uint32_t(cx))<<32)|uint32_t(cz)); if(it==p.byChunk.end())return;
 for(const auto&e:it->second){ c.at(e[0]&15,e[1],e[2]&15)=(uint16_t)e[3]; c.maxY=std::max(c.maxY,e[1]); }
}
void World::generate(int cx,int cz,Chunk&c)const{
 if(m_debug){generateDebug(cx,cz,c);return;}
 PerfScope ps("terrain generation of one chunk",m_steel?1500:60);   // Steel chunks normally take 60-120 ms: only warn when something is really stuck
 if(m_steel&&steel::generate(m_seed64,cx,cz,m_dim,c))return;   // vanilla-exact terrain from the SteelMC helper
 // fallback when the helper is not set up (run build-steel.sh): a flat world
 if(m_dim!=0){   // nether / end without the helper: a plain floor so the game stays playable
  const Registry&R2=Registry::get(); const uint16_t floorId=R2.blockByName(m_dim==1?"netherrack":"end_stone"),bed=R2.bedrock?R2.bedrock:floorId; const int y0=Chunk::Y_OFFSET;
  for(int lz=0;lz<16;lz++)for(int lx=0;lx<16;lx++){ const int wx=cx*16+lx,wz=cz*16+lz; if(m_dim==2&&wx*wx+wz*wz>60*60)continue; c.at(lx,y0,lz)=bed; for(int y=y0+1;y<y0+(m_dim==1?64:60);y++)c.at(lx,y,lz)=floorId; if(m_dim==1)c.at(lx,y0+127,lz)=bed; }
  c.maxY=y0+(m_dim==1?127:60); return; }
 const Registry&R=Registry::get(); const uint16_t stone=R.stone,dirt=R.dirt,grass=R.grass,bedrock=R.bedrock?R.bedrock:R.stone; const int top=SEA_LEVEL+1;
 for(int lz=0;lz<16;lz++)for(int lx=0;lx<16;lx++){ c.at(lx,0,lz)=bedrock; for(int y=1;y<top-3;y++)c.at(lx,y,lz)=stone; for(int y=top-3;y<top;y++)c.at(lx,y,lz)=dirt; c.at(lx,top,lz)=grass; }
 c.maxY=top;
}

std::shared_ptr<const Chunk> World::acquire(int cx,int cz,bool cache)const{
 const uint64_t k=key(cx,cz); Loader loader;
 { std::shared_lock<std::shared_mutex> l(m_mu);
   auto it=m_chunks.find(k); if(it!=m_chunks.end())return it->second;
   auto d=m_dirty.find(k); if(d!=m_dirty.end()){ if(!cache)return d->second; auto dc=d->second; l.unlock(); std::unique_lock<std::shared_mutex> u(m_mu); auto r=m_chunks.emplace(k,dc); return r.first->second; }
   loader=m_loader; }
 auto c=std::make_shared<Chunk>();
 bool loaded=false; if(loader){ PerfScope ps("loading one chunk from the save folder",40); loaded=loader(cx,cz,*c); }
 if(!loaded){ *c=Chunk(); generate(cx,cz,*c); }
 c->pack();   // palette-compressed from here on (immutable)
 if(cache){ std::unique_lock<std::shared_mutex> l(m_mu); auto r=m_chunks.emplace(k,std::shared_ptr<const Chunk>(c)); if(r.second&&!loaded&&!m_debug){ m_generated.emplace_back(k,r.first->second);
   // note the chests the terrain generator placed (structure loot is only ever rolled for these, once)
   static const std::vector<uint8_t> lootIds=[]{ const Registry&R=Registry::get(); std::vector<uint8_t> v(R.blockCount()+1,0); for(size_t i=1;i<v.size();i++){ const std::string&n=R.blockName(R.baseOf(static_cast<BlockId>(i))); if(n=="chest"||n=="trapped_chest"||n=="barrel")v[i]=1; } return v; }();
   std::vector<std::array<int,3>> found; const Chunk&ch=*r.first->second; const int H=c->maxY+1;
   if(H>0){ static thread_local std::vector<uint16_t> dn; dn.resize(static_cast<size_t>(((H+15)>>4)<<4)*256); ch.unpack(dn.data(),H);
    for(int y=0;y<H;y++)for(int z=0;z<16;z++)for(int x=0;x<16;x++){ const uint16_t id=dn[(static_cast<size_t>(y)*16+static_cast<size_t>(z))*16+static_cast<size_t>(x)]; if(id&&id<lootIds.size()&&lootIds[id])found.push_back({cx*16+x,y,cz*16+z}); } }
   if(!found.empty()){ std::lock_guard<std::mutex> lg(m_lootMu); m_newLoot.insert(m_newLoot.end(),found.begin(),found.end()); } }
  return r.first->second; }
 return c;
}

uint16_t World::get(int x,int y,int z)const{
 if(y<0||y>=Chunk::HEIGHT)return 0;
 const int cx=x>>4,cz=z>>4,lx=x&15,lz=z&15;
 { std::shared_lock<std::shared_mutex> l(m_mu); auto it=m_chunks.find(key(cx,cz)); if(it!=m_chunks.end())return it->second->at(lx,y,lz); }
 return acquire(cx,cz,true)->at(lx,y,lz);
}

int World::set(int x,int y,int z,uint16_t id,int out[3][2]){
 { std::lock_guard<std::mutex> lg(m_lootMu); if(m_changes.size()<200000)m_changes.push_back({x,y,z}); }
 if(y<0||y>=Chunk::HEIGHT)return 0;
 const int cx=x>>4,cz=z>>4,lx=x&15,lz=z&15; const uint64_t k=key(cx,cz);
 auto cur=acquire(cx,cz,true);
 { std::unique_lock<std::shared_mutex> l(m_mu);
   auto it=m_chunks.find(k); if(it==m_chunks.end())it=m_chunks.emplace(k,cur).first;   // never lose an edit because the chunk was pruned meanwhile
   auto nc=std::make_shared<Chunk>(*it->second); nc->put(lx,y,lz,id); if(id&&y>nc->maxY)nc->maxY=y;
   it->second=std::shared_ptr<const Chunk>(nc); m_dirty[k]=it->second; }
 int n=0; out[n][0]=cx;out[n][1]=cz;n++;
 if(lx==0){out[n][0]=cx-1;out[n][1]=cz;n++;} else if(lx==15){out[n][0]=cx+1;out[n][1]=cz;n++;}
 if(lz==0){out[n][0]=cx;out[n][1]=cz-1;n++;} else if(lz==15){out[n][0]=cx;out[n][1]=cz+1;n++;}
 return n;
}

ChunkSnapshot World::snapshot(int cx,int cz)const{
 return ChunkSnapshot{acquire(cx,cz,true),acquire(cx-1,cz,true),acquire(cx+1,cz,true),acquire(cx,cz-1,true),acquire(cx,cz+1,true)};
}

void World::pruneCache(int cx,int cz,int radius,const std::function<bool(int,int)>&keep){
 std::unique_lock<std::shared_mutex> l(m_mu);
 for(auto it=m_chunks.begin();it!=m_chunks.end();){
  const int ccx=(int)(int32_t)(it->first>>32),ccz=(int)(int32_t)(it->first&0xffffffffu);
  const int d=std::max(std::abs(ccx-cx),std::abs(ccz-cz)); if(d>radius||(keep&&d>6&&!keep(ccx,ccz)))it=m_chunks.erase(it); else ++it;
 }
}
World::DirtyList World::dirtyChunks()const{ std::shared_lock<std::shared_mutex> l(m_mu); DirtyList r; r.reserve(m_dirty.size()); for(auto&kv:m_dirty)r.emplace_back(kv.first,kv.second); return r; }
void World::markSaved(const DirtyList&saved){ std::unique_lock<std::shared_mutex> l(m_mu); for(auto&s:saved){ auto it=m_dirty.find(s.first); if(it!=m_dirty.end()&&it->second==s.second)m_dirty.erase(it); } }
World::DirtyList World::takeGenerated(){ std::unique_lock<std::shared_mutex> l(m_mu); DirtyList r; r.reserve(m_generated.size()); for(auto&g:m_generated)if(!m_dirty.count(g.first))r.push_back(g); m_generated.clear(); return r; }   // chunks that were edited go out through the dirty list instead
size_t World::generatedCount()const{std::shared_lock<std::shared_mutex> l(m_mu);return m_generated.size();}
size_t World::dirtyCount()const{std::shared_lock<std::shared_mutex> l(m_mu);return m_dirty.size();}

Vec3 World::findSpawn()const{
 if(m_debug)return Vec3{0.5f,float(Chunk::Y_OFFSET+74),-4.5f};
 const Registry&R=Registry::get();
 for(int r=0;r<=960;r+=16){
  const int steps=r==0?1:std::max(8,r/4);
  for(int i=0;i<steps;i++){
   const float a=6.2831853f*float(i)/float(steps); const int x=(int)std::lround(std::cos(a)*float(r)),z=(int)std::lround(std::sin(a)*float(r));
   const int y=terrainHeight(x,z); if(y>=SEA_LEVEL+1&&!R.info(get(x,y,z)).fluid)return Vec3{float(x)+0.5f,float(y)+1.01f,float(z)+0.5f};   // dry land
  }
 }
 return Vec3{0.5f,float(SEA_LEVEL)+2.f,0.5f};
}

namespace{
struct MeshOut{ std::vector<Vertex> b[kBuckets]; };
// One quad = 4 vertices, written in the order that makes the shared quad index pattern (0,1,2, 0,2,3) face outwards.
void addQuad(MeshOut&o,int bucket,int face,float x,float y,float z,float w,float h,uint32_t tex,float vOff=0.f,int turns=0,float uOff=0.f){
 static const int ua[6]={0,0,2,2,0,0}, va[6]={2,2,1,1,1,1};
 static const float nrm[6][3]={{0,-1,0},{0,1,0},{-1,0,0},{1,0,0},{0,0,-1},{0,0,1}};
 const float du[4]={0,w,w,0}, dv[4]={0,0,h,h};
 float P[4][3];
 for(int i=0;i<4;i++){P[i][0]=x;P[i][1]=y;P[i][2]=z;P[i][ua[face]]+=du[i];P[i][va[face]]+=dv[i];}
 float e1[3],e2[3];for(int k=0;k<3;k++){e1[k]=P[1][k]-P[0][k];e2[k]=P[2][k]-P[0][k];}
 float cx=e1[1]*e2[2]-e1[2]*e2[1],cy=e1[2]*e2[0]-e1[0]*e2[2],cz=e1[0]*e2[1]-e1[1]*e2[0];
 const bool ccw=(cx*nrm[face][0]+cy*nrm[face][1]+cz*nrm[face][2])>0;
 auto q=[](float v){return (uint32_t)std::lround(v*16.f);};
 Vertex vt[4];
 for(int i=0;i<4;i++){
  float u=du[i], v=dv[i];
  if(face>=2)v=h-dv[i];
  if(face==3||face==4)u=w-du[i];
  v+=vOff; u+=uOff;
  { float we=w; for(int tq=0;tq<turns;tq++){ const float tu=u; u=v; v=we-tu; we=(tq%2==0)?h:w; } }       // texture turned n x 90 degrees (sideways logs, glazed terracotta)
  const uint32_t qy=q(P[i][1]);
  vt[i].a=q(P[i][0])|(q(P[i][2])<<9)|((qy&4095u)<<18)|((uint32_t)(face&3)<<30);
  vt[i].b=q(u)|(q(v)<<9)|(tex<<18)|((uint32_t)(face>>2)<<30)|((qy>>12)<<31);
 }
 auto&ob=o.b[bucket];
 if(ccw)ob.insert(ob.end(),{vt[0],vt[1],vt[2],vt[3]}); else ob.insert(ob.end(),{vt[0],vt[3],vt[2],vt[1]});
}
const std::vector<uint8_t>& wireTable(){   // blocks redstone dust connects to
 static const std::vector<uint8_t> t=[]{ const Registry&R=Registry::get(); std::vector<uint8_t> v(R.blockCount()+1,0);
  for(size_t i=1;i<v.size();i++){ const std::string&n=R.blockName(R.baseOf(static_cast<BlockId>(i)));
   auto ends=[&](const char*e){ const size_t l=std::strlen(e); return n.size()>=l&&n.compare(n.size()-l,l,e)==0; };
   if(n=="redstone_wire"||n=="repeater"||n=="comparator"||n=="observer"||n=="redstone_block"||n=="redstone_torch"||n=="lever"||n=="target"||n=="daylight_detector"||n=="tripwire_hook"||ends("_button")||ends("_pressure_plate"))v[i]=1; }
  return v; }();
 return t;
}
constexpr uint8_t F_CUBE=1,F_TRANSP=2,F_CULLSAME=4,F_TRANSLUCENT=8;
// ---- detailed models (Registry::initModels): per-face texture rectangles, flat planes, tilted parts
void addFaceUV(MeshOut&o,int bucket,int face,float ox,float oy,float oz,float wpx,float hpx,const MFace&mf,bool both){
 static const int ua[6]={0,0,2,2,0,0}, va[6]={2,2,1,1,1,1};
 static const float nrm[6][3]={{0,-1,0},{0,1,0},{-1,0,0},{1,0,0},{0,0,-1},{0,0,1}};
 const float wb=wpx/16.f,hb=hpx/16.f; const float du[4]={0,wb,wb,0},dv[4]={0,0,hb,hb};
 float P[4][3]; for(int i=0;i<4;i++){P[i][0]=ox;P[i][1]=oy;P[i][2]=oz;P[i][ua[face]]+=du[i];P[i][va[face]]+=dv[i];}
 float e1[3],e2[3]; for(int k=0;k<3;k++){e1[k]=P[1][k]-P[0][k];e2[k]=P[2][k]-P[0][k];}
 const float cx=e1[1]*e2[2]-e1[2]*e2[1],cy=e1[2]*e2[0]-e1[0]*e2[2],cz=e1[0]*e2[1]-e1[1]*e2[0];
 const bool ccw=(cx*nrm[face][0]+cy*nrm[face][1]+cz*nrm[face][2])>0;
 Vertex vt[4];
 for(int i=0;i<4;i++){
  float u,v; faceUV(face,wpx,hpx,du[i]*16.f,dv[i]*16.f,mf,u,v);
  const uint32_t qx=(uint32_t)std::max(0l,std::lround(P[i][0]*16.f)),qy=(uint32_t)std::max(0l,std::lround(P[i][1]*16.f)),qz=(uint32_t)std::max(0l,std::lround(P[i][2]*16.f));
  const uint32_t qu=(uint32_t)std::clamp(std::lround(u),0l,511l),qv=(uint32_t)std::clamp(std::lround(v),0l,511l);
  vt[i].a=(qx&511u)|((qz&511u)<<9)|((qy&4095u)<<18)|((uint32_t)(face&3)<<30);
  vt[i].b=qu|(qv<<9)|((uint32_t)mf.tex<<18)|((uint32_t)(face>>2)<<30)|((qy>>12)<<31);
 }
 auto&ob=o.b[bucket];
 if(ccw)ob.insert(ob.end(),{vt[0],vt[1],vt[2],vt[3]}); else ob.insert(ob.end(),{vt[0],vt[3],vt[2],vt[1]});
 if(both){ if(ccw)ob.insert(ob.end(),{vt[0],vt[3],vt[2],vt[1]}); else ob.insert(ob.end(),{vt[0],vt[1],vt[2],vt[3]}); }
}
template<class NB,class FLG> void emitModel(MeshOut&o,const Model&m,int x,int y,int z,uint16_t id,NB&&nbf,FLG&&flagsOf){
 for(const MBox&b:m.boxes){
  for(int f=0;f<6;f++){
   const MFace&mf=b.f[f]; if(mf.tex<0)continue;
   if(b.cullBorder){   // a face lying on the block border is hidden by an opaque neighbour (or by the same translucent block)
    bool border=false; int dx=0,dy=0,dz=0;
    switch(f){ case 0: border=b.y0<=0.001f; dy=-1; break; case 1: border=b.y1>=15.999f; dy=1; break; case 2: border=b.x0<=0.001f; dx=-1; break; case 3: border=b.x1>=15.999f; dx=1; break; case 4: border=b.z0<=0.001f; dz=-1; break; default: border=b.z1>=15.999f; dz=1; break; }
    if(border){ const uint16_t n=nbf(dx,dy,dz); if(n){ const uint8_t nf=flagsOf(n); if(!(nf&F_TRANSP)||(b.trans&&(nf&F_CULLSAME)&&n==id))continue; } }
   }
   float ox=x+b.x0/16.f,oy=y+b.y0/16.f,oz=z+b.z0/16.f,w=0,h=0;
   switch(f){ case 0: w=b.x1-b.x0; h=b.z1-b.z0; break; case 1: oy=y+b.y1/16.f; w=b.x1-b.x0; h=b.z1-b.z0; break; case 2: w=b.z1-b.z0; h=b.y1-b.y0; break;
    case 3: ox=x+b.x1/16.f; w=b.z1-b.z0; h=b.y1-b.y0; break; case 4: w=b.x1-b.x0; h=b.y1-b.y0; break; default: oz=z+b.z1/16.f; w=b.x1-b.x0; h=b.y1-b.y0; break; }
   if(w<=0.001f||h<=0.001f)continue;
   addFaceUV(o,b.trans?7:(b.twoSided?0:kFaceBucket[f]),f,ox,oy,oz,w,h,mf,b.twoSided);
  }
 }
 for(const MQuad&q:m.quads){
  float e1[3],e2[3]; for(int k=0;k<3;k++){e1[k]=q.p[1][k]-q.p[0][k];e2[k]=q.p[2][k]-q.p[0][k];}
  const float n[3]={e1[1]*e2[2]-e1[2]*e2[1],e1[2]*e2[0]-e1[0]*e2[2],e1[0]*e2[1]-e1[1]*e2[0]};
  const float ax=std::fabs(n[0]),ay=std::fabs(n[1]),az=std::fabs(n[2]);
  const int face=(ay>=ax&&ay>=az)?(n[1]>0?1:0):(ax>=az?(n[0]>0?3:2):(n[2]>0?5:4));
  Vertex vt[4];
  for(int i=0;i<4;i++){
   const uint32_t qx=(uint32_t)std::max(0l,std::lround(x*16.f+q.p[i][0])),qy=(uint32_t)std::max(0l,std::lround(y*16.f+q.p[i][1])),qz=(uint32_t)std::max(0l,std::lround(z*16.f+q.p[i][2]));
   const uint32_t qu=(uint32_t)std::clamp(std::lround(q.uv[i][0]),0l,511l),qv=(uint32_t)std::clamp(std::lround(q.uv[i][1]),0l,511l);
   vt[i].a=(qx&511u)|((qz&511u)<<9)|((qy&4095u)<<18)|((uint32_t)(face&3)<<30);
   vt[i].b=qu|(qv<<9)|((uint32_t)q.tex<<18)|((uint32_t)(face>>2)<<30)|((qy>>12)<<31);
  }
  auto&ob=o.b[0];   // free (tilted) quads are not axis aligned, so the per-direction culling cannot be trusted for them: always drawn
  ob.insert(ob.end(),{vt[0],vt[1],vt[2],vt[3]});
  if(q.twoSided)ob.insert(ob.end(),{vt[0],vt[3],vt[2],vt[1]});
 }
}

const std::vector<uint8_t>& blockFlags(){   // compact per-block flags so the mesher does not touch the big BlockInfo for every neighbour test
 static const std::vector<uint8_t> t=[]{
  const Registry&R=Registry::get(); std::vector<uint8_t> v(R.blockCount()+1);
  for(size_t i=0;i<v.size();i++){ const BlockInfo&bi=R.info(static_cast<BlockId>(i)); uint8_t f=0; if(!(bi.height16<16||bi.shape||bi.model))f|=F_CUBE; if(bi.transparent||bi.fluid)f|=F_TRANSP; if(bi.cullSame)f|=F_CULLSAME; if(bi.translucent)f|=F_TRANSLUCENT; v[i]=f; }
  return v; }();
 return t;
}
}

// One pass over the chunk: every cube face that is not hidden by an opaque neighbour goes into a per-face mask, the masks are greedy-merged into quads,
// and the quads are grouped by direction (see ChunkMesh) so the renderers can skip the faces that point away from the camera.
ChunkMesh World::buildMesh(const ChunkSnapshot&s,int cx,int cz){
 ChunkMesh res; if(!s.c)return res;
 const Chunk&c=*s.c; const int N=16,H=c.maxY+1; if(H<=0)return res;
 const Registry&R=Registry::get(); const std::vector<uint8_t>&FL=blockFlags(); const size_t nFl=FL.size();
 auto flagsOf=[&](uint16_t id)->uint8_t{ return id<nFl?FL[id]:FL[nFl-1]; };
 // padded copy of the chunk (1 block border from the 4 neighbours, 1 layer of air above and below) so neighbour tests are plain array reads
 const int Hs=((H+15)>>4)<<4; constexpr int PW=18; constexpr size_t layer=size_t(PW)*PW;
 static thread_local std::vector<uint16_t> pad,dense;
 pad.assign(static_cast<size_t>(Hs+2)*layer,uint16_t(0)); dense.resize(static_cast<size_t>(Hs)*256); c.unpack(dense.data(),H);
 auto P=[&](int x,int y,int z)->size_t{ return (static_cast<size_t>(y+1)*PW+static_cast<size_t>(z+1))*PW+static_cast<size_t>(x+1); };
 for(int y=0;y<Hs;y++)for(int z=0;z<N;z++)std::memcpy(&pad[P(0,y,z)],&dense[(static_cast<size_t>(y)*16+static_cast<size_t>(z))*16],32);
 if(s.xm)for(int y=0;y<H;y++)for(int z=0;z<N;z++)pad[P(-1,y,z)]=s.xm->at(15,y,z);
 if(s.xp)for(int y=0;y<H;y++)for(int z=0;z<N;z++)pad[P(16,y,z)]=s.xp->at(0,y,z);
 if(s.zm)for(int y=0;y<H;y++)for(int x=0;x<N;x++)pad[P(x,y,-1)]=s.zm->at(x,y,15);
 if(s.zp)for(int y=0;y<H;y++)for(int x=0;x<N;x++)pad[P(x,y,16)]=s.zp->at(x,y,0);
 auto nb=[&](int x,int y,int z)->uint16_t{ if(y<-1||y>Hs)return 0; return pad[P(x,y,z)]; };
 // mask value: (tile+1) | LOW<<29 (fluid surface, lowered by 2/16) | TRANS<<30 | quarter turns<<26
 constexpr uint32_t FLU=1u<<28,LOW=1u<<29,TRANS=1u<<30,TEXM=(1u<<14)-1u,TURNSH=26;
 static thread_local std::vector<uint32_t> mk[6]; static thread_local std::vector<uint16_t> cnt[6];
 const size_t total=static_cast<size_t>(H)*256;
 for(int f=0;f<6;f++){ mk[f].assign(total,0u); cnt[f].assign(static_cast<size_t>(f<2?H:N),uint16_t(0)); }
 static thread_local std::vector<std::array<int,3>> partial; partial.clear();   // blocks with a shape smaller than a cube
 for(int y=0;y<H;y++)for(int z=0;z<N;z++){
  const uint16_t*row=&pad[P(0,y,z)];
  for(int x=0;x<N;x++){
   const uint16_t id=row[x]; if(!id)continue;
   const uint8_t fl=flagsOf(id);
   if(!(fl&F_CUBE)){ partial.push_back({x,y,z}); continue; }
   const size_t pi=P(x,y,z);
   const uint16_t nbr[6]={pad[pi-layer],pad[pi+layer],pad[pi-1],pad[pi+1],pad[pi-PW],pad[pi+PW]};   // -Y +Y -X +X -Z +Z
   for(int face=0;face<6;face++){
    const uint16_t n=nbr[face]; bool vis=(n==0);
    const BlockInfo&bi=R.info(id);
    if(bi.fluid)vis=(n==0)||R.baseOf(n)!=R.baseOf(id);   // fluids are never culled by solid neighbours (only by their own kind) and never hide the blocks next to them
    else if(!vis){ const uint8_t nf=flagsOf(n); vis=(nf&F_TRANSP)&&!((nf&F_CULLSAME)&&n==id); }
    if(!vis)continue;
    uint32_t val=bi.texture[face]+1u; if(fl&F_TRANSLUCENT)val|=TRANS; val|=((uint32_t(bi.rotq)>>(face*2))&3u)<<TURNSH;
    if(bi.fluid){ val|=FLU; if(face!=0&&(nbr[1]==0||(R.baseOf(nbr[1])!=R.baseOf(id)&&(flagsOf(nbr[1])&F_TRANSP))))val|=LOW; }   // the surface is lowered only under open air - under a block it stays full height (no gap)
    size_t idx,dep;
    if(face<2){ dep=static_cast<size_t>(y); idx=dep*256+static_cast<size_t>(z)*16+static_cast<size_t>(x); }
    else if(face<4){ dep=static_cast<size_t>(x); idx=dep*(16*static_cast<size_t>(H))+static_cast<size_t>(y)*16+static_cast<size_t>(z); }
    else { dep=static_cast<size_t>(z); idx=dep*(16*static_cast<size_t>(H))+static_cast<size_t>(y)*16+static_cast<size_t>(x); }
    mk[face][idx]=val; cnt[face][dep]++;
   }
  }
 }
 MeshOut out;
 for(int face=0;face<6;face++){
  const int depth=face<2?H:N, U=N, V=face<2?N:H;
  for(int d=0;d<depth;d++){
   if(!cnt[face][static_cast<size_t>(d)])continue;
   uint32_t*mask=&mk[face][static_cast<size_t>(d)*static_cast<size_t>(U)*static_cast<size_t>(V)];
   for(int v=0;v<V;v++)for(int u=0;u<U;u++){
    const uint32_t val=mask[(size_t)v*U+u]; if(!val)continue;
    int w=1,h=1;
    // quads are at most 31 blocks long: the packed uv has 9 bits (1/16 units = 32 blocks), a longer quad wrapped around and came out stretched / with the wrong texture
    while(u+w<U&&w<31&&mask[(size_t)v*U+u+w]==val)w++;
    while(v+h<V&&h<31){bool ok=true;for(int k=0;k<w;k++)if(mask[(size_t)(v+h)*U+u+k]!=val){ok=false;break;}if(!ok)break;h++;}
    for(int yy=0;yy<h;yy++)for(int xx=0;xx<w;xx++)mask[(size_t)(v+yy)*U+u+xx]=0;
    float x=0,y=0,z=0,a=0,b=0;
    if(face<2){x=float(u);y=float(d+(face==1?1:0));z=float(v);a=float(w);b=float(h);}
    else if(face<4){x=float(d+(face==3?1:0));y=float(v);z=float(u);a=float(w);b=float(h);}
    else{x=float(u);y=float(v);z=float(d+(face==5?1:0));a=float(w);b=float(h);}
    float vOff=0.f;
    if(val&LOW){ if(face==1)y-=2.f/16.f; else if(face>=2){ b-=2.f/16.f; vOff=2.f/16.f; } }       // side quads of lowered cells are always 1 high
    addQuad(out,(val&TRANS)?7:kFaceBucket[face],face,x,y,z,a,b,(val&TEXM)-1u,vOff,int((val>>TURNSH)&3u));
    if((val&FLU)&&face==1){   // the fluid surface is also visible from below (swimming under water / lava)
     const auto&src=out.b[(val&TRANS)?7:kFaceBucket[1]]; const size_t m=src.size(); const Vertex q4[4]={src[m-4],src[m-3],src[m-2],src[m-1]};
     auto&dst=out.b[(val&TRANS)?7:kFaceBucket[0]]; dst.insert(dst.end(),{q4[0],q4[3],q4[2],q4[1]}); }
   }
  }
 }
 // partial blocks (enchanting table ...): a box from y to y+height, side textures cropped like the vanilla model
 for(auto&p:partial){
  const int x=p[0],y=p[1],z=p[2]; const uint16_t bid=pad[P(x,y,z)]; const BlockInfo&bi=R.info(bid);
  if(R.redstoneWire&&R.baseOf(bid)==R.redstoneWire){   // dust: dot / line / corner / T / cross depending on the neighbours (also climbing onto a block next to it)
   const std::vector<uint8_t>&wt=wireTable(); int mask=0; const int dd[4][2]={{0,-1},{1,0},{0,1},{-1,0}};
   for(int d=0;d<4;d++){ const int dx=dd[d][0],dz=dd[d][1]; const uint16_t nn=nb(x+dx,y,z+dz); bool c=nn&&nn<wt.size()&&wt[nn];
    if(!c){ const uint16_t up=nb(x+dx,y+1,z+dz); if(up&&R.baseOf(up)==R.redstoneWire&&!nb(x,y+1,z))c=true; }
    if(!c&&!nn){ const uint16_t dn=nb(x+dx,y-1,z+dz); if(dn&&R.baseOf(dn)==R.redstoneWire)c=true; }
    if(c)mask|=1<<d; }
   if(const Model*wm=R.wireModel(mask)){ emitModel(out,*wm,x,y,z,bid,[&](int dx,int dy,int dz){return nb(x+dx,y+dy,z+dz);},flagsOf); continue; } }
  if(bi.fluid){   // flowing water / lava (partial height): a box whose height follows the level; no faces towards the same liquid
   const float hh=float(bi.height16)/16.f; const BlockId fb=R.baseOf(bid);
   const uint16_t nbr6[6]={nb(x,y-1,z),nb(x,y+1,z),nb(x-1,y,z),nb(x+1,y,z),nb(x,y,z-1),nb(x,y,z+1)};
   for(int f=0;f<6;f++){ const uint16_t n=nbr6[f]; if(n){ const BlockInfo&ni=R.info(n); const bool sameFam=ni.fluid&&R.baseOf(n)==fb;
     if(sameFam){ if(f==0||f==1)continue; if(ni.height16>=bi.height16)continue; }
     else if(f!=1&&ni.solid&&!ni.transparent&&!ni.fluid&&ni.height16>=16&&!ni.shape)continue; }
    const int bucket=bi.translucent?7:kFaceBucket[f]; float ox=float(x),oy=float(y),oz=float(z),ww=1.f,hgt=1.f;
    switch(f){ case 0: break; case 1: oy+=hh; break; case 2: hgt=hh; break; case 3: ox+=1.f; hgt=hh; break; case 4: hgt=hh; break; default: oz+=1.f; hgt=hh; break; }
    addQuad(out,bucket,f,ox,oy,oz,ww,hgt,bi.texture[f]?bi.texture[f]:bi.texture[2]);
    if(f==1){ auto&src=out.b[bucket]; const size_t m=src.size(); const Vertex q4[4]={src[m-4],src[m-3],src[m-2],src[m-1]}; auto&dst=out.b[bi.translucent?7:kFaceBucket[0]]; dst.insert(dst.end(),{q4[0],q4[3],q4[2],q4[1]}); } }
   continue; }
  if(bi.model){ if(const Model*md=R.modelOf(bid)){ emitModel(out,*md,x,y,z,bid,[&](int dx,int dy,int dz){return nb(x+dx,y+dy,z+dz);},flagsOf); continue; } }
  if(bi.shape==Registry::SH_PLANT){   // crossed plant: two diagonal planes, each emitted twice (front + back) so the GPU can keep back-face culling on
   const uint32_t tex=bi.texture[2]; const float x0=float(x)+0.05f,x1=float(x)+0.95f,z0=float(z)+0.05f,z1=float(z)+0.95f,y0=float(y),y1=float(y)+1.f;
   auto q=[&](float v){return (uint32_t)std::lround(v*16.f);};
   for(int d=0;d<2;d++){
    const float ax=x0,az=d?z1:z0,bx=x1,bz=d?z0:z1;
    const float px[4]={ax,bx,bx,ax},pz[4]={az,bz,bz,az},py[4]={y0,y0,y1,y1},uu[4]={0,1,1,0},vv[4]={1,1,0,0};
    Vertex vt[4]; for(int i=0;i<4;i++){ const uint32_t qy=q(py[i]); vt[i].a=q(px[i])|(q(pz[i])<<9)|((qy&4095u)<<18)|(2u<<30); vt[i].b=q(uu[i])|(q(vv[i])<<9)|(tex<<18)|((qy>>12)<<31); }
    auto&ob=out.b[0]; ob.insert(ob.end(),{vt[0],vt[1],vt[2],vt[3]}); ob.insert(ob.end(),{vt[0],vt[3],vt[2],vt[1]});
   }
   continue;
  }
  if(bi.shape){   // boxes (slabs, stairs, carpets, torches, chests) or neighbour-dependent shapes (fences, walls, panes)
   Box dyn[9]; std::vector<Box> sta; const Box*bp; int bn;
   if(bi.shape>=Registry::SH_FENCE&&bi.shape<=Registry::SH_PANE){
    auto conn=[&](int dx,int dz){ const uint16_t n=nb(x+dx,y,z+dz); if(!n)return false; const BlockInfo&ni=R.info(n); return (ni.conn&&ni.conn==bi.conn)||(ni.shape==0&&ni.solid&&!ni.transparent&&!ni.fluid&&ni.height16>=16); };
    int cm=0; if(bi.shape==Registry::SH_WALL){ const uint16_t up=nb(x,y+1,z); if(up&&R.info(up).conn&&R.info(up).conn==bi.conn)cm=4; }
    bn=Registry::connectBoxes(bi.shape,conn(0,-1),conn(1,0),conn(0,1),conn(-1,0),dyn,cm); bp=dyn; }
   else if(const int sst=R.stairState(bid);sst>=0){   // stairs: corners follow the neighbouring stairs
    const int ns[4]={[&]{const uint16_t n=nb(x,y,z-1);return n?R.stairState(n):-1;}(),[&]{const uint16_t n=nb(x,y,z+1);return n?R.stairState(n):-1;}(),[&]{const uint16_t n=nb(x+1,y,z);return n?R.stairState(n):-1;}(),[&]{const uint16_t n=nb(x-1,y,z);return n?R.stairState(n):-1;}()};
    bn=Registry::stairBoxes(sst,ns,dyn); bp=dyn; }
   else { sta=R.boxes(bi.shape); bp=sta.data(); bn=(int)sta.size(); }
   for(int bk=0;bk<bn;bk++){ const Box&bx=bp[bk];
    const float fx0=bx.x0/16.f,fy0=bx.y0/16.f,fz0=bx.z0/16.f,fx1=bx.x1/16.f,fy1=bx.y1/16.f,fz1=bx.z1/16.f;
    for(int face=0;face<6;face++){
     // a face lying on the block border is hidden by a solid cube next to it
     const int nx=face==2?-1:face==3?1:0,ny=face==0?-1:face==1?1:0,nz=face==4?-1:face==5?1:0;
     const bool onBorder=(face==0&&bx.y0==0)||(face==1&&bx.y1==16)||(face==2&&bx.x0==0)||(face==3&&bx.x1==16)||(face==4&&bx.z0==0)||(face==5&&bx.z1==16);
     if(onBorder){ const uint16_t n=nb(x+nx,y+ny,z+nz); if(n!=0&&!R.info(n).transparent)continue; }
     const uint32_t tex=bi.texture[face];
     switch(face){
      case 0: addQuad(out,bi.translucent?7:kFaceBucket[0],0,float(x)+fx0,float(y)+fy0,float(z)+fz0,fx1-fx0,fz1-fz0,tex,fz0,false,fx0); break;
      case 1: addQuad(out,bi.translucent?7:kFaceBucket[1],1,float(x)+fx0,float(y)+fy1,float(z)+fz0,fx1-fx0,fz1-fz0,tex,fz0,false,fx0); break;
      case 2: addQuad(out,bi.translucent?7:kFaceBucket[2],2,float(x)+fx0,float(y)+fy0,float(z)+fz0,fz1-fz0,fy1-fy0,tex,1.f-fy1,false,fz0); break;
      case 3: addQuad(out,bi.translucent?7:kFaceBucket[3],3,float(x)+fx1,float(y)+fy0,float(z)+fz0,fz1-fz0,fy1-fy0,tex,1.f-fy1,false,1.f-fz1); break;
      case 4: addQuad(out,bi.translucent?7:kFaceBucket[4],4,float(x)+fx0,float(y)+fy0,float(z)+fz0,fx1-fx0,fy1-fy0,tex,1.f-fy1,false,1.f-fx1); break;
      default: addQuad(out,bi.translucent?7:kFaceBucket[5],5,float(x)+fx0,float(y)+fy0,float(z)+fz1,fx1-fx0,fy1-fy0,tex,1.f-fy1,false,fx0); break;
     }
    }
   }
   continue;
  }
  const float hh=float(bi.height16)/16.f;   // enchanting table: one box, 12/16 high
  for(int face=0;face<6;face++){
   const int nx=face==2?-1:face==3?1:0,ny=face==0?-1:face==1?1:0,nz=face==4?-1:face==5?1:0;
   if(face>=2){ const uint16_t n=nb(x+nx,y,z+nz); const BlockInfo&ni=R.info(n); if(n!=0&&!ni.transparent)continue; }
   else if(face==0){ const uint16_t n=nb(x,y-1,z); if(n!=0&&!R.info(n).transparent)continue; }
   const uint32_t tex=bi.texture[face];
   if(face==1)addQuad(out,kFaceBucket[1],1,float(x),float(y)+hh,float(z),1.f,1.f,tex);
   else if(face==0)addQuad(out,kFaceBucket[0],0,float(x),float(y),float(z),1.f,1.f,tex);
   else{ float fx=float(x),fz=float(z); if(face==3)fx+=1.f; if(face==5)fz+=1.f; addQuad(out,kFaceBucket[face],face,fx,float(y),fz,1.f,hh,tex,1.f-hh); }
  }
 }
 (void)cx;(void)cz;
 res.maxY=H; size_t n=0; for(int b=0;b<kBuckets;b++)n+=out.b[b].size();
 res.vertices.reserve(n); for(int b=0;b<kBuckets;b++){ res.vertices.insert(res.vertices.end(),out.b[b].begin(),out.b[b].end()); res.bucketEnd[b]=static_cast<uint32_t>(res.vertices.size()); }
 return res;
}
