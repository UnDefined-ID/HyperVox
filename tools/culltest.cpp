#include "World.hpp"
#include "TexturePack.hpp"
#include "Registry.hpp"
#include <cstdio>
#include <random>
#include <iostream>
int main(int,char**argv){
 AssetPaths ap; ap.base=AssetPaths::findBase(argv[1]); TexturePack tp; tp.load(ap,"/tmp/hvcache"); Registry&R=Registry::get(); R.init(tp);
 std::mt19937 rng(5); long checked=0,bad=0,tris=0,kept=0;
 for(int t=0;t<40;t++){
  auto c=std::make_shared<Chunk>(); const int Y=64+10; int maxY=0;
  for(int i=0;i<300;i++){ int x=rng()%16,y=Y+rng()%12,z=rng()%16; uint16_t id=1+rng()%(R.blockCount()-1); if(R.doorBottomOf(id))continue; c->at(x,y,z)=id; maxY=std::max(maxY,y);} c->maxY=maxY; c->pack();
  ChunkSnapshot s{c,nullptr,nullptr,nullptr,nullptr}; ChunkMesh m=World::buildMesh(s,0,0);
  for(int k=0;k<60;k++){
   float cam[3]={float(int(rng()%80)-32)+0.37f,float(Y-30+int(rng()%80))+0.21f,float(int(rng()%80)-32)+0.61f};
   unsigned mask=visibleBuckets(0-cam[0]+0,0-cam[1],0-cam[2],m.maxY-1); // chunk min corner relative to camera
   mask=visibleBuckets(-cam[0],-cam[1],-cam[2],m.maxY-1);
   std::vector<char> drawn(m.vertices.size()/4,0);
   forEachBucketRun(m.bucketEnd,mask,[&](uint32_t f,uint32_t n){ for(uint32_t q=f/4;q<(f+n)/4;q++)drawn[q]=1; });
   for(size_t q=0;q<m.vertices.size()/4;q++){
    if(q*4>=m.bucketEnd[6])break;   // translucent not culled
    auto pos=[&](int i,float*p){ uint32_t a=m.vertices[q*4+i].a,b=m.vertices[q*4+i].b; p[0]=(a&511)/16.f;p[2]=((a>>9)&511)/16.f;p[1]=(((a>>18)&4095)|((b>>31)<<12))/16.f; };
    float p0[3],p1[3],p2[3]; pos(0,p0);pos(1,p1);pos(2,p2);
    float e1[3],e2[3],n[3]; for(int i=0;i<3;i++){e1[i]=p1[i]-p0[i];e2[i]=p2[i]-p0[i];}
    n[0]=e1[1]*e2[2]-e1[2]*e2[1];n[1]=e1[2]*e2[0]-e1[0]*e2[2];n[2]=e1[0]*e2[1]-e1[1]*e2[0];
    float d=n[0]*(cam[0]-p0[0])+n[1]*(cam[1]-p0[1])+n[2]*(cam[2]-p0[2]);
    bool front=d>1e-4f; tris++; if(drawn[q])kept++;
    if(front&&!drawn[q]){bad++;} checked++;
   }
  }
 }
 printf("quads checked %ld, front-facing quads wrongly culled: %ld, drawn fraction %.3f\n",checked,bad,double(kept)/tris);
}
