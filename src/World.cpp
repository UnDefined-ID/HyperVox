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

World::World(){}
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
 if(m_steel&&steel::generate(m_seed64,cx,cz,c))return;   // vanilla-exact terrain from the SteelMC helper
 // fallback when the helper is not set up (run build-steel.sh): a flat world
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
 c->blocks.shrink_to_fit();
 if(cache){ std::unique_lock<std::shared_mutex> l(m_mu); auto r=m_chunks.emplace(k,std::shared_ptr<const Chunk>(c)); if(r.second&&!loaded&&!m_debug)m_generated.emplace_back(k,r.first->second); return r.first->second; }
 return c;
}

uint16_t World::get(int x,int y,int z)const{
 if(y<0||y>=Chunk::HEIGHT)return 0;
 const int cx=x>>4,cz=z>>4,lx=x&15,lz=z&15;
 { std::shared_lock<std::shared_mutex> l(m_mu); auto it=m_chunks.find(key(cx,cz)); if(it!=m_chunks.end())return it->second->at(lx,y,lz); }
 return acquire(cx,cz,true)->at(lx,y,lz);
}

int World::set(int x,int y,int z,uint16_t id,int out[3][2]){
 if(y<0||y>=Chunk::HEIGHT)return 0;
 const int cx=x>>4,cz=z>>4,lx=x&15,lz=z&15; const uint64_t k=key(cx,cz);
 auto cur=acquire(cx,cz,true);
 { std::unique_lock<std::shared_mutex> l(m_mu);
   auto it=m_chunks.find(k); if(it==m_chunks.end())it=m_chunks.emplace(k,cur).first;   // never lose an edit because the chunk was pruned meanwhile
   auto nc=std::make_shared<Chunk>(*it->second); nc->at(lx,y,lz)=id; if(id&&y>nc->maxY)nc->maxY=y;
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

static void addQuad(ChunkMesh&o,bool trans,int face,float x,float y,float z,float w,float h,uint32_t tex,float vOff=0.f,int turns=0,float uOff=0.f){
 static const int ua[6]={0,0,2,2,0,0}, va[6]={2,2,1,1,1,1};
 static const float nrm[6][3]={{0,-1,0},{0,1,0},{-1,0,0},{1,0,0},{0,0,-1},{0,0,1}};
 const float du[4]={0,w,w,0}, dv[4]={0,0,h,h};
 float P[4][3];
 for(int i=0;i<4;i++){P[i][0]=x;P[i][1]=y;P[i][2]=z;P[i][ua[face]]+=du[i];P[i][va[face]]+=dv[i];}
 float e1[3],e2[3];for(int k=0;k<3;k++){e1[k]=P[1][k]-P[0][k];e2[k]=P[2][k]-P[0][k];}
 float cx=e1[1]*e2[2]-e1[2]*e2[1],cy=e1[2]*e2[0]-e1[0]*e2[2],cz=e1[0]*e2[1]-e1[1]*e2[0];
 bool ccw=(cx*nrm[face][0]+cy*nrm[face][1]+cz*nrm[face][2])>0;
 uint32_t base=(uint32_t)o.vertices.size();
 auto q=[](float v){return (uint32_t)std::lround(v*16.f);};
 for(int i=0;i<4;i++){
  float u=du[i], v=dv[i];
  if(face>=2)v=h-dv[i];
  if(face==3||face==4)u=w-du[i];
  v+=vOff; u+=uOff;
  { float we=w; for(int tq=0;tq<turns;tq++){ const float tu=u; u=v; v=we-tu; we=(tq%2==0)?h:w; } }       // texture turned n x 90 degrees (sideways logs, glazed terracotta)
  Vertex vt;
  const uint32_t qy=q(P[i][1]);
  vt.a=q(P[i][0])|(q(P[i][2])<<9)|((qy&4095u)<<18)|((uint32_t)(face&3)<<30);
  vt.b=q(u)|(q(v)<<9)|(tex<<18)|((uint32_t)(face>>2)<<30)|((qy>>12)<<31);
  o.vertices.push_back(vt);
 }
 auto&ix=trans?o.tindices:o.indices;
 if(ccw)ix.insert(ix.end(),{base,base+1,base+2,base,base+2,base+3});
 else ix.insert(ix.end(),{base,base+2,base+1,base,base+3,base+2});
}

ChunkMesh World::buildMesh(const ChunkSnapshot&s,int cx,int cz){
 ChunkMesh out; if(!s.c)return out;
 const Chunk&c=*s.c; const int N=16,H=c.maxY+1; if(H<=0)return out;
 const Registry&R=Registry::get();
 auto nb=[&](int x,int y,int z)->uint16_t{
  if(y<0||y>=Chunk::HEIGHT)return 0;
  if(x<0)return s.xm?s.xm->at(15,y,z):uint16_t(0);
  if(x>=N)return s.xp?s.xp->at(0,y,z):uint16_t(0);
  if(z<0)return s.zm?s.zm->at(x,y,15):uint16_t(0);
  if(z>=N)return s.zp?s.zp->at(x,y,0):uint16_t(0);
  return c.at(x,y,z);
 };
 // mask value: (tile+1) | LOW<<29 (fluid surface, lowered by 2/16) | TRANS<<30
 constexpr uint32_t LOW=1u<<29,TRANS=1u<<30,TEXM=(1u<<14)-1u,TURNSH=26;   // bits 26-27: quarter turns
 std::vector<uint32_t> mask((size_t)N*std::max(N,H));
 std::vector<std::array<int,3>> partial;                                    // blocks with a shape smaller than a cube
 for(int face=0;face<6;face++){
  const int depth=face<2?H:N, U=N, V=face<2?N:H;
  for(int d=0;d<depth;d++){
   std::fill(mask.begin(),mask.begin()+(size_t)U*V,0u);
   bool any=false;
   for(int v=0;v<V;v++)for(int u=0;u<U;u++){
    int x=0,y=0,z=0,nx=0,ny=0,nz=0;
    if(face<2){x=u;z=v;y=d;ny=(face==1?1:-1);}
    else if(face<4){z=u;y=v;x=d;nx=(face==3?1:-1);}
    else {x=u;y=v;z=d;nz=(face==5?1:-1);}
    const uint16_t id=c.at(x,y,z); if(!id)continue;
    const BlockInfo&bi=R.info(id);
    if(bi.height16<16||bi.shape){ if(face==0)partial.push_back({x,y,z}); continue; }    // partial shapes are emitted separately below
    const uint16_t n=nb(x+nx,y+ny,z+nz);
    bool vis=(n==0);
    if(!vis){const BlockInfo&ni=R.info(n); vis=ni.transparent&&!(ni.cullSame&&n==id);}
    if(vis){
     uint32_t val=bi.texture[face]+1u; if(bi.translucent)val|=TRANS; val|=((uint32_t(bi.rotq)>>(face*2))&3u)<<TURNSH;
     if(bi.fluid){ const uint16_t up=nb(x,y+1,z); if(up!=id&&(face==1||(face>=2&&up!=id)))val|=LOW; }   // top of a fluid column
     mask[(size_t)v*U+u]=val;any=true;
    }
   }
   if(!any)continue;
   for(int v=0;v<V;v++)for(int u=0;u<U;u++){
    const uint32_t val=mask[(size_t)v*U+u]; if(!val)continue;
    int w=1,h=1;
    while(u+w<U&&mask[(size_t)v*U+u+w]==val)w++;
    while(v+h<V){bool ok=true;for(int k=0;k<w;k++)if(mask[(size_t)(v+h)*U+u+k]!=val){ok=false;break;}if(!ok)break;h++;}
    for(int yy=0;yy<h;yy++)for(int xx=0;xx<w;xx++)mask[(size_t)(v+yy)*U+u+xx]=0;
    float x=0,y=0,z=0,a=0,b=0;
    if(face<2){x=float(u);y=float(d+(face==1?1:0));z=float(v);a=float(w);b=float(h);}
    else if(face<4){x=float(d+(face==3?1:0));y=float(v);z=float(u);a=float(w);b=float(h);}
    else{x=float(u);y=float(v);z=float(d+(face==5?1:0));a=float(w);b=float(h);}
    float vOff=0.f;
    if(val&LOW){ if(face==1)y-=2.f/16.f; else if(face>=2){ b-=2.f/16.f; vOff=2.f/16.f; } }       // side quads of lowered cells are always 1 high
    addQuad(out,(val&TRANS)!=0,face,x,y,z,a,b,(val&TEXM)-1u,vOff,int((val>>TURNSH)&3u));
   }
  }
 }
 // partial blocks (enchanting table ...): a box from y to y+height, side textures cropped like the vanilla model
 for(auto&p:partial){
  const int x=p[0],y=p[1],z=p[2]; const uint16_t bid=c.at(x,y,z); const BlockInfo&bi=R.info(bid);
  if(bi.shape==Registry::SH_PLANT){   // crossed plant: two diagonal quads, double sided
   const uint32_t tex=bi.texture[2]; const float x0=float(x)+0.05f,x1=float(x)+0.95f,z0=float(z)+0.05f,z1=float(z)+0.95f,y0=float(y),y1=float(y)+1.f;
   auto q=[&](float v){return (uint32_t)std::lround(v*16.f);};
   for(int d=0;d<2;d++){
    const float ax=d?x0:x0,az=d?z1:z0,bx=d?x1:x1,bz=d?z0:z1;
    const uint32_t base=(uint32_t)out.vertices.size(); const float px[4]={ax,bx,bx,ax},pz[4]={az,bz,bz,az},py[4]={y0,y0,y1,y1},uu[4]={0,1,1,0},vv[4]={1,1,0,0};
    for(int i=0;i<4;i++){ const uint32_t qy=q(py[i]); Vertex vt; vt.a=q(px[i])|(q(pz[i])<<9)|((qy&4095u)<<18)|(2u<<30); vt.b=q(uu[i])|(q(vv[i])<<9)|(tex<<18)|((qy>>12)<<31); out.vertices.push_back(vt); }
    out.indices.insert(out.indices.end(),{base,base+1,base+2,base,base+2,base+3,base,base+2,base+1,base,base+3,base+2});   // both sides
   }
   continue;
  }
  if(bi.shape){   // boxes (slabs, stairs, carpets, torches, chests) or neighbour-dependent shapes (fences, walls, panes)
   Box dyn[9]; std::vector<Box> sta; const Box*bp; int bn;
   if(bi.shape>=Registry::SH_FENCE&&bi.shape<=Registry::SH_PANE){
    auto conn=[&](int dx,int dz){ const uint16_t n=nb(x+dx,y,z+dz); if(!n)return false; const BlockInfo&ni=R.info(n); return (ni.conn&&ni.conn==bi.conn)||(ni.shape==0&&ni.solid&&!ni.transparent&&!ni.fluid&&ni.height16>=16); };
    bn=Registry::connectBoxes(bi.shape,conn(0,-1),conn(1,0),conn(0,1),conn(-1,0),dyn,false); bp=dyn; }
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
      case 0: addQuad(out,bi.translucent,0,float(x)+fx0,float(y)+fy0,float(z)+fz0,fx1-fx0,fz1-fz0,tex,fz0,false,fx0); break;
      case 1: addQuad(out,bi.translucent,1,float(x)+fx0,float(y)+fy1,float(z)+fz0,fx1-fx0,fz1-fz0,tex,fz0,false,fx0); break;
      case 2: addQuad(out,bi.translucent,2,float(x)+fx0,float(y)+fy0,float(z)+fz0,fz1-fz0,fy1-fy0,tex,1.f-fy1,false,fz0); break;
      case 3: addQuad(out,bi.translucent,3,float(x)+fx1,float(y)+fy0,float(z)+fz0,fz1-fz0,fy1-fy0,tex,1.f-fy1,false,1.f-fz1); break;
      case 4: addQuad(out,bi.translucent,4,float(x)+fx0,float(y)+fy0,float(z)+fz0,fx1-fx0,fy1-fy0,tex,1.f-fy1,false,1.f-fx1); break;
      default: addQuad(out,bi.translucent,5,float(x)+fx0,float(y)+fy0,float(z)+fz1,fx1-fx0,fy1-fy0,tex,1.f-fy1,false,fx0); break;
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
   if(face==1)addQuad(out,false,1,float(x),float(y)+hh,float(z),1.f,1.f,tex);
   else if(face==0)addQuad(out,false,0,float(x),float(y),float(z),1.f,1.f,tex);
   else{ float fx=float(x),fz=float(z); if(face==3)fx+=1.f; if(face==5)fz+=1.f; addQuad(out,false,face,fx,float(y),fz,1.f,hh,tex,1.f-hh); }
  }
 }
 (void)cx;(void)cz; out.maxY=H; out.vertices.shrink_to_fit(); out.indices.shrink_to_fit(); out.tindices.shrink_to_fit();
 return out;
}
