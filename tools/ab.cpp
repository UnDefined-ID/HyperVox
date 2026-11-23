// A/B test: same synthetic terrain meshed by the old and the new mesher; compares the triangle sets and times both.
#include "World.hpp"
#include "TexturePack.hpp"
#include "Registry.hpp"
#include "Settings.hpp"
#include <chrono>
#include <cstdio>
#include <iostream>
#include <set>
#include <algorithm>
#include <map>
static uint32_t H32(uint32_t x){x^=x>>16;x*=0x7feb352dU;x^=x>>15;x*=0x846ca68bU;x^=x>>16;return x;}
static float vn(int x,int z,int s){return float(H32(uint32_t(x)*73856093u^uint32_t(z)*19349663u^uint32_t(s)*83492791u)&0xFFFF)/65535.f;}
static float noise(float x,float z,int s){int xi=(int)std::floor(x),zi=(int)std::floor(z);float fx=x-xi,fz=z-zi;fx=fx*fx*(3-2*fx);fz=fz*fz*(3-2*fz);
 float a=vn(xi,zi,s),b=vn(xi+1,zi,s),c=vn(xi,zi+1,s),d=vn(xi+1,zi+1,s);return (a+(b-a)*fx)*(1-fz)+(c+(d-c)*fx)*fz;}
static void genChunk(Chunk&c,int cx,int cz){
 Registry&R=Registry::get(); auto B=[&](const char*n){return R.blockIdOrZero(n);};
 const uint16_t stone=R.stone,dirt=R.dirt,grass=R.grass,water=R.water,sand=R.sand,deep=B("deepslate"),log=R.log,leaves=R.leaves,glass=B("glass"),coal=B("coal_ore"),slab=B("oak_slab"),stairs=B("oak_stairs"),fence=B("oak_fence"),tg=B("tinted_glass"),sgp=B("short_grass");
 std::vector<uint16_t> sp; for(uint16_t i=1;i<R.blockCount();i+=37)sp.push_back(i);
 for(int lz=0;lz<16;lz++)for(int lx=0;lx<16;lx++){
  const int wx=cx*16+lx,wz=cz*16+lz; const float h=noise(wx*0.02f,wz*0.02f,1)*60+noise(wx*0.1f,wz*0.1f,2)*10; const int top=100+(int)h;   // y 100..170
  for(int y=0;y<top;y++){ uint16_t id=y<40?deep:stone; if(y>=top-4)id=dirt; if(y==top-1)id=(top<128?sand:grass);
   if(y>12&&y<top-6){ const float cv=noise(wx*0.07f+y*0.05f,wz*0.07f+y*0.09f,5); if(cv>0.62f)id=0; }
   if(id==stone&&H32(wx*31+y*17+wz*13)%97==0)id=coal; c.at(lx,y,lz)=id; }
  for(int y=top;y<126;y++)c.at(lx,y,lz)=water;
  if(top>=128){ const uint32_t r=H32(wx*7+wz*131)%100; if(r<3){ for(int t=0;t<5;t++)c.at(lx,top+t,lz)=log; for(int dy=3;dy<7;dy++)c.at(lx,top+dy,lz)=leaves; }
   else if(r<10&&sgp)c.at(lx,top,lz)=sgp; else if(r<12&&slab)c.at(lx,top,lz)=slab; else if(r<13&&stairs)c.at(lx,top,lz)=stairs; else if(r<14&&fence)c.at(lx,top,lz)=fence;
   else if(r<15&&glass)c.at(lx,top,lz)=glass; else if(r<16&&tg)c.at(lx,top,lz)=tg; else if(r==17)c.at(lx,top,lz)=sp[H32(wx+wz*5)%sp.size()]; }
 }
 int mx=0; for(int y=0;y<384;y++)for(int z=0;z<16;z++)for(int x=0;x<16;x++)if(c.at(x,y,z))mx=std::max(mx,y); c.maxY=mx;
}
using V=std::pair<uint32_t,uint32_t>; using Tri=std::array<V,3>;
static Tri canon(V a,V b,V c){ std::array<V,3> t={a,b,c}; int m=0; for(int i=1;i<3;i++)if(t[i]<t[m])m=i; return Tri{t[m],t[(m+1)%3],t[(m+2)%3]}; }
static uint64_t hashTris(std::vector<std::pair<Tri,int>>&v){ std::sort(v.begin(),v.end()); uint64_t h=1469598103934665603ULL; for(auto&p:v){ for(auto&x:p.first){h=(h^x.first)*1099511628211ULL;h=(h^x.second)*1099511628211ULL;} h=(h^uint64_t(p.second))*1099511628211ULL; } return h; }
int main(int argc,char**argv){
 AssetPaths ap; ap.base=AssetPaths::findBase(argv[1]); TexturePack tp; if(!tp.load(ap,"/tmp/hvcache")){std::cerr<<"texture load failed\n";return 1;}
 Registry::get().init(tp);
 const int G=8; std::map<std::pair<int,int>,std::shared_ptr<const Chunk>> ch; size_t denseB=0,packedB=0;
 for(int cz=0;cz<G;cz++)for(int cx=0;cx<G;cx++){ auto c=std::make_shared<Chunk>(); genChunk(*c,cx,cz); denseB+=c->blocks.size()*2;
#ifndef OLD
  c->pack(); packedB+=c->bytes();
#endif
  ch[{cx,cz}]=c; }
 auto get=[&](int x,int z)->std::shared_ptr<const Chunk>{ auto it=ch.find({x,z}); return it==ch.end()?nullptr:it->second; };
 size_t totalTris=0; uint64_t all=0; std::vector<ChunkMesh> keep;
 auto t0=std::chrono::steady_clock::now(); int reps=3; size_t vtx=0;
 for(int r=0;r<reps;r++)for(int cz=1;cz<G-1;cz++)for(int cx=1;cx<G-1;cx++){
  ChunkSnapshot s{get(cx,cz),get(cx-1,cz),get(cx+1,cz),get(cx,cz-1),get(cx,cz+1)};
  ChunkMesh m=World::buildMesh(s,cx,cz);
  if(r==0){
   std::vector<std::pair<Tri,int>> tr;
#ifdef OLD
   for(size_t i=0;i+2<m.indices.size();i+=3)tr.push_back({canon({m.vertices[m.indices[i]].a,m.vertices[m.indices[i]].b},{m.vertices[m.indices[i+1]].a,m.vertices[m.indices[i+1]].b},{m.vertices[m.indices[i+2]].a,m.vertices[m.indices[i+2]].b}),0});
   for(size_t i=0;i+2<m.tindices.size();i+=3)tr.push_back({canon({m.vertices[m.tindices[i]].a,m.vertices[m.tindices[i]].b},{m.vertices[m.tindices[i+1]].a,m.vertices[m.tindices[i+1]].b},{m.vertices[m.tindices[i+2]].a,m.vertices[m.tindices[i+2]].b}),1});
   vtx+=m.vertices.size()*8+(m.indices.size()+m.tindices.size())*2;
#else
   for(size_t q=0;q+3<m.vertices.size();q+=4){ int t=q>=m.bucketEnd[6]?1:0; auto&v=m.vertices; V a{v[q].a,v[q].b},b{v[q+1].a,v[q+1].b},c{v[q+2].a,v[q+2].b},d{v[q+3].a,v[q+3].b}; tr.push_back({canon(a,b,c),t}); tr.push_back({canon(a,c,d),t}); }
   vtx+=m.vertices.size()*8;
#endif
   totalTris+=tr.size(); uint64_t h=hashTris(tr); all^=h*(cx*31+cz+7); printf("chunk %d %d tris %zu hash %016llx\n",cx,cz,tr.size(),(unsigned long long)h);
  }
 }
 double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t0).count()/(reps*(G-2)*(G-2));
 printf("TOTAL tris %zu allhash %016llx\n",totalTris,(unsigned long long)all);
 fprintf(stderr,"ms/chunk %.3f  gpu bytes (6x6 chunks) %zu  dense chunk bytes %zu packed %zu\n",ms,vtx,denseB,packedB);
}
