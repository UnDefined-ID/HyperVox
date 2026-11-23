// Nether and End (terrain from Steel) + portals + switching between dimensions.
#include "Game.hpp"
#include "SteelClient.hpp"
#include <algorithm>
#include <cmath>

namespace{
bool openAt(const World&w,int x,int y,int z){ if(y<0||y>=Chunk::HEIGHT)return false; const uint16_t id=w.get(x,y,z); if(!id)return true; const BlockInfo&b=Registry::get().info(id); return !b.solid&&!b.fluid; }
bool isBlock(const World&w,int x,int y,int z,BlockId want){ const uint16_t id=w.get(x,y,z); return id&&Registry::get().baseOf(id)==want; }
}

// Saves the dimension you are leaving, then loads the other one (its chunks live in DIM-1/ and DIM1/ like in vanilla worlds).
void Game::switchDimension(int dim){
 if(dim==world_.dimension())return;
 saveWorld(true); waitSave(); if(tmpFut_.valid())tmpFut_.get(); if(streamer_)streamer_->reset();
 world_.reset(cur_.seed); world_.setDimension(dim); world_.setDebug(false); cur_.dim=dim; entitiesReset(cur_.folder,dim);
 { std::error_code ec; std::filesystem::remove_all(tmpDir_,ec); std::filesystem::create_directories(tmpDir_/"region",ec); }
 store_=std::make_shared<ChunkStore>(saves_.regionDirDim(cur_.folder,dim)); tmpStore_=std::make_shared<ChunkStore>(tmpDir_/"region");
 world_.setLoader([st=store_,tm=tmpStore_](int x,int z,Chunk&c){return st->load(x,z,c)||tm->load(x,z,c);});
 containers_.clear(); loadContainers(cur_.folder,dim);
 vel_=Vec3{}; portalT_=0; portalCool_=3.0; hit_=Hit{};
}

int Game::safeY(int x,int z,int lo,int hi){   // highest standing spot (solid below, two free blocks) between lo and hi, or -1
 for(int y=hi;y>lo;y--){ if(!openAt(world_,x,y,z)||!openAt(world_,x,y+1,z))continue; const uint16_t b=world_.get(x,y-1,z); if(b&&Registry::get().info(b).solid&&!Registry::get().info(b).fluid)return y; }
 return -1;
}

// Nether portal frame: obsidian ring around a 2 wide x 3 tall opening (axis x), with a small obsidian landing underneath; clears the space around it.
void Game::buildNetherPortal(int x,int y,int z){
 const Registry&R=Registry::get(); const BlockId obs=R.blockIdOrZero("obsidian"); if(!obs||!R.netherPortalX)return; int o[3][2];
 for(int dx=-1;dx<=2;dx++)for(int dz=-1;dz<=1;dz++)for(int dy=0;dy<=3;dy++){ if(dz!=0||dy>=0){ if(!(dz==0&&dx>=0&&dx<=1&&dy<=2)&&!(dz==0&&(dx==-1||dx==2)&&dy<=3)&&!(dz==0&&dy==3)) { world_.set(x+dx,y+dy,z+dz,0,o); } } }
 for(int dx=-1;dx<=2;dx++)for(int dz=-1;dz<=1;dz++)world_.set(x+dx,y-1,z+dz,obs,o);   // landing
 for(int dy=-0;dy<=3;dy++){ world_.set(x-1,y+dy,z,obs,o); world_.set(x+2,y+dy,z,obs,o); }
 for(int dx=0;dx<=1;dx++){ world_.set(x+dx,y-1,z,obs,o); world_.set(x+dx,y+3,z,obs,o); }
 for(int dx=0;dx<=1;dx++)for(int dy=0;dy<=2;dy++)world_.set(x+dx,y+dy,z,R.netherPortalX,o);
}

bool Game::findPortalNear(int x,int z,int&ox,int&oy,int&oz){
 const Registry&R=Registry::get(); long best=-1; const int r=16; const int cx0=(x-r)>>4,cx1=(x+r)>>4,cz0=(z-r)>>4,cz1=(z+r)>>4;
 for(int cz=cz0;cz<=cz1;cz++)for(int cx=cx0;cx<=cx1;cx++){ const auto c=world_.acquire(cx,cz,true); const int top=std::min(c->maxY,Chunk::HEIGHT-1);
  for(int y=0;y<=top;y++)for(int lz=0;lz<16;lz++)for(int lx=0;lx<16;lx++){ const uint16_t id=c->at(lx,y,lz); if(!id||!R.isNetherPortal(id))continue; const int wx=cx*16+lx,wz=cz*16+lz; if(std::abs(wx-x)>r||std::abs(wz-z)>r)continue;
   const long d=long(wx-x)*(wx-x)+long(wz-z)*(wz-z)*1+long(y)*0; if(best<0||d<best||(d==best&&y<oy)){best=d;ox=wx;oy=y;oz=wz;} } }
 if(best<0)return false;
 while(oy>0&&R.isNetherPortal(world_.get(ox,oy-1,oz)))oy--;   // the bottom of that portal column
 return true;
}

void Game::ensureEndExit(){   // the way back: a bedrock-rimmed end portal at the centre of the island
 const Registry&R=Registry::get(); const BlockId bed=R.bedrock; if(!bed||!R.endPortal)return; int top=world_.terrainHeight(0,0); const int y=top<=Chunk::Y_OFFSET+10?Chunk::Y_OFFSET+64:top+1; int o[3][2];
 if(isBlock(world_,0,y,0,R.endPortal))return;
 for(int dx=-2;dx<=2;dx++)for(int dz=-2;dz<=2;dz++){ for(int dy=1;dy<=4;dy++)world_.set(dx,y+dy,dz,0,o); world_.set(dx,y-1,dz,bed,o); const bool rim=std::max(std::abs(dx),std::abs(dz))==2; if(rim&&std::abs(dx)==2&&std::abs(dz)==2){ world_.set(dx,y,dz,0,o); continue; } world_.set(dx,y,dz,rim?bed:R.endPortal,o); }
}

void Game::travelTo(int to){
 const int from=world_.dimension(); if(to==from)return; const Vec3 old=pos_; const Registry&R=Registry::get();
 switchDimension(to);
 if(to==1){   // overworld -> nether: x / z divided by 8
  const int tx=(int)std::floor(old.x/8.f),tz=(int)std::floor(old.z/8.f); int px,py,pz;
  if(findPortalNear(tx,tz,px,py,pz)){ pos_=Vec3{float(px)+0.5f,float(py),float(pz)+0.5f}; }
  else { int y=safeY(tx,tz,Chunk::Y_OFFSET+32,Chunk::Y_OFFSET+120); if(y<0)y=Chunk::Y_OFFSET+70; buildNetherPortal(tx,y,tz); pos_=Vec3{float(tx)+0.5f,float(y),float(tz)+0.5f}; }
 } else if(to==0){
  if(from==1){ const int tx=(int)std::floor(old.x*8.f),tz=(int)std::floor(old.z*8.f); int px,py,pz;
   if(findPortalNear(tx,tz,px,py,pz)){ pos_=Vec3{float(px)+0.5f,float(py),float(pz)+0.5f}; }
   else { int y=world_.terrainHeight(tx,tz)+1; if(y<=Chunk::Y_OFFSET)y=Chunk::Y_OFFSET+64; buildNetherPortal(tx,y,tz); pos_=Vec3{float(tx)+0.5f,float(y),float(tz)+0.5f}; }
  } else pos_=world_.findSpawn();   // from the end: back to the world spawn
 } else {   // the end: obsidian landing platform at 100 / 0 like vanilla
  const BlockId obs=R.blockIdOrZero("obsidian"); int o[3][2]; const int y=Chunk::Y_OFFSET+48;
  for(int dx=-2;dx<=2;dx++)for(int dz=-2;dz<=2;dz++){ world_.set(100+dx,y,dz,obs,o); for(int dy=1;dy<=3;dy++)world_.set(100+dx,y+dy,dz,0,o); }
  pos_=Vec3{100.5f,float(y+1),0.5f}; ensureEndExit();
 }
 while(collides(pos_)&&pos_.y<Chunk::HEIGHT-8)pos_.y+=1.f;
 vel_=Vec3{}; fallPeak_=pos_.y; portalCool_=3.0; portalT_=0; playSound("portal.travel",0.7f,1.f);
}

// Standing in a nether portal for 4 seconds (instantly in creative) or touching an end portal sends you to the other dimension.
void Game::updatePortals(float dt){
 if(!inWorld_||screen_!=Screen::Play)return; if(portalCool_>0)portalCool_-=dt;
 const Registry&R=Registry::get(); const int x=(int)std::floor(pos_.x),z=(int)std::floor(pos_.z),y0=(int)std::floor(pos_.y+0.1f),y1=(int)std::floor(pos_.y+1.0f);
 const uint16_t a=world_.get(x,y0,z),b=world_.get(x,y1,z);
 if(R.isNetherPortal(a)||R.isNetherPortal(b)){ if(portalCool_>0){portalT_=0;return;} portalT_+=dt; if(portalT_<=dt*1.5f&&cur_.mode!=MODE_CREATIVE)playSound("portal.trigger",0.6f,1.f); if(portalT_>=(cur_.mode==MODE_CREATIVE?0.1:4.0)){ travelTo(world_.dimension()==0?1:0); } }
 else { portalT_=std::max(0.0,portalT_-double(dt)*2.0);
  if(R.endPortal&&portalCool_<=0&&((a&&R.baseOf(a)==R.endPortal)||(b&&R.baseOf(b)==R.endPortal))){ travelTo(world_.dimension()==2?0:2); } }
}

// flint and steel on obsidian: finds a rectangular obsidian frame (4x5 .. 23x23) around the fire position and fills it with portal blocks
bool Game::tryLightNetherPortal(int fx,int fy,int fz){
 const Registry&R=Registry::get(); const BlockId obs=R.blockIdOrZero("obsidian"); if(!obs||!R.netherPortalX)return false; int o[3][2];
 for(int axis=0;axis<2;axis++){   // 0: frame runs along x, 1: along z
  const int dx=axis==0?1:0,dz=axis==0?0:1; int y0=fy; while(y0>fy-22&&openAt(world_,fx,y0-1,fz)&&!isBlock(world_,fx,y0-1,fz,obs))y0--; if(!isBlock(world_,fx,y0-1,fz,obs))continue;
  int l=0,r=0; while(l<22&&openAt(world_,fx-dx*(l+1),y0,fz-dz*(l+1))&&!isBlock(world_,fx-dx*(l+1),y0,fz-dz*(l+1),obs))l++; if(!isBlock(world_,fx-dx*(l+1),y0,fz-dz*(l+1),obs))continue;
  while(r<22&&openAt(world_,fx+dx*(r+1),y0,fz+dz*(r+1))&&!isBlock(world_,fx+dx*(r+1),y0,fz+dz*(r+1),obs))r++; if(!isBlock(world_,fx+dx*(r+1),y0,fz+dz*(r+1),obs))continue;
  const int w=l+r+1; if(w<2||w>21)continue; const int sx=fx-dx*l,sz=fz-dz*l;
  int h=0; bool ok=true; while(h<21){ bool rowOpen=true; for(int i=0;i<w;i++)if(!openAt(world_,sx+dx*i,y0+h,sz+dz*i)){rowOpen=false;break;} if(!rowOpen)break; h++; }
  if(h<3||h>21)continue;
  for(int i=0;i<w&&ok;i++)if(!isBlock(world_,sx+dx*i,y0-1,sz+dz*i,obs)||!isBlock(world_,sx+dx*i,y0+h,sz+dz*i,obs))ok=false;   // bottom + top rows
  for(int j=0;j<h&&ok;j++)if(!isBlock(world_,sx-dx,y0+j,sz-dz,obs)||!isBlock(world_,sx+dx*w,y0+j,sz+dz*w,obs))ok=false;           // both sides
  if(!ok)continue;
  const BlockId pid=axis==0?R.netherPortalX:R.netherPortalZ; for(int i=0;i<w;i++)for(int j=0;j<h;j++){ const int n=world_.set(sx+dx*i,y0+j,sz+dz*i,pid,o); markDirtyAround(sx+dx*i,sz+dz*i,n,o); }
  return true; }
 return false;
}

// eye of ender on an end portal frame; when all 12 frames of a ring have their eye the 3x3 inside becomes an end portal
bool Game::tryEyeOnFrame(int x,int y,int z){
 const Registry&R=Registry::get(); const uint16_t id=world_.get(x,y,z); if(!id||R.endFrameEye(id)!=0)return false; int o[3][2];
 { const int n=world_.set(x,y,z,R.endFrameWithEye(id),o); markDirtyAround(x,z,n,o); playSound("block.end_portal.eyeplace",0.8f,1.f); }
 if(!R.endPortal)return true;
 for(int cx=x-3;cx<=x+3;cx++)for(int cz=z-3;cz<=z+3;cz++){   // candidate centre of the 3x3 hole
  bool ok=true; for(int dx=-2;dx<=2&&ok;dx++)for(int dz=-2;dz<=2&&ok;dz++){ const int ax=std::abs(dx),az=std::abs(dz); if(ax==2&&az==2)continue; if(ax<2&&az<2)continue; const uint16_t f=world_.get(cx+dx,y,cz+dz); if(!f||R.endFrameEye(f)!=1)ok=false; }
  if(!ok)continue;
  for(int dx=-1;dx<=1;dx++)for(int dz=-1;dz<=1;dz++){ const int n=world_.set(cx+dx,y,cz+dz,R.endPortal,o); markDirtyAround(cx+dx,cz+dz,n,o); }
  playSound("block.end_portal.endportal",1.f,1.f); return true; }
 return true;
}
