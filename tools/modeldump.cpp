// Headless harness: builds the real chunk mesh of every block standing alone and dumps it for the python renderer.
#include "World.hpp"
#include "TexturePack.hpp"
#include "Registry.hpp"
#include <cstdio>
#include <fstream>
#include <iostream>
int main(int argc,char**argv){
 AssetPaths ap; ap.base=AssetPaths::findBase(argv[1]); TexturePack tp; if(!tp.load(ap,"/tmp/hvcache")){std::cerr<<"texture load failed\n";return 1;}
 Registry&R=Registry::get(); R.init(tp);
 { std::ofstream f("/tmp/atlas.rgba",std::ios::binary); f.write((const char*)tp.atlas().rgba.data(),tp.atlas().rgba.size()); std::cout<<"atlas "<<tp.atlas().width<<"x"<<tp.atlas().height<<"\n"; }
 std::ofstream out("/tmp/models.txt");
 const int Y=64+10;
 auto emit=[&](const std::string&label,uint16_t id,const std::vector<std::array<int,4>>&extra){   // extra: x,y,z,id
  auto c=std::make_shared<Chunk>(); c->at(8,Y,8)=id; c->maxY=Y;
  if(const BlockId top=R.doorTopFor(id)){ c->at(8,Y+1,8)=top; c->maxY=Y+1; }
  for(auto&e:extra){ c->at(e[0],e[1],e[2])=(uint16_t)e[3]; c->maxY=std::max(c->maxY,e[1]); }
  ChunkSnapshot s{c,nullptr,nullptr,nullptr,nullptr};
  ChunkMesh m=World::buildMesh(s,0,0);
  out<<"M "<<id<<" "<<label<<" "<<m.vertices.size()<<" "<<m.indices.size()<<" "<<m.tindices.size()<<"\n";
  for(auto&v:m.vertices)out<<v.a<<" "<<v.b<<"\n";
  for(auto i:m.indices)out<<i<<" "; out<<"\n"; for(auto i:m.tindices)out<<i<<" "; out<<"\n";
 };
 size_t n=0;
 for(uint16_t id=1;id<R.blockCount();id++){
  if(R.doorBottomOf(id))continue;
  emit(R.blockName(id),id,{}); n++;
  const BlockInfo&bi=R.info(id);
  if(R.baseOf(id)==id&&bi.shape>=Registry::SH_FENCE&&bi.shape<=Registry::SH_PANE){   // connected variant: cobble to north and east
   emit(R.blockName(id)+"@NE",id,{{8,Y,7,(int)R.cobble},{9,Y,8,(int)R.cobble}}); }
 }
 std::cout<<"dumped "<<n<<" blocks (total ids "<<R.blockCount()<<")\n";
}
#include "Settings.hpp"
std::filesystem::path Settings::dataDir(){ return "/tmp/hvdata"; }
uint16_t mapMcBlock(const std::string&,const std::string&,const std::string&,const std::string&,const std::string&){ return 0; }
