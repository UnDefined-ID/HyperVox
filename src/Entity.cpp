#include "Entity.hpp"
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <random>

namespace{
std::mt19937& rng(){ static std::mt19937 g(0xC0FFEEu); return g; }
float rnd(float a,float b){ return std::uniform_real_distribution<float>(a,b)(rng()); }
int rndi(int a,int b){ return std::uniform_int_distribution<int>(a,b)(rng()); }
const float PI=3.14159265f;
float angDiff(float a,float b){ float d=std::fmod(b-a+PI,2*PI); if(d<0)d+=2*PI; return d-PI; }
float yawToward(float dx,float dz){ return std::atan2(-dx,-dz); }          // forward = (-sin yaw, -cos yaw)
Vec3 fwd(float yaw){ return Vec3{-std::sin(yaw),0,-std::cos(yaw)}; }
float len2(const Vec3&v){ return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z); }

struct Dims{ float w,h,hp; };
Dims dimsOf(EType t){
 switch(t){ case EType::Item: return {0.25f,0.25f,5}; case EType::Cow: return {0.9f,1.4f,10}; case EType::Pig: return {0.9f,0.9f,10}; case EType::Chicken: return {0.4f,0.7f,4};
  case EType::Creeper: return {0.6f,1.7f,20}; case EType::Enderman: return {0.6f,2.9f,40}; case EType::Blaze: return {0.6f,1.8f,20}; case EType::Dragon: return {8.f,4.f,200};
  case EType::Crystal: return {2.f,2.f,1}; case EType::Boat: return {1.4f,0.6f,4}; case EType::Minecart: return {0.98f,0.7f,4}; case EType::Zombie: return {0.6f,1.95f,20}; case EType::Skeleton: return {0.6f,1.99f,20}; case EType::Spider: return {1.4f,0.9f,16}; case EType::Sheep: return {0.9f,1.3f,8}; case EType::Ghast: return {4.f,4.f,10};
  case EType::Slime: return {0.52f,0.52f,4}; case EType::Wolf: return {0.6f,0.85f,8}; case EType::Villager: return {0.6f,1.95f,20}; case EType::Arrow: return {0.2f,0.2f,1}; default: return {0.3f,0.3f,1}; }
}
bool isNamed(const World&w,int x,int y,int z,const char*name){ const uint16_t id=const_cast<World&>(w).get(x,y,z); return id&&Registry::get().blockName(Registry::get().baseOf(id))==name; }
bool isWater(World&w,int x,int y,int z){ const uint16_t id=w.get(x,y,z); return id&&Registry::get().info(id).fluid&&Registry::get().baseOf(id)==Registry::get().water; }
bool isLava(World&w,int x,int y,int z){ const uint16_t id=w.get(x,y,z); return id&&Registry::get().info(id).fluid&&Registry::get().blockName(Registry::get().baseOf(id))=="lava"; }
bool isRail(World&w,int x,int y,int z){ const uint16_t id=w.get(x,y,z); if(!id)return false; const std::string&n=Registry::get().blockName(Registry::get().baseOf(id)); return n.size()>=4&&n.compare(n.size()-4,4,"rail")==0; }
uint16_t blockTop16(World&w,int x,int y,int z){      // height of the solid part of a block in 1/16 (0 = nothing to collide with)
 const uint16_t id=w.get(x,y,z); if(!id)return 0; const BlockInfo&bi=Registry::get().info(id); if(!bi.solid||bi.fluid)return 0; return bi.height16?bi.height16:16;
}
bool boxHits(World&w,const Vec3&p,float hw,float h){
 const int x0=(int)std::floor(p.x-hw+1e-4f),x1=(int)std::floor(p.x+hw-1e-4f),y0=(int)std::floor(p.y+1e-4f),y1=(int)std::floor(p.y+h-1e-4f),z0=(int)std::floor(p.z-hw+1e-4f),z1=(int)std::floor(p.z+hw-1e-4f);
 for(int y=y0;y<=y1;y++)for(int z=z0;z<=z1;z++)for(int x=x0;x<=x1;x++){ const uint16_t t=blockTop16(w,x,y,z); if(!t)continue; if(float(y)+float(t)/16.f>p.y+1e-4f)return true; }
 return false;
}
// moves an entity with collision; step = max step up height
void moveBody(Entity&e,World&w,float dt,float step){
 e.wall=false; const float hw=e.w*0.5f; int n=std::max(1,(int)std::ceil(len2(e.vel)*dt/0.4f)); const float sdt=dt/n;
 for(int i=0;i<n;i++){
  { Vec3 np=e.pos; np.y+=e.vel.y*sdt; if(!boxHits(w,np,hw,e.h))e.pos=np; else { if(e.vel.y<0)e.onGround=true; e.vel.y=0; } }
  for(int ax=0;ax<2;ax++){ const float d=(ax==0?e.vel.x:e.vel.z)*sdt; if(d==0)continue; Vec3 np=e.pos; (ax==0?np.x:np.z)+=d;
   if(!boxHits(w,np,hw,e.h)){ e.pos=np; continue; }
   bool ok=false; if(step>0&&(e.onGround||e.inWater)){ for(float up:{0.5f,1.0f}){ if(up>step+0.01f)break; Vec3 sp=np; sp.y+=up; if(!boxHits(w,sp,hw,e.h)&&!boxHits(w,Vec3{e.pos.x,e.pos.y+up,e.pos.z},hw,e.h)){ e.pos=sp; ok=true; break; } } }
   if(!ok){ (ax==0?e.vel.x:e.vel.z)=0; e.wall=true; } }
  Vec3 below=e.pos; below.y-=0.02f; if(e.vel.y<=0&&boxHits(w,below,hw,e.h))e.onGround=true;
 }
}
void applyFluid(Entity&e,World&w){
 const int x=(int)std::floor(e.pos.x),z=(int)std::floor(e.pos.z);
 e.inWater=isWater(w,x,(int)std::floor(e.pos.y+0.1f),z)||isWater(w,x,(int)std::floor(e.pos.y+e.h*0.5f),z);
 e.inLava=isLava(w,x,(int)std::floor(e.pos.y+0.1f),z);
}
Vec3 horizTo(const Vec3&a,const Vec3&b){ return Vec3{b.x-a.x,0,b.z-a.z}; }
float dist2d(const Vec3&a,const Vec3&b){ const float dx=a.x-b.x,dz=a.z-b.z; return std::sqrt(dx*dx+dz*dz); }
float dist3d(const Vec3&a,const Vec3&b){ return len2(Vec3{a.x-b.x,a.y-b.y,a.z-b.z}); }
bool losClear(World&w,Vec3 a,Vec3 b){ Vec3 d{b.x-a.x,b.y-a.y,b.z-a.z}; const float L=len2(d); if(L<0.01f)return true; const int n=(int)(L/0.4f); for(int i=1;i<n;i++){ const float t=float(i)/n; if(blockTop16(w,(int)std::floor(a.x+d.x*t),(int)std::floor(a.y+d.y*t),(int)std::floor(a.z+d.z*t))>=16)return false; } return true; }
bool skyExposed(World&w,const Vec3&p){ const int x=(int)std::floor(p.x),z=(int)std::floor(p.z); for(int y=(int)p.y+2;y<(int)p.y+48&&y<Chunk::HEIGHT-1;y++)if(blockTop16(w,x,y,z)>=16)return false; return true; }
// ground mover shared by the walking mobs: turn towards dir, accelerate to `want` blocks/s, hop over a block in the way
void walkToward(Entity&e,float dt,Vec3 dir,float want,float turn=8.f){
 const float L=std::sqrt(dir.x*dir.x+dir.z*dir.z);
 if(L>0.001f&&std::fabs(want)>0.001f){ dir.x/=L; dir.z/=L; e.yaw+=angDiff(e.yaw,yawToward(dir.x,dir.z))*std::min(1.f,dt*turn); const Vec3 f=fwd(e.yaw); const float sgn=want<0?-1.f:1.f; const float sp=std::fabs(want); e.vel.x+=(f.x*sp*sgn-e.vel.x)*std::min(1.f,dt*8.f); e.vel.z+=(f.z*sp*sgn-e.vel.z)*std::min(1.f,dt*8.f); }
 else if(e.t3<=0){ e.vel.x*=std::max(0.f,1.f-dt*10.f); e.vel.z*=std::max(0.f,1.f-dt*10.f); }
 if(e.t3>0)e.t3-=dt;
 if(e.wall&&e.onGround&&std::fabs(want)>0.001f)e.vel.y=7.2f; if(e.inWater)e.vel.y+=(e.vel.y<2.f?18.f:0.f)*dt;
}
void animWalk(Entity&e,float dt,float k=2.4f){ const float sp=std::sqrt(e.vel.x*e.vel.x+e.vel.z*e.vel.z); e.walkSpeed+=(std::min(1.f,sp*0.6f)-e.walkSpeed)*std::min(1.f,dt*10.f); e.walk+=sp*dt*k; }

// ---- minecart on rails: straight, ascending and curved shapes (the rail variant number is the shape, see Registry kind 21)
int railShapeOf(World&w,int x,int y,int z){ const Registry&R=Registry::get(); const uint16_t id=w.get(x,y,z); if(!id)return -1; const BlockId b=R.baseOf(id); const std::string&n=R.blockName(b); if(!(n.size()>=4&&n.compare(n.size()-4,4,"rail")==0))return -1; const int slot=R.variantIndex(id); return n=="rail"?slot:slot%6; }
void stepMinecart(Entity&e,World&w,float dt,EntityEnv&env){
 (void)env; int x=(int)std::floor(e.pos.x),z=(int)std::floor(e.pos.z),y=(int)std::floor(e.pos.y+0.15f); int shape=railShapeOf(w,x,y,z);
 if(shape<0&&railShapeOf(w,x,y-1,z)>=0){ y--; shape=railShapeOf(w,x,y,z); } else if(shape<0&&railShapeOf(w,x,y+1,z)>=0){ y++; shape=railShapeOf(w,x,y,z); }
 if(shape<0){ e.vel.y-=22.f*dt; e.vel.x*=std::max(0.f,1.f-dt*3.f); e.vel.z*=std::max(0.f,1.f-dt*3.f); moveBody(e,w,dt,0.f); return; }
 const uint16_t id=w.get(x,y,z); const bool powered=Registry::get().blockName(Registry::get().baseOf(id))=="powered_rail"&&Registry::get().variantIndex(id)>=6;
 float fx=e.pos.x-x,fz=e.pos.z-z; e.vel.y=0;
 static const int DX[4]={0,1,0,-1},DZ[4]={-1,0,1,0};   // N E S W
 if(shape>=6){   // curve: connections (S,E) (S,W) (N,W) (N,E)
  static const int C[4][2]={{2,1},{2,3},{0,3},{0,1}}; const int* cc=C[shape-6];
  const float sp=std::sqrt(e.vel.x*e.vel.x+e.vel.z*e.vel.z);
  if(sp>0.05f){ const int enteredFrom=std::fabs(e.vel.x)>std::fabs(e.vel.z)?(e.vel.x>0?3:1):(e.vel.z>0?0:2); int out=-1; if(enteredFrom==cc[0])out=cc[1]; else if(enteredFrom==cc[1])out=cc[0];
   if(out>=0&&std::fabs(fx-0.5f)<0.45f&&std::fabs(fz-0.5f)<0.45f){ e.vel.x=DX[out]*sp; e.vel.z=DZ[out]*sp; e.pos.x+=(x+0.5f-e.pos.x)*std::min(1.f,dt*6.f); e.pos.z+=(z+0.5f-e.pos.z)*std::min(1.f,dt*6.f); } }
  e.t1=std::fabs(e.vel.x)>std::fabs(e.vel.z)?1.f:0.f; e.pos.y=y+0.1f;
 } else {
  const bool alongX=(shape==1||shape==2||shape==3); e.t1=alongX?1.f:0.f; if(alongX){ e.vel.z=0; e.pos.z+=(z+0.5f-e.pos.z)*std::min(1.f,dt*8.f); } else { e.vel.x=0; e.pos.x+=(x+0.5f-e.pos.x)*std::min(1.f,dt*8.f); }
  float& v=alongX?e.vel.x:e.vel.z;
  if(shape==2)v-=4.5f*dt; else if(shape==3)v+=4.5f*dt; else if(shape==4)v+=4.5f*dt; else if(shape==5)v-=4.5f*dt;   // downhill
  if(powered&&std::fabs(v)>0.2f)v*=1.f+dt*0.9f; v*=std::max(0.f,1.f-dt*0.25f); if(std::fabs(v)>9.f)v=v>0?9.f:-9.f;
  float rise=0; fx=e.pos.x-x; fz=e.pos.z-z; if(shape==2)rise=fx; else if(shape==3)rise=1.f-fx; else if(shape==4)rise=1.f-fz; else if(shape==5)rise=fz; e.pos.y=y+0.1f+std::clamp(rise,0.f,1.f);
 }
 // advance; stop (bounce) at the end of the track
 const float nx=e.pos.x+e.vel.x*dt,nz=e.pos.z+e.vel.z*dt; const int cx=(int)std::floor(nx+(e.vel.x>0?0.45f:(e.vel.x<0?-0.45f:0.f))),cz=(int)std::floor(nz+(e.vel.z>0?0.45f:(e.vel.z<0?-0.45f:0.f)));
 if(cx!=x||cz!=z){ const bool ok=railShapeOf(w,cx,y,cz)>=0||railShapeOf(w,cx,y+1,cz)>=0||railShapeOf(w,cx,y-1,cz)>=0; if(!ok){ e.vel.x*=-0.2f; e.vel.z*=-0.2f; return; } }
 e.pos.x=nx; e.pos.z=nz; if(e.vel.x!=0||e.vel.z!=0)e.yaw=yawToward(e.vel.x,e.vel.z);
}
ItemId itemNamed(const char*n){ return Registry::get().itemByName(n); }
bool isNight(double t){ const long long m=(long long)t%24000; return m>=13000; }
}

Entity* EntityMgr::find(uint32_t id){ for(auto&e:list)if(e.id==id)return &e; return nullptr; }
int EntityMgr::index(uint32_t id)const{ for(size_t i=0;i<list.size();i++)if(list[i].id==id)return (int)i; return -1; }
bool EntityMgr::solidAt(const World&w,int x,int y,int z){ return blockTop16(const_cast<World&>(w),x,y,z)>0; }
float EntityMgr::surfaceY(const World&w,int x,int z,int fromY){
 World&ww=const_cast<World&>(w); for(int y=std::min(fromY,Chunk::HEIGHT-2);y>1;y--){ if(blockTop16(ww,x,y,z)>=16&&!blockTop16(ww,x,y+1,z)&&!blockTop16(ww,x,y+2,z)){ const uint16_t above=ww.get(x,y+1,z); if(above&&Registry::get().info(above).fluid)return -1.f; return float(y+1); } }
 return -1.f;
}

Entity& EntityMgr::spawn(EType t,const Vec3&pos){
 Entity e; e.id=nextId++; e.type=t; e.pos=pos; const Dims d=dimsOf(t); e.w=d.w; e.h=d.h; e.health=e.maxHealth=d.hp; e.yaw=rnd(0,2*PI); e.bodyYaw=e.yaw; e.t0=rnd(0.5f,3.f);
 if(t==EType::Dragon){ e.s0=0; e.t0=8.f; e.t3=rnd(0,6.28f); }
 list.push_back(e); return list.back();
}
void EntityMgr::spawnItem(const Vec3&pos,ItemId it,int count,const Vec3&vel,float pickupDelay){
 if(!it||count<=0)return;
 Entity&e=spawn(EType::Item,pos); e.item=it; e.count=count; e.vel=vel; e.pickupDelay=pickupDelay; e.yaw=0; e.health=1;
}
Entity* EntityMgr::raycast(const Vec3&o,const Vec3&d,float maxDist,float&tOut){
 Entity*best=nullptr; float bt=maxDist;
 for(auto&e:list){ if(e.dead||e.type==EType::Item||e.type==EType::Fireball||e.type==EType::DragonFireball||e.id==riddenId)continue;
  const float hw=e.w*0.5f; const float mn[3]={e.pos.x-hw,e.pos.y,e.pos.z-hw},mx[3]={e.pos.x+hw,e.pos.y+e.h,e.pos.z+hw}; const float oo[3]={o.x,o.y,o.z},dd[3]={d.x,d.y,d.z};
  float t0=0,t1=bt; bool hit=true; for(int a=0;a<3;a++){ if(std::fabs(dd[a])<1e-6f){ if(oo[a]<mn[a]||oo[a]>mx[a]){hit=false;break;} } else { float ta=(mn[a]-oo[a])/dd[a],tb=(mx[a]-oo[a])/dd[a]; if(ta>tb)std::swap(ta,tb); t0=std::max(t0,ta); t1=std::min(t1,tb); if(t0>t1){hit=false;break;} } }
  if(hit&&t0<bt){ bt=t0; best=&e; } }
 if(best)tOut=bt; return best;
}
float EntityMgr::dragonHealth()const{ for(auto&e:list)if(e.type==EType::Dragon&&!e.dead)return e.health; return 0; }
bool EntityMgr::dragonAlive()const{ for(auto&e:list)if(e.type==EType::Dragon&&!e.dead)return true; return false; }
Vec3 EntityMgr::ridePos(float alpha)const{ for(auto&e:list)if(e.id==riddenId){ const Vec3 p{e.prev.x+(e.pos.x-e.prev.x)*alpha,e.prev.y+(e.pos.y-e.prev.y)*alpha,e.prev.z+(e.pos.z-e.prev.z)*alpha}; return e.type==EType::Boat?Vec3{p.x,p.y+0.35f,p.z}:Vec3{p.x,p.y+0.1f,p.z}; } return Vec3{}; }
bool EntityMgr::serialize(const Entity&e,std::string&line){
 if(e.dead||e.type==EType::Fireball||e.type==EType::DragonFireball||e.type==EType::Arrow)return false;
 line=std::to_string((int)e.type)+" "+std::to_string(e.pos.x)+" "+std::to_string(e.pos.y)+" "+std::to_string(e.pos.z)+" "+std::to_string(e.yaw)+" "+std::to_string(e.health)+" "+std::to_string(e.variant)+" "+std::to_string((unsigned)e.item)+" "+std::to_string(e.count); return true;
}
bool EntityMgr::parse(const std::string&line,Entity&o){
 int t=0,variant=0,count=1; unsigned item=0; float x=0,y=0,z=0,yaw=0,hp=1; if(std::sscanf(line.c_str(),"%d %f %f %f %f %f %d %u %d",&t,&x,&y,&z,&yaw,&hp,&variant,&item,&count)<9)return false;
 if(t<0||t>=(int)EType::Count)return false; o.type=(EType)t; o.pos=Vec3{x,y,z}; o.yaw=yaw; o.health=hp; o.variant=variant; o.item=(ItemId)item; o.count=count; return true;
}

void EntityMgr::hurt(Entity&e,float amount,const Vec3&from,float knock,EntityEnv&env){
 if(e.dead||e.type==EType::Item)return; if(e.hurt>0.35f&&e.type!=EType::Crystal)return;   // brief invulnerability
 if(e.type==EType::Dragon){ amount*=(e.s0==3?1.f:0.5f); }
 e.health-=amount; e.hurt=0.5f;
 Vec3 away{e.pos.x-from.x,0,e.pos.z-from.z}; const float L=std::max(0.001f,len2(away)); away.x/=L; away.z/=L;
 if(e.type!=EType::Dragon&&e.type!=EType::Crystal&&e.type!=EType::Boat&&e.type!=EType::Minecart&&e.type!=EType::Blaze&&e.type!=EType::Ghast&&e.type!=EType::Arrow){ e.vel.x+=away.x*knock*4.f; e.vel.z+=away.z*knock*4.f; e.vel.y=std::max(e.vel.y,3.5f*knock); e.onGround=false; e.t3=0.3f; }
 if(e.type==EType::Wolf||e.type==EType::Spider){ e.s0=1; e.t1=20.f; }
 if(e.type==EType::Cow||e.type==EType::Pig||e.type==EType::Chicken||e.type==EType::Sheep||e.type==EType::Villager){ e.s0=2; e.t0=4.f; e.target=Vec3{e.pos.x+away.x*12,0,e.pos.z+away.z*12}; }
 if(e.type==EType::Enderman){ e.s0=1; e.t1=0; if(e.health>0&&e.health<e.maxHealth&&rnd(0,1)<0.7f){ e.t2=0.05f; } }
 if(env.sound){ const char*ev=e.type==EType::Zombie?"mob.zombie.hurt":e.type==EType::Skeleton?"mob.skeleton.hurt":e.type==EType::Spider?"mob.spider.say":e.type==EType::Sheep?"mob.sheep.say":e.type==EType::Ghast?"mob.ghast.scream":e.type==EType::Slime?"mob.slime.small":e.type==EType::Wolf?"mob.wolf.hurt":e.type==EType::Villager?"mob.villager.hit":e.type==EType::Cow?"mob.cow.hurt":e.type==EType::Pig?"mob.pig.say":e.type==EType::Chicken?"mob.chicken.hurt":e.type==EType::Creeper?"mob.creeper.say":e.type==EType::Enderman?"mob.endermen.hit":e.type==EType::Blaze?"mob.blaze.hit":e.type==EType::Dragon?"mob.enderdragon.hit":"random.pop"; env.sound(ev,e.pos,1.f,1.f); }
 if(e.type==EType::Crystal&&env.explode){ e.dead=true; env.explode(Vec3{e.pos.x,e.pos.y+1.f,e.pos.z},6.f,true,e.id); }
 if(e.health<=0&&!e.dead){
  if(e.type==EType::Dragon){ e.s0=5; e.t0=0; e.health=0; return; }
  e.dead=true; const Vec3 p{e.pos.x,e.pos.y+e.h*0.5f,e.pos.z};
  auto drop=[&](const char*n,int lo,int hi){ const ItemId id=itemNamed(n); const int c=rndi(lo,hi); if(id&&c>0)spawnItem(p,id,c,Vec3{rnd(-1,1),3.f,rnd(-1,1)}); };
  switch(e.type){ case EType::Cow: drop("leather",0,2); drop("beef",1,3); break; case EType::Pig: drop("porkchop",1,3); break; case EType::Chicken: drop("feather",0,2); drop("chicken",1,1); break;
   case EType::Creeper: drop("gunpowder",0,2); break; case EType::Enderman: drop("ender_pearl",0,1); break; case EType::Blaze: drop("blaze_rod",0,1); break;
   case EType::Zombie: drop("rotten_flesh",0,2); break; case EType::Skeleton: drop("bone",0,2); drop("arrow",0,2); break; case EType::Spider: drop("string",0,2); if(rndi(0,2)==0)drop("spider_eye",1,1); break;
   case EType::Sheep: drop("mutton",1,2); drop("white_wool",1,1); break; case EType::Ghast: drop("ghast_tear",0,1); drop("gunpowder",0,2); break;
   case EType::Slime: { if(e.variant<=1)drop("slime_ball",0,2); else { const int n=rndi(2,3); for(int k=0;k<n;k++){ Entity&s=spawn(EType::Slime,Vec3{e.pos.x+rnd(-0.5f,0.5f),e.pos.y+0.2f,e.pos.z+rnd(-0.5f,0.5f)}); s.variant=e.variant/2; s.w=0.52f*s.variant; s.h=0.52f*s.variant; s.health=s.maxHealth=float(s.variant*s.variant); s.vel=Vec3{rnd(-2,2),4.f,rnd(-2,2)}; } } break; }
   case EType::Boat: { const ItemId id=itemNamed("oak_boat"); if(id)spawnItem(p,id,1,Vec3{0,2,0}); break; } case EType::Minecart: { const ItemId id=itemNamed("minecart"); if(id)spawnItem(p,id,1,Vec3{0,2,0}); break; } default: break; }
  if(env.sound)env.sound(e.type==EType::Creeper?"mob.creeper.death":e.type==EType::Zombie?"mob.zombie.death":e.type==EType::Skeleton?"mob.skeleton.death":e.type==EType::Spider?"mob.spider.death":e.type==EType::Ghast?"mob.ghast.death":e.type==EType::Wolf?"mob.wolf.death":e.type==EType::Villager?"mob.villager.death":"random.pop",e.pos,1.f,0.8f);
 }
}

void EntityMgr::drive(float forward,float turn,bool jump,float dt,EntityEnv&env){
 (void)jump; Entity*e=find(riddenId); if(!e||e->dead)return;
 if(e->type==EType::Boat){ e->yaw+=-turn*1.9f*dt; const Vec3 f=fwd(e->yaw); const float acc=forward>0?9.f:(forward<0?-4.f:0.f); e->vel.x+=f.x*acc*dt; e->vel.z+=f.z*acc*dt; }
 else if(e->type==EType::Minecart){ const Vec3 f=fwd(env.playerYaw); const bool alongX=std::fabs(e->t1)>0.5f; const float a=alongX?(f.x>=0?1.f:-1.f):(f.z>=0?1.f:-1.f); const float acc=forward*a*6.f; if(alongX)e->vel.x+=acc*dt; else e->vel.z+=acc*dt; }
}

void EntityMgr::setupEnd(EntityEnv&env){
 if(!env.world||dragonSpawned||dragonDefeated)return; dragonSpawned=true; World&w=*env.world;
 const uint16_t obs=Registry::get().blockIdOrZero("obsidian");
 for(int i=0;i<10;i++){ const float a=i*PI*2/10.f; const int cx=(int)std::lround(42.f*std::cos(a)),cz=(int)std::lround(42.f*std::sin(a)); int bestY=-1,bx=cx,bz=cz;
  for(int dz=-3;dz<=3;dz++)for(int dx=-3;dx<=3;dx++){ for(int y=Chunk::HEIGHT-2;y>40;y--){ if(w.get(cx+dx,y,cz+dz)==obs&&obs){ if(y>bestY){bestY=y;bx=cx+dx;bz=cz+dz;} break; } if(w.get(cx+dx,y,cz+dz)!=0)break; } }
  if(bestY>0){ Entity&c=spawn(EType::Crystal,Vec3{(float)cx+0.5f,(float)bestY+1.f,(float)cz+0.5f}); c.yaw=0; (void)bx;(void)bz; } }
 if(!dragonDefeated){ float sy=surfaceY(w,0,0,Chunk::HEIGHT-2); if(sy<0)sy=130.f; Entity&d=spawn(EType::Dragon,Vec3{0.f,sy+30.f,60.f}); d.t0=6.f; d.t3=0; if(env.notice)env.notice("The Ender Dragon has awakened"); }
}

namespace{
void passiveAI(EntityMgr&m,Entity&e,float dt,EntityEnv&env){
 (void)m; const float speed=e.type==EType::Chicken?1.3f:(e.type==EType::Villager?0.8f:1.2f); float want=0; Vec3 dir{0,0,0};
 e.t0-=dt;
 if(e.s0==2){ if(e.t0<=0)e.s0=0; else{ dir=horizTo(e.pos,e.target); want=speed*2.4f; if(len2(dir)<1.f){ e.target=Vec3{e.pos.x+rnd(-12,12),0,e.pos.z+rnd(-12,12)}; } } }
 else if(e.s0==1){ dir=horizTo(e.pos,e.target); if(e.t0<=0||len2(dir)<0.6f){ e.s0=0; e.t0=rnd(1.f,5.f); } else want=speed; }
 else if(e.t0<=0){ if(rnd(0,1)<0.6f){ const float a=rnd(0,2*PI),d=rnd(3.f,8.f); e.target=Vec3{e.pos.x+std::cos(a)*d,0,e.pos.z+std::sin(a)*d}; e.s0=1; e.t0=rnd(3.f,7.f); } else e.t0=rnd(1.f,4.f); }
 if(e.t3>0){ e.t3-=dt; want=0; }
 const float L=len2(dir); if(L>0.001f&&want>0){ dir.x/=L; dir.z/=L; const float ty=yawToward(dir.x,dir.z); e.yaw+=angDiff(e.yaw,ty)*std::min(1.f,dt*8.f); const Vec3 f=fwd(e.yaw); e.vel.x+=(f.x*want-e.vel.x)*std::min(1.f,dt*8.f); e.vel.z+=(f.z*want-e.vel.z)*std::min(1.f,dt*8.f); }
 else if(e.t3<=0){ e.vel.x*=std::max(0.f,1.f-dt*10.f); e.vel.z*=std::max(0.f,1.f-dt*10.f); }
 if(e.wall&&e.onGround&&want>0)e.vel.y=7.2f;                        // jump over a block that is in the way
 if(e.inWater)e.vel.y+=(e.vel.y<2.f?18.f:0.f)*dt;
 if(e.type==EType::Chicken&&e.vel.y<-1.2f)e.vel.y=-1.2f;
 (void)env;
}
}

void EntityMgr::update(float dt,EntityEnv&env){
 if(!env.world)return; World&w=*env.world; dt=std::min(dt,0.1f);
 static float spawnT=0,healT=0;
 // ---------------- physics + AI
 for(size_t i=0;i<list.size();i++){
  Entity&e=list[i]; e.prev=e.pos; e.prevYaw=e.yaw; if(e.dead)continue; e.age+=dt; e.hurt=std::max(0.f,e.hurt-dt);
  const float dp=dist3d(e.pos,env.player); if(dp>env.activeRadius&&e.type!=EType::Dragon)continue;
  applyFluid(e,w);
  const bool flying=e.type==EType::Blaze||e.type==EType::Ghast||e.type==EType::Arrow||e.type==EType::Dragon||e.type==EType::Crystal||e.type==EType::Fireball||e.type==EType::DragonFireball;
  if(!flying){ e.vel.y-=(e.type==EType::Item?20.f:27.f)*dt; if(e.vel.y<-40.f)e.vel.y=-40.f; }
  e.onGround=false;
  switch(e.type){
   case EType::Item: {
    e.pickupDelay-=dt; if(e.inWater){ e.vel.y+=(e.vel.y<1.f?30.f:0.f)*dt; e.vel.x*=0.9f; e.vel.z*=0.9f; }
    moveBody(e,w,dt,0.f); const float fr=e.onGround?std::max(0.f,1.f-dt*8.f):1.f; e.vel.x*=fr; e.vel.z*=fr;
    if(e.age>300.f)e.dead=true;
    // merge with a neighbour of the same item
    for(size_t j=i+1;j<list.size();j++){ Entity&o=list[j]; if(o.dead||o.type!=EType::Item||o.item!=e.item||dist3d(o.pos,e.pos)>0.8f)continue; const int mx=Registry::get().maxStack(e.item); if(e.count+o.count<=mx){ e.count+=o.count; o.dead=true; e.age=std::min(e.age,100.f); } }
    if(e.pickupDelay<=0&&env.playerAlive&&env.givePlayer&&std::fabs(e.pos.x-env.player.x)<1.1f&&std::fabs(e.pos.z-env.player.z)<1.1f&&e.pos.y>env.player.y-0.6f&&e.pos.y<env.player.y+2.0f){
     { const int left=env.givePlayer(e.item,e.count); if(left<e.count&&env.sound)env.sound("random.pop",e.pos,0.5f,1.f+rnd(0,0.4f)); if(left<=0)e.dead=true; else e.count=left; } }
    break; }
   case EType::Cow: case EType::Pig: case EType::Chicken: case EType::Sheep: case EType::Villager: {
    passiveAI(*this,e,dt,env); moveBody(e,w,dt,0.6f); const float sp=std::sqrt(e.vel.x*e.vel.x+e.vel.z*e.vel.z); e.walkSpeed+=(std::min(1.f,sp*0.9f)-e.walkSpeed)*std::min(1.f,dt*10.f); e.walk+=sp*dt*2.6f;
    if(e.inLava)hurt(e,4.f*dt,e.pos,0,env); if(e.pos.y<2)e.dead=true; break; }
   case EType::Creeper: {
    const bool see=env.playerAlive&&env.playerSurvival&&dp<16.f&&losClear(w,Vec3{e.pos.x,e.pos.y+1.4f,e.pos.z},Vec3{env.player.x,env.player.y+1.2f,env.player.z});
    float want=0; Vec3 dir=horizTo(e.pos,env.player);
    if(see&&dist2d(e.pos,env.player)<3.0f){ e.t1+=dt; want=0; e.s0=2; if(e.t1>=1.5f&&env.explode){ e.dead=true; env.explode(Vec3{e.pos.x,e.pos.y+0.8f,e.pos.z},3.f,true,e.id); break; } }
    else { if(e.t1>0)e.t1=std::max(0.f,e.t1-dt*2.f); e.s0=see?1:0; if(see)want=3.6f; else { e.t0-=dt; if(e.t0<=0){ e.target=Vec3{e.pos.x+rnd(-6,6),0,e.pos.z+rnd(-6,6)}; e.t0=rnd(2,6); } dir=horizTo(e.pos,e.target); want=len2(dir)>1.f?1.2f:0.f; } }
    const float L=len2(dir); if(L>0.001f&&want>0){ dir.x/=L; dir.z/=L; e.yaw+=angDiff(e.yaw,yawToward(dir.x,dir.z))*std::min(1.f,dt*8.f); const Vec3 f=fwd(e.yaw); e.vel.x+=(f.x*want-e.vel.x)*std::min(1.f,dt*8.f); e.vel.z+=(f.z*want-e.vel.z)*std::min(1.f,dt*8.f); }
    else if(e.t3<=0){ e.vel.x*=std::max(0.f,1.f-dt*10.f); e.vel.z*=std::max(0.f,1.f-dt*10.f); }
    if(e.t3>0)e.t3-=dt; if(e.wall&&e.onGround&&want>0)e.vel.y=7.2f; if(e.inWater)e.vel.y+=(e.vel.y<2.f?18.f:0.f)*dt;
    moveBody(e,w,dt,0.6f); { const float sp=std::sqrt(e.vel.x*e.vel.x+e.vel.z*e.vel.z); e.walkSpeed+=(std::min(1.f,sp*0.5f)-e.walkSpeed)*std::min(1.f,dt*10.f); e.walk+=sp*dt*2.4f; }
    if(e.pos.y<2)e.dead=true; break; }
   case EType::Enderman: {
    // aggravated when the player looks at its head
    if(e.s0==0&&env.playerAlive&&env.playerSurvival&&dp<64.f){ const Vec3 eye{e.pos.x,e.pos.y+2.55f,e.pos.z}; Vec3 v{eye.x-env.player.x,eye.y-(env.player.y+env.playerEye),eye.z-env.player.z}; const float L=len2(v);
     if(L>0.1f){ v.x/=L;v.y/=L;v.z/=L; const float cp=std::cos(env.playerPitch),sp=std::sin(env.playerPitch); const Vec3 look{std::sin(env.playerYaw)*cp,sp,-std::cos(env.playerYaw)*cp};   // the player looks along (sin yaw, -cos yaw)
      if(v.x*look.x+v.y*look.y+v.z*look.z>0.985f&&losClear(w,Vec3{env.player.x,env.player.y+env.playerEye,env.player.z},eye)){ e.s0=1; if(env.sound)env.sound("mob.endermen.scream",e.pos,1.f,1.f); } } }
    e.t2-=dt; e.t0-=dt;
    float want=0; Vec3 dir{0,0,0};
    if(e.s0==1&&env.playerAlive&&env.playerSurvival&&dp<40.f){ dir=horizTo(e.pos,env.player); want=5.2f; e.t1+=dt;
      if(dist3d(e.pos,env.player)<2.2f&&e.hurt<=0.f&&e.t3<=0&&env.hurtPlayer){ env.hurtPlayer(7.f,e.pos,1.f); e.hurt=0.01f; e.t3=0; e.t1=0; }
      if(e.t2<=0&&rnd(0,1)<dt*0.35f)e.t2=0.01f; }
    else if(e.s0==1){ e.s0=0; }
    else { if(e.t0<=0){ e.target=Vec3{e.pos.x+rnd(-8,8),0,e.pos.z+rnd(-8,8)}; e.t0=rnd(3,9); } dir=horizTo(e.pos,e.target); want=len2(dir)>1.f?1.0f:0.f; }
    if(e.t2>0&&e.t2<0.2f&&env.world){ // teleport
      for(int tries=0;tries<12;tries++){ const float a=rnd(0,2*PI),d=rnd(6.f,16.f); const int tx=(int)std::floor(e.pos.x+std::cos(a)*d),tz=(int)std::floor(e.pos.z+std::sin(a)*d); const float sy=surfaceY(w,tx,tz,(int)e.pos.y+10);
       if(sy>0&&std::fabs(sy-e.pos.y)<12.f&&!isWater(w,tx,(int)sy,tz)){ if(env.sound)env.sound("mob.endermen.portal",e.pos,1.f,1.f); e.pos=Vec3{tx+0.5f,sy,tz+0.5f}; e.vel=Vec3{0,0,0}; if(env.sound)env.sound("mob.endermen.portal",e.pos,1.f,1.f); break; } }
      e.t2=-5.f; }
    if(e.inWater&&e.t2<=0.2f){ e.t2=0.01f; hurt(e,0.f,e.pos,0,env); }
    const float L=len2(dir); if(L>0.001f&&want>0){ dir.x/=L; dir.z/=L; e.yaw+=angDiff(e.yaw,yawToward(dir.x,dir.z))*std::min(1.f,dt*10.f); const Vec3 f=fwd(e.yaw); e.vel.x+=(f.x*want-e.vel.x)*std::min(1.f,dt*8.f); e.vel.z+=(f.z*want-e.vel.z)*std::min(1.f,dt*8.f); }
    else { e.vel.x*=std::max(0.f,1.f-dt*10.f); e.vel.z*=std::max(0.f,1.f-dt*10.f); }
    if(e.wall&&e.onGround&&want>0)e.vel.y=7.f; e.t3=std::max(0.f,e.t3-dt);
    moveBody(e,w,dt,0.6f); { const float sp=std::sqrt(e.vel.x*e.vel.x+e.vel.z*e.vel.z); e.walkSpeed+=(std::min(1.f,sp*0.5f)-e.walkSpeed)*std::min(1.f,dt*10.f); e.walk+=sp*dt*2.0f; }
    if(e.pos.y<2)e.dead=true; break; }
   case EType::Blaze: {
    const bool aware=env.playerAlive&&env.playerSurvival&&dp<30.f; Vec3 goal=e.pos;
    if(aware){ const float hd=dist2d(e.pos,env.player); Vec3 d=horizTo(e.pos,env.player); const float L=std::max(0.01f,len2(d)); d.x/=L; d.z/=L;
     const float want=hd>8.f?1.f:(hd<5.f?-0.6f:0.f); goal=Vec3{e.pos.x+d.x*want*5.f-d.z*std::sin(e.age*0.7f)*2.f,env.player.y+2.5f+std::sin(e.age*1.3f),e.pos.z+d.z*want*5.f+d.x*std::sin(e.age*0.7f)*2.f};
     e.yaw+=angDiff(e.yaw,yawToward(d.x,d.z))*std::min(1.f,dt*6.f);
     e.t1+=dt;   // fire three balls, a pause, repeat
     if(hd<26.f&&losClear(w,Vec3{e.pos.x,e.pos.y+1.f,e.pos.z},Vec3{env.player.x,env.player.y+1.2f,env.player.z})){ e.t2-=dt; if(e.t2<=0){ e.s1++; e.t2=(e.s1%3==0)?3.2f:0.45f; Entity&f=spawn(EType::Fireball,Vec3{e.pos.x,e.pos.y+1.f,e.pos.z}); Vec3 tv{env.player.x-f.pos.x,env.player.y+1.0f-f.pos.y,env.player.z-f.pos.z}; const float TL=std::max(0.01f,len2(tv)); f.vel=Vec3{tv.x/TL*11.f,tv.y/TL*11.f,tv.z/TL*11.f}; f.t0=5.f; f.variant=(int)e.id; if(env.sound)env.sound("mob.ghast.fireball",e.pos,1.f,1.f); } } }
    else { e.t0-=dt; if(e.t0<=0){ e.t0=rnd(2,5); e.target=Vec3{e.pos.x+rnd(-5,5),e.pos.y+rnd(-2,2),e.pos.z+rnd(-5,5)}; } goal=e.target; }
    Vec3 dv{goal.x-e.pos.x,goal.y-e.pos.y,goal.z-e.pos.z}; const float L=len2(dv); if(L>0.2f){ const float sp=std::min(3.5f,L*1.5f); e.vel.x+=(dv.x/L*sp-e.vel.x)*std::min(1.f,dt*3.f); e.vel.y+=(dv.y/L*sp-e.vel.y)*std::min(1.f,dt*3.f); e.vel.z+=(dv.z/L*sp-e.vel.z)*std::min(1.f,dt*3.f); }
    moveBody(e,w,dt,0.f); e.onGround=false; if(e.pos.y<2)e.dead=true; break; }
   case EType::Fireball: case EType::DragonFireball: {
    e.t0-=dt; e.pos.x+=e.vel.x*dt; e.pos.y+=e.vel.y*dt; e.pos.z+=e.vel.z*dt;
    const bool big=e.type==EType::DragonFireball; const float R=big?3.5f:0.7f;
    if(e.variant==2&&e.type==EType::Fireball){ const bool hit=e.t0<=0||blockTop16(w,(int)std::floor(e.pos.x),(int)std::floor(e.pos.y),(int)std::floor(e.pos.z))>0||(env.playerAlive&&env.playerSurvival&&dist3d(e.pos,Vec3{env.player.x,env.player.y+0.9f,env.player.z})<1.4f); if(hit){ e.dead=true; if(env.explode)env.explode(e.pos,1.2f,true,0); } break; }
    if(e.t0<=0||blockTop16(w,(int)std::floor(e.pos.x),(int)std::floor(e.pos.y),(int)std::floor(e.pos.z))>0){ e.dead=true; if(big&&env.explode){ if(dist3d(e.pos,env.player)<R+1.5f&&env.hurtPlayer)env.hurtPlayer(6.f,e.pos,0.5f); if(env.sound)env.sound("mob.enderdragon.growl",e.pos,1.f,1.f); } }
    else if(env.playerAlive&&env.playerSurvival&&dist3d(Vec3{e.pos.x,e.pos.y,e.pos.z},Vec3{env.player.x,env.player.y+0.9f,env.player.z})<R+0.5f){ e.dead=true; if(env.hurtPlayer)env.hurtPlayer(big?6.f:5.f,e.pos,0.6f); }
    break; }

   case EType::Zombie: case EType::Skeleton: case EType::Spider: case EType::Wolf: {
    const bool sees=env.playerAlive&&env.playerSurvival&&dp<(e.type==EType::Skeleton?20.f:28.f);
    const bool night=isNight(env.worldTime)||!skyExposed(w,e.pos);
    bool hostile=e.type==EType::Zombie||e.type==EType::Skeleton||(e.type==EType::Spider&&(night||e.s0==1))||(e.type==EType::Wolf&&e.s0==1);
    if(e.type==EType::Wolf||e.type==EType::Spider){ if(e.s0==1){ e.t1-=dt; if(e.t1<=0)e.s0=0; } }
    // zombies and skeletons burn in daylight
    if((e.type==EType::Zombie||e.type==EType::Skeleton)&&!isNight(env.worldTime)&&skyExposed(w,e.pos)&&!e.inWater&&!e.inLava){ e.t2+=dt; if(e.t2>=1.f){ e.t2=0; e.hurt=0; hurt(e,1.f,e.pos,0.f,env); if(e.dead)break; } }
    Vec3 dir=horizTo(e.pos,env.player); float want=0; const float hd=dist2d(e.pos,env.player);
    if(hostile&&sees){
     if(e.type==EType::Skeleton){
      const bool los=losClear(w,Vec3{e.pos.x,e.pos.y+1.5f,e.pos.z},Vec3{env.player.x,env.player.y+1.4f,env.player.z});
      e.t2-=dt; want=hd<6.f?-2.4f:(hd>12.f?2.6f:(los?std::sin(e.age*0.8f)*1.2f:2.f)); if(!los&&hd<=12.f)want=2.f;
      e.s0=(los&&e.t2<0.7f)?1:0;
      if(los&&hd<18.f&&e.t2<=0.f&&env.hurtPlayer){ e.t2=rnd(1.6f,2.4f); Entity&a=spawn(EType::Arrow,Vec3{e.pos.x,e.pos.y+1.5f,e.pos.z}); Vec3 tv{env.player.x-a.pos.x+rnd(-0.4f,0.4f),env.player.y+1.2f-a.pos.y+hd*0.06f,env.player.z-a.pos.z+rnd(-0.4f,0.4f)}; const float TL=std::max(0.01f,len2(tv)); a.vel=Vec3{tv.x/TL*17.f,tv.y/TL*17.f,tv.z/TL*17.f}; a.t0=6.f; a.yaw=yawToward(a.vel.x,a.vel.z); if(env.sound)env.sound("random.bow",e.pos,1.f,1.f); }
     } else {
      want=e.type==EType::Zombie?3.1f:(e.type==EType::Spider?4.4f:5.2f);
      if(hd<1.5f+(e.type==EType::Spider?0.4f:0.f)&&std::fabs(env.player.y-e.pos.y)<1.9f){ want=0.4f; e.t2-=dt; if(e.t2<=0.f&&env.hurtPlayer){ e.t2=1.0f; env.hurtPlayer(e.type==EType::Zombie?3.f:(e.type==EType::Spider?2.f:4.f),e.pos,0.7f); } }
      else e.t2=std::min(e.t2,0.4f);
      if(e.type==EType::Spider&&e.onGround&&hd>2.5f&&hd<5.5f&&rnd(0,1)<dt*1.5f){ const Vec3 f=fwd(e.yaw); e.vel.x=f.x*6.5f; e.vel.z=f.z*6.5f; e.vel.y=6.2f; }   // leap
     }
    } else {
     e.t0-=dt; if(e.t0<=0){ e.target=Vec3{e.pos.x+rnd(-7,7),0,e.pos.z+rnd(-7,7)}; e.t0=rnd(2,6); e.s1=rnd(0,1)<0.5f?1:0; } dir=horizTo(e.pos,e.target); want=(e.s1&&len2(dir)>1.f)?1.0f:0.f; }
    walkToward(e,dt,dir,want,e.type==EType::Skeleton&&hostile&&sees?10.f:8.f);
    if(e.type==EType::Spider&&e.wall&&want>0.f)e.vel.y=std::max(e.vel.y,3.2f);       // climbs walls
    moveBody(e,w,dt,0.6f); animWalk(e,dt,e.type==EType::Spider?3.f:2.4f);
    e.amb-=dt; if(e.amb<=0){ e.amb=rnd(9,22); if(env.sound&&dp<24.f)env.sound(e.type==EType::Zombie?"mob.zombie.say":e.type==EType::Skeleton?"mob.skeleton.say":e.type==EType::Spider?"mob.spider.say":"mob.wolf.growl",e.pos,0.7f,1.f); }
    if(e.pos.y<2)e.dead=true; break; }
   case EType::Slime: {
    e.t0-=dt; e.t2-=dt; const bool sees=env.playerAlive&&env.playerSurvival&&dp<16.f; const int size=std::max(1,e.variant);
    if(e.onGround&&e.t0<=0.f){ Vec3 dir=sees?horizTo(e.pos,env.player):Vec3{rnd(-1,1),0,rnd(-1,1)}; const float L=std::max(0.01f,len2(dir)); dir.x/=L; dir.z/=L; e.yaw=yawToward(dir.x,dir.z); e.vel.x=dir.x*(2.2f+size*0.45f); e.vel.z=dir.z*(2.2f+size*0.45f); e.vel.y=6.2f+size*0.25f; e.onGround=false; e.t0=rnd(0.7f,1.5f); if(env.sound&&dp<20.f)env.sound(size>1?"mob.slime.big":"mob.slime.small",e.pos,0.6f,1.f); }
    else if(e.onGround){ e.vel.x*=std::max(0.f,1.f-dt*12.f); e.vel.z*=std::max(0.f,1.f-dt*12.f); }
    if(sees&&size>=2&&e.t2<=0.f&&dist3d(Vec3{e.pos.x,e.pos.y+e.h*0.5f,e.pos.z},Vec3{env.player.x,env.player.y+0.9f,env.player.z})<e.w*0.5f+0.8f&&env.hurtPlayer){ e.t2=1.f; env.hurtPlayer(size>=4?4.f:2.f,e.pos,0.8f); }
    if(e.inWater)e.vel.y+=(e.vel.y<2.f?18.f:0.f)*dt;
    moveBody(e,w,dt,0.f); if(e.pos.y<2)e.dead=true; break; }
   case EType::Ghast: {
    const bool aware=env.playerAlive&&env.playerSurvival&&dp<60.f; Vec3 goal=e.pos; const bool los=aware&&losClear(w,Vec3{e.pos.x,e.pos.y,e.pos.z},Vec3{env.player.x,env.player.y+1.2f,env.player.z});
    if(aware){ Vec3 d=horizTo(e.pos,env.player); const float L=std::max(0.01f,len2(d)); d.x/=L; d.z/=L; e.yaw+=angDiff(e.yaw,yawToward(d.x,d.z))*std::min(1.f,dt*3.f);
     const float want=L>24.f?1.f:(L<14.f?-1.f:0.f); goal=Vec3{e.pos.x+d.x*want*8.f-d.z*std::sin(e.age*0.3f)*4.f,env.player.y+8.f+std::sin(e.age*0.4f)*3.f,e.pos.z+d.z*want*8.f+d.x*std::sin(e.age*0.3f)*4.f};
     if(los&&L<48.f){ e.t2-=dt; if(e.t2<=0.f){ e.t2=3.4f; Entity&f=spawn(EType::Fireball,Vec3{e.pos.x,e.pos.y+0.5f,e.pos.z}); f.variant=2; f.t0=6.f; Vec3 tv{env.player.x-f.pos.x,env.player.y+0.9f-f.pos.y,env.player.z-f.pos.z}; const float TL=std::max(0.01f,len2(tv)); f.vel=Vec3{tv.x/TL*10.f,tv.y/TL*10.f,tv.z/TL*10.f}; if(env.sound)env.sound("mob.ghast.fireball",e.pos,1.5f,1.f); } } }
    else { e.t0-=dt; if(e.t0<=0){ e.t0=rnd(3,7); e.target=Vec3{e.pos.x+rnd(-8,8),e.pos.y+rnd(-3,3),e.pos.z+rnd(-8,8)}; } goal=e.target; }
    Vec3 dv{goal.x-e.pos.x,goal.y-e.pos.y,goal.z-e.pos.z}; const float L=len2(dv); if(L>0.3f){ const float sp=std::min(2.6f,L); e.vel.x+=(dv.x/L*sp-e.vel.x)*std::min(1.f,dt*1.5f); e.vel.y+=(dv.y/L*sp-e.vel.y)*std::min(1.f,dt*1.5f); e.vel.z+=(dv.z/L*sp-e.vel.z)*std::min(1.f,dt*1.5f); }
    moveBody(e,w,dt,0.f); e.onGround=false; e.amb-=dt; if(e.amb<=0){ e.amb=rnd(8,16); if(env.sound&&dp<40.f)env.sound("mob.ghast.moan",e.pos,1.f,1.f); } if(e.pos.y<2)e.dead=true; break; }
   case EType::Arrow: {
    e.t0-=dt; e.vel.y-=14.f*dt; e.pos.x+=e.vel.x*dt; e.pos.y+=e.vel.y*dt; e.pos.z+=e.vel.z*dt; e.yaw=yawToward(e.vel.x,e.vel.z);
    if(e.t0<=0||blockTop16(w,(int)std::floor(e.pos.x),(int)std::floor(e.pos.y),(int)std::floor(e.pos.z))>0){ e.dead=true; }
    else if(env.playerAlive&&env.playerSurvival&&std::fabs(e.pos.x-env.player.x)<0.45f&&std::fabs(e.pos.z-env.player.z)<0.45f&&e.pos.y>env.player.y-0.1f&&e.pos.y<env.player.y+1.9f){ e.dead=true; if(env.hurtPlayer)env.hurtPlayer(3.5f,Vec3{e.pos.x-e.vel.x,e.pos.y,e.pos.z-e.vel.z},0.4f); }
    break; }
   case EType::Crystal: { e.yaw=0; break; }
   case EType::Dragon: {
    e.age+=0; const Vec3 center{0.f,e.t1>0?0.f:0.f,0.f}; (void)center;
    float cy=130.f; { const float sy=surfaceY(w,0,0,Chunk::HEIGHT-2); if(sy>0)cy=sy; }
    if(e.s0==5){ e.t0+=dt; e.vel=Vec3{0,-1.5f,0}; e.pos.y+=e.vel.y*dt; e.pitch=0.4f; e.yaw+=dt*0.5f;
      if(e.t0>5.f){ e.dead=true; dragonDefeated=true; // exit portal + egg
        const int py=(int)cy-1; auto set=[&](int x,int y,int z,const char*n){ const uint16_t id=Registry::get().blockIdOrZero(n); if(id&&env.setBlock)env.setBlock(x,y,z,id); };
        for(int dx=-2;dx<=2;dx++)for(int dz=-2;dz<=2;dz++){ const int d2=dx*dx+dz*dz; if(d2<=5){ set(dx,py,dz,"bedrock"); if(d2<=2)set(dx,py+1,dz,"end_portal"); } else if(d2<=8)set(dx,py,dz,"bedrock"); }
        for(int y=py+1;y<=py+4;y++)set(0,y,0,"bedrock"); set(0,py+5,0,"dragon_egg"); set(1,py+2,0,"torch"); set(-1,py+2,0,"torch"); set(0,py+2,1,"torch"); set(0,py+2,-1,"torch");
        if(env.notice)env.notice("You have defeated the Ender Dragon!"); if(env.explode)env.explode(Vec3{0.5f,(float)py+3,0.5f},0.f,false,e.id); }
      break; }
    // crystals heal it
    healT+=dt; if(healT>=1.f){ healT=0; for(auto&c:list)if(c.type==EType::Crystal&&!c.dead&&dist3d(c.pos,e.pos)<60.f&&e.health<e.maxHealth)e.health=std::min(e.maxHealth,e.health+1.f); }
    e.t0-=dt; Vec3 goal=e.pos; float speed=13.f;
    int crystals=0; for(auto&c:list)if(c.type==EType::Crystal&&!c.dead)crystals++;
    if(e.s0==0){            // circle the island, strafing the player now and then
      e.t3+=dt*0.28f; goal=Vec3{std::cos(e.t3)*55.f,cy+28.f+std::sin(e.t3*2.f)*6.f,std::sin(e.t3)*55.f};
      if(e.t0<=0){ const float r=rnd(0,1); if(crystals==0&&r<0.45f){ e.s0=3-0; e.s0=6; e.t0=40.f; } else if(r<0.55f){ e.s0=2; e.t0=7.f; } else if(r<0.85f){ e.s0=1; e.t0=8.f; } else e.t0=rnd(6,10); } }
    else if(e.s0==1){       // strafe: fly over the player and spit fire
      goal=Vec3{env.player.x,env.player.y+14.f,env.player.z}; speed=12.f; e.t2-=dt;
      if(dist2d(e.pos,env.player)<28.f&&e.t2<=0){ e.t2=2.2f; Entity&f=spawn(EType::DragonFireball,Vec3{e.pos.x,e.pos.y,e.pos.z}); Vec3 tv{env.player.x-f.pos.x,env.player.y-f.pos.y,env.player.z-f.pos.z}; const float TL=std::max(0.01f,len2(tv)); f.vel=Vec3{tv.x/TL*14.f,tv.y/TL*14.f,tv.z/TL*14.f}; f.t0=5.f; e.t1=0.8f; if(env.sound)env.sound("mob.enderdragon.growl",e.pos,2.f,1.f); }
      if(e.t0<=0){ e.s0=0; e.t0=rnd(6,10); } }
    else if(e.s0==2){       // charge
      goal=Vec3{env.player.x,env.player.y+1.f,env.player.z}; speed=22.f;
      if(dist3d(e.pos,env.player)<7.f&&env.hurtPlayer&&e.t2<=0){ env.hurtPlayer(10.f,e.pos,2.f); e.t2=1.5f; e.s0=0; e.t0=rnd(5,9); }
      e.t2-=dt; if(e.t0<=0){ e.s0=0; e.t0=rnd(5,9); } }
    else if(e.s0==6){       // fly to the fountain and land
      goal=Vec3{0.f,cy+6.f,0.f}; speed=11.f; if(dist3d(e.pos,goal)<4.f){ e.s0=3; e.t0=18.f; if(env.notice)env.notice("The Ender Dragon has landed"); } }
    else if(e.s0==3){       // perched: vulnerable, breathes at the player
      goal=Vec3{0.f,cy+4.f,0.f}; speed=6.f; e.t2-=dt;
      if(dist2d(e.pos,env.player)<20.f&&e.t2<=0){ e.t2=1.6f; Entity&f=spawn(EType::DragonFireball,Vec3{e.pos.x,e.pos.y+2,e.pos.z-3}); Vec3 tv{env.player.x-f.pos.x,env.player.y-f.pos.y,env.player.z-f.pos.z}; const float TL=std::max(0.01f,len2(tv)); f.vel=Vec3{tv.x/TL*12.f,tv.y/TL*12.f,tv.z/TL*12.f}; f.t0=4.f; e.t1=0.8f; }
      if(e.t0<=0){ e.s0=0; e.t0=rnd(6,10); } }
    e.t1=std::max(0.f,e.t1-dt);
    Vec3 dv{goal.x-e.pos.x,goal.y-e.pos.y,goal.z-e.pos.z}; const float L=std::max(0.01f,len2(dv)); const float sp=std::min(speed,L*1.2f+2.f);
    e.vel.x+=(dv.x/L*sp-e.vel.x)*std::min(1.f,dt*2.f); e.vel.y+=(dv.y/L*sp-e.vel.y)*std::min(1.f,dt*2.f); e.vel.z+=(dv.z/L*sp-e.vel.z)*std::min(1.f,dt*2.f);
    e.pos.x+=e.vel.x*dt; e.pos.y+=e.vel.y*dt; e.pos.z+=e.vel.z*dt;
    if(std::fabs(e.vel.x)+std::fabs(e.vel.z)>0.5f){ const float ty=yawToward(e.vel.x,e.vel.z); e.yaw+=angDiff(e.yaw,ty)*std::min(1.f,dt*2.5f); }   // head first
    e.pitch=std::clamp(-e.vel.y*0.03f,-0.4f,0.4f);
    // hit the player when it flies through them
    if(env.playerAlive&&env.playerSurvival&&dist3d(Vec3{e.pos.x,e.pos.y+1,e.pos.z},env.player)<4.5f&&e.t2<=0&&env.hurtPlayer&&e.s0!=2){ env.hurtPlayer(6.f,e.pos,1.5f); e.t2=1.f; }
    break; }
   case EType::Boat: {
    const int bx=(int)std::floor(e.pos.x),bz=(int)std::floor(e.pos.z); const bool inW=isWater(w,bx,(int)std::floor(e.pos.y+0.2f),bz); const bool above=isWater(w,bx,(int)std::floor(e.pos.y+0.7f),bz);
    if(inW){ e.vel.y+=(above?9.f:22.f)*dt; e.vel.y*=std::max(0.f,1.f-dt*3.f); } else e.vel.y-=22.f*dt;
    const float fr=inW?std::max(0.f,1.f-dt*0.9f):std::max(0.f,1.f-dt*6.f); e.vel.x*=fr; e.vel.z*=fr; { const float sp=std::sqrt(e.vel.x*e.vel.x+e.vel.z*e.vel.z); if(sp>7.f){ e.vel.x*=7.f/sp; e.vel.z*=7.f/sp; } }
    moveBody(e,w,dt,0.f); if(e.rider&&env.playerRiding==false)e.rider=0; if(e.pos.y<2)e.dead=true; break; }
   case EType::Minecart: { stepMinecart(e,w,dt,env); if(e.rider&&env.playerRiding==false)e.rider=0; if(e.pos.y<2)e.dead=true; break; }
   default: break; }
 }
 // ---------------- spawning (every couple of seconds) and clean-up
 spawnT+=dt;
 if(spawnT>=2.f&&env.playerAlive){ spawnT=0;
  int passive=0,creepers=0,enders=0,blazes=0,ghasts=0; for(auto&e:list){ if(e.dead)continue; const float d=dist3d(e.pos,env.player); if(d>80.f)continue; if(e.type==EType::Cow||e.type==EType::Pig||e.type==EType::Chicken||e.type==EType::Sheep)passive++; else if(e.type==EType::Ghast)ghasts++; else if(e.type==EType::Creeper||e.type==EType::Zombie||e.type==EType::Skeleton||e.type==EType::Spider||e.type==EType::Slime)creepers++; else if(e.type==EType::Enderman)enders++; else if(e.type==EType::Blaze)blazes++; }
  const Registry&R=Registry::get(); const uint16_t grass=R.blockIdOrZero("grass_block"),nbricks=R.blockIdOrZero("nether_bricks"),endstone=R.blockIdOrZero("end_stone");
  auto pickSpot=[&](float dmin,float dmax,Vec3&out)->bool{ for(int t=0;t<6;t++){ const float a=rnd(0,2*PI),d=rnd(dmin,dmax); const int x=(int)std::floor(env.player.x+std::cos(a)*d),z=(int)std::floor(env.player.z+std::sin(a)*d); const float sy=surfaceY(w,x,z,(int)env.player.y+24); if(sy<0||std::fabs(sy-env.player.y)>24.f)continue; out=Vec3{x+0.5f,sy,z+0.5f}; return true; } return false; };
  if(env.dim==0){
   if(passive<14&&!isNight(env.worldTime)){ Vec3 p; if(pickSpot(20.f,48.f,p)){ const uint16_t below=w.get((int)std::floor(p.x),(int)p.y-1,(int)std::floor(p.z)); if(below==grass&&grass){ const int n=rndi(2,4); const int k=rndi(0,15); const EType ty=k<4?EType::Cow:(k<8?EType::Pig:(k<11?EType::Chicken:(k<15?EType::Sheep:EType::Wolf))); for(int i=0;i<n;i++){ Vec3 q=p; q.x+=rnd(-2,2); q.z+=rnd(-2,2); const float sy=surfaceY(w,(int)std::floor(q.x),(int)std::floor(q.z),(int)p.y+3); if(sy>0&&std::fabs(sy-p.y)<2.f){ q.y=sy; spawn(ty,q); } } } } }
   if(creepers<9){ Vec3 p; if(pickSpot(24.f,44.f,p)){ bool sky=true; for(int y=(int)p.y+2;y<(int)p.y+40&&y<Chunk::HEIGHT-1;y++){ if(blockTop16(w,(int)std::floor(p.x),y,(int)std::floor(p.z))>=16){ sky=false; break; } }
      if((sky&&isNight(env.worldTime))||(!sky&&rnd(0,1)<0.35f)){ const float r=rnd(0,1); if(!sky&&r<0.14f){ Entity&s=spawn(EType::Slime,p); s.variant=(rnd(0,1)<0.5f?1:2); s.w=0.52f*s.variant; s.h=0.52f*s.variant; s.health=s.maxHealth=float(s.variant*s.variant); } else spawn(r<0.28f?EType::Creeper:(r<0.62f?EType::Zombie:(r<0.86f?EType::Skeleton:EType::Spider)),p); } } } }
  else if(env.dim==1){ if(ghasts<3&&rnd(0,1)<0.35f){ for(int t=0;t<6;t++){ const float a=rnd(0,2*PI),d=rnd(28.f,52.f); Vec3 q{env.player.x+std::cos(a)*d,env.player.y+rnd(2.f,16.f),env.player.z+std::sin(a)*d}; if(q.y>20&&q.y<Chunk::HEIGHT-30&&!boxHits(w,q,2.4f,4.2f)){ spawn(EType::Ghast,q); break; } } }
   if(blazes<5){ Vec3 p; if(pickSpot(12.f,40.f,p)){ const uint16_t below=w.get((int)std::floor(p.x),(int)p.y-1,(int)std::floor(p.z)); if(below==nbricks&&nbricks){ spawn(EType::Blaze,Vec3{p.x,p.y+1.5f,p.z}); } } } }
  else if(env.dim==2){ if(enders<8){ Vec3 p; if(pickSpot(24.f,48.f,p)){ const uint16_t below=w.get((int)std::floor(p.x),(int)p.y-1,(int)std::floor(p.z)); if(below==endstone&&endstone)spawn(EType::Enderman,p); } } }
  // despawn far hostile mobs / vanished items
  for(auto&e:list){ if(e.dead)continue; const float d=dist3d(e.pos,env.player); if((e.type==EType::Creeper||e.type==EType::Enderman||e.type==EType::Blaze||e.type==EType::Zombie||e.type==EType::Skeleton||e.type==EType::Spider||e.type==EType::Slime||e.type==EType::Ghast)&&d>96.f)e.dead=true; if((e.type==EType::Cow||e.type==EType::Pig||e.type==EType::Chicken||e.type==EType::Sheep)&&d>140.f)e.dead=true; }
 }
 list.erase(std::remove_if(list.begin(),list.end(),[&](const Entity&e){ if(e.dead&&e.id==riddenId)riddenId=0; return e.dead; }),list.end());
}
