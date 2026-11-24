// Block updates (what may stand where: plants need dirt, torches need a wall ...), falling blocks, and flowing water / lava.
// World::set logs every change; each game tick the neighbours of changed blocks are re-checked (blocksTick).
#include "Game.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace{
enum Kind:uint8_t{ K_NONE=0,K_PLANT,K_CROP,K_CANE,K_CACTUS,K_DEAD,K_MUSH,K_LILY,K_FLOOR,K_TORCH,K_LADDER,K_FALL };
bool ends(const std::string&s,const char*e){ const size_t l=std::strlen(e); return s.size()>=l&&s.compare(s.size()-l,l,e)==0; }
uint8_t kindOfName(const std::string&n){
 static const char*plants[]={"short_grass","tall_grass","fern","large_fern","dandelion","poppy","blue_orchid","allium","azure_bluet","red_tulip","orange_tulip","white_tulip","pink_tulip","oxeye_daisy","cornflower","lily_of_the_valley","sunflower","lilac","rose_bush","peony","sweet_berry_bush","bush","torchflower","pitcher_plant","wither_rose","azalea","flowering_azalea","bamboo","closed_eyeblossom","open_eyeblossom","firefly_bush"};
 for(const char*p:plants)if(n==p)return K_PLANT; if(ends(n,"_sapling")||ends(n,"_propagule"))return K_PLANT;
 static const char*crops[]={"wheat","carrots","potatoes","beetroots","melon_stem","pumpkin_stem","attached_melon_stem","attached_pumpkin_stem","torchflower_crop","pitcher_crop","nether_wart"};
 for(const char*p:crops)if(n==p)return K_CROP;
 if(n=="sugar_cane")return K_CANE; if(n=="cactus")return K_CACTUS; if(n=="dead_bush")return K_DEAD; if(n=="brown_mushroom"||n=="red_mushroom")return K_MUSH; if(n=="lily_pad")return K_LILY;
 if(ends(n,"rail")||ends(n,"_pressure_plate")||(ends(n,"_carpet")&&n.find("moss")==std::string::npos)||n=="redstone_wire"||n=="repeater"||n=="comparator"||n=="snow"||n=="leaf_litter"||n=="pink_petals"||n=="wildflowers")return K_FLOOR;
 if(n=="torch"||n=="soul_torch"||n=="redstone_torch"||n=="copper_torch")return K_TORCH; if(n=="ladder")return K_LADDER;
 if(n=="sand"||n=="red_sand"||n=="gravel"||ends(n,"_concrete_powder")||n=="anvil"||n=="chipped_anvil"||n=="damaged_anvil"||n=="dragon_egg")return K_FALL;
 return K_NONE;
}
uint64_t keyOf(int x,int y,int z){ return ((uint64_t)(uint32_t)(x+30000000)<<38)^((uint64_t)(uint32_t)(z+30000000)<<12)^(uint64_t)(uint32_t)y; }
}


namespace{
bool isRailBase(const Registry&R,BlockId b){ const std::string&n=R.blockName(b); return n.size()>=4&&n.compare(n.size()-4,4,"rail")==0; }
}
// rails connect to their neighbours: straight, ascending towards a rail one block higher, curved (plain rails only)
uint16_t Game::railVariantFor(int x,int y,int z,uint16_t cur){
 const Registry&R=Registry::get(); const BlockId base=R.baseOf(cur); const bool plain=R.blockName(base)=="rail"; const int curSlot=R.variantIndex(cur); const bool on=!plain&&curSlot>=6;
 static const int DX[4]={0,1,0,-1},DZ[4]={-1,0,1,0}; bool conn[4]={false,false,false,false},up[4]={false,false,false,false};
 for(int d=0;d<4;d++){ auto isR=[&](int yy){ const uint16_t b=world_.get(x+DX[d],yy,z+DZ[d]); return b&&isRailBase(R,R.baseOf(b)); }; if(isR(y))conn[d]=true; else if(isR(y+1)){ conn[d]=true; up[d]=true; } else if(isR(y-1))conn[d]=true; }
 int shape=0;
 if(conn[0]&&conn[2])shape=0; else if(conn[1]&&conn[3])shape=1;
 else if(plain&&(conn[0]||conn[2])&&(conn[1]||conn[3])){ if(conn[2]&&conn[1])shape=6; else if(conn[2]&&conn[3])shape=7; else if(conn[0]&&conn[3])shape=8; else shape=9; }
 else if(conn[1]||conn[3])shape=1; else shape=0;
 if(shape==0){ if(up[0])shape=4; else if(up[2])shape=5; } else if(shape==1){ if(up[1])shape=2; else if(up[3])shape=3; }
 const int slot=plain?shape:(shape+(on?6:0)); return slot==0?(uint16_t)base:(uint16_t)R.variantOf(base,slot);
}
void Game::queueBlockUpdate(int x,int y,int z){ if(y<0||y>=Chunk::HEIGHT||bupd_.size()>60000)return; bupd_.push_back({x,y,z}); }
void Game::queueFluid(int x,int y,int z,int delay){ if(y<0||y>=Chunk::HEIGHT||fluidQ_.size()>30000)return; if(!fluidSet_.insert(keyOf(x,y,z)).second)return; fluidQ_.push_back(FluidJob{x,y,z,gameTicks_+(unsigned long long)delay}); }

bool Game::supportOk(int x,int y,int z,uint16_t id){
 if(!id||y<1)return true; const Registry&R=Registry::get(); static std::vector<uint8_t> kinds; if(kinds.size()!=R.blockCount()+1){ kinds.assign(R.blockCount()+1,0); for(size_t i=1;i<kinds.size();i++)kinds[i]=kindOfName(R.blockName(R.baseOf((BlockId)i))); }
 const uint8_t k=id<kinds.size()?kinds[id]:0; if(k==K_NONE||k==K_FALL)return true;
 auto nameAt=[&](int xx,int yy,int zz)->std::string{ const uint16_t b=world_.get(xx,yy,zz); return b?R.blockName(R.baseOf(b)):std::string(); };
 auto solidFull=[&](int xx,int yy,int zz){ const uint16_t b=world_.get(xx,yy,zz); if(!b)return false; const BlockInfo&bi=R.info(b); return bi.solid&&!bi.fluid&&bi.shape==0&&bi.height16>=16; };
 auto solidAny=[&](int xx,int yy,int zz){ const uint16_t b=world_.get(xx,yy,zz); if(!b)return false; const BlockInfo&bi=R.info(b); return bi.solid&&!bi.fluid&&bi.shape!=Registry::SH_PLANT; };
 auto isDirt=[&](const std::string&n){ return n=="dirt"||n=="grass_block"||n=="podzol"||n=="coarse_dirt"||n=="rooted_dirt"||n=="mycelium"||n=="moss_block"||n=="farmland"||n=="mud"||n=="muddy_mangrove_roots"||n=="dirt_path"; };
 const std::string self=R.blockName(R.baseOf(id)); const std::string below=nameAt(x,y-1,z);
 switch(k){
  case K_PLANT: return isDirt(below)||below==self||(self.find("azalea")!=std::string::npos&&below=="clay")||(self=="bamboo"&&(below=="sand"||below=="red_sand"||below=="gravel"));
  case K_CROP: return self=="nether_wart"?below=="soul_sand":below=="farmland";
  case K_CANE: { if(below==self)return true; if(!(isDirt(below)||below=="sand"||below=="red_sand"))return false; static const int d[4][2]={{1,0},{-1,0},{0,1},{0,-1}}; for(auto&q:d){ const uint16_t w=world_.get(x+q[0],y-1,z+q[1]); if(w&&R.info(w).fluid)return true; } return false; }
  case K_CACTUS: { if(!(below=="sand"||below=="red_sand"||below=="cactus"))return false; static const int d[4][2]={{1,0},{-1,0},{0,1},{0,-1}}; for(auto&q:d)if(solidFull(x+q[0],y,z+q[1]))return false; return true; }
  case K_DEAD: return below=="sand"||below=="red_sand"||ends(below,"terracotta")||isDirt(below)||below=="dead_bush";
  case K_MUSH: return solidAny(x,y-1,z);
  case K_LILY: { const uint16_t b=world_.get(x,y-1,z); return b&&R.info(b).fluid&&R.baseOf(b)==R.water; }
  case K_FLOOR: return solidAny(x,y-1,z);
  case K_TORCH: { const int slot=R.variantIndex(id); if(slot<=0||slot>4)return solidAny(x,y-1,z); static const int wx[5]={0,-1,1,0,0},wz[5]={0,0,0,-1,1}; return solidFull(x+wx[slot],y,z+wz[slot]); }
  case K_LADDER: { static const int lx[4]={0,1,0,-1},lz[4]={-1,0,1,0}; const int f=R.variantIndex(id)&3; return solidFull(x-lx[f],y,z-lz[f]); }
  default: return true; }
}

void Game::breakByUpdate(int x,int y,int z){
 const uint16_t id=world_.get(x,y,z); if(!id)return; const Registry&R=Registry::get(); const ItemId d=R.dropFor(id);
 int out[3][2]; const int n=world_.set(x,y,z,0,out); markDirtyAround(x,z,n,out); markWorldDirty();
 if(d)ents_.spawnItem(Vec3{x+0.5f,y+0.3f,z+0.5f},d,1,Vec3{0,2.f,0},0.3f);
}

void Game::fluidStep(int x,int y,int z){
 const Registry&R=Registry::get(); const BlockId wb=R.water,lb=R.blockByName("lava");
 auto fam=[&](uint16_t id)->int{ if(!id||!R.info(id).fluid)return 0; const BlockId b=R.baseOf(id); return b==wb?1:(b==lb?2:0); };
 auto lvl=[&](uint16_t id)->int{ return id?R.variantIndex(id):0; };
 auto mk=[&](int f,int l)->uint16_t{ const BlockId b=f==1?wb:lb; return l<=0?b:R.variantOf(b,l); };
 auto put=[&](int xx,int yy,int zz,uint16_t id){ int out[3][2]; const int n=world_.set(xx,yy,zz,id,out); markDirtyAround(xx,zz,n,out); markWorldDirty(); };
 auto replaceable=[&](uint16_t id){ return !id||R.info(id).shape==Registry::SH_PLANT; };
 const uint16_t me=world_.get(x,y,z); const int f=fam(me); if(!f)return; int l=lvl(me); const int maxSpread=f==1?7:3;
 const uint16_t above=world_.get(x,y+1,z);
 // flowing cells need a feed or they dry up
 if(l>=1){
  if(fam(above)==f){ if(l!=8){ put(x,y,z,mk(f,8)); queueFluid(x,y,z,f==1?5:30); return; } }
  else { bool fed=false; static const int d[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
   if(l<8)for(auto&q:d){ const uint16_t t=world_.get(x+q[0],y,z+q[1]); if(fam(t)==f){ const int e=lvl(t)==8?0:lvl(t); if(e<l){ fed=true; break; } } }
   if(!fed){ put(x,y,z,0); return; } }
 }
 auto clash=[&](int tx,int ty,int tz,uint16_t t,bool down)->bool{   // water meets lava: obsidian / cobblestone / stone
  const int tf=fam(t); if(!tf||tf==f)return false; const char*res; if(f==1)res=lvl(t)==0?"obsidian":"cobblestone"; else res=down?"stone":"cobblestone";
  const uint16_t rid=R.blockIdOrZero(res); if(rid)put(tx,ty,tz,rid); return true; };
 const uint16_t below=world_.get(x,y-1,z);
 if(y>1&&(replaceable(below)||fam(below)!=0)){
  if(clash(x,y-1,z,below,true))return;
  if(fam(below)==f){ if(lvl(below)!=8&&lvl(below)!=0){ put(x,y-1,z,mk(f,8)); queueFluid(x,y-1,z,f==1?5:30); } return; }
  if(replaceable(below)){ put(x,y-1,z,mk(f,8)); queueFluid(x,y-1,z,f==1?5:30); return; }
 }
 const int eff=(l==8)?0:l; const int nl=eff+1; if(nl>maxSpread)return;
 static const int d4[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
 for(auto&q:d4){ const int tx=x+q[0],tz=z+q[1]; const uint16_t t=world_.get(tx,y,tz);
  if(clash(tx,y,tz,t,false))continue;
  if(replaceable(t)){ put(tx,y,tz,mk(f,nl)); queueFluid(tx,y,tz,f==1?5:30); }
  else if(fam(t)==f&&lvl(t)>nl&&lvl(t)!=8){ put(tx,y,tz,mk(f,nl)); queueFluid(tx,y,tz,f==1?5:30); } }
}

void Game::blocksTick(){
 const Registry&R=Registry::get();
 // 1. what changed: re-check the neighbours (and wake fluids)
 for(const auto&c:world_.takeChanges()){
  static const int d[7][3]={{0,0,0},{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
  for(auto&q:d){ const int x=c[0]+q[0],y=c[1]+q[1],z=c[2]+q[2]; queueBlockUpdate(x,y,z); const uint16_t b=world_.get(x,y,z); if(b&&R.info(b).fluid)queueFluid(x,y,z,R.baseOf(b)==R.water?5:30); } }
 // 2. block updates: unsupported blocks pop off, sand / gravel fall
 for(int n=0;n<160&&!bupd_.empty();n++){
  const auto c=bupd_.front(); bupd_.pop_front(); const uint16_t id=world_.get(c[0],c[1],c[2]); if(!id)continue;
  if(!supportOk(c[0],c[1],c[2],id)){ breakByUpdate(c[0],c[1],c[2]); continue; }
  if(isRailBase(R,R.baseOf(id))){ const uint16_t want=railVariantFor(c[0],c[1],c[2],id); if(want&&want!=id){ int o[3][2]; const int n=world_.set(c[0],c[1],c[2],want,o); markDirtyAround(c[0],c[2],n,o); markWorldDirty(); } continue; }
  const std::string&bn=R.blockName(R.baseOf(id));
  if(bn=="sand"||bn=="red_sand"||bn=="gravel"||ends(bn,"_concrete_powder")||bn=="anvil"||bn=="chipped_anvil"||bn=="damaged_anvil"||bn=="dragon_egg"){
   auto passable=[&](uint16_t b){ if(!b)return true; const BlockInfo&bi=R.info(b); return bi.fluid||bi.shape==Registry::SH_PLANT||!bi.solid; };
   if(c[1]>1&&passable(world_.get(c[0],c[1]-1,c[2]))){ int ny=c[1]-1; while(ny>1&&c[1]-ny<300&&passable(world_.get(c[0],ny-1,c[2])))ny--;
    int o1[3][2],o2[3][2]; const int n1=world_.set(c[0],c[1],c[2],0,o1); markDirtyAround(c[0],c[2],n1,o1); const int n2=world_.set(c[0],ny,c[2],id,o2); markDirtyAround(c[0],c[2],n2,o2); markWorldDirty(); playSound("block.sand.place",0.4f,1.f); }
  } }
 // 3. fluids (water every 5 ticks, lava every 30)
 for(int n=0;n<240&&!fluidQ_.empty();n++){ const FluidJob j=fluidQ_.front(); if(j.due>gameTicks_)break; fluidQ_.pop_front(); fluidSet_.erase(keyOf(j.x,j.y,j.z)); fluidStep(j.x,j.y,j.z); }
}
