// Entity models: vanilla style box models (texture offsets as in the vanilla model classes) built into the BLOCK vertex format.
// Every face of every box is baked into a 16x16 tile of the block atlas at start-up (TexturePack), so the mobs are drawn by the same pipeline as the terrain,
// with positions quantised to 1/16 block (one pixel, like the textures).
#include "Entity.hpp"
#include "TexturePack.hpp"
#include <array>
#include <cmath>
#include <map>

namespace{
const float PI=3.14159265f;
struct EPart{ float px=0,py=0,pz=0,rx=0,ry=0,rz=0,s=1; int parent=-1; };
struct EBox{ int part=0; float x=0,y=0,z=0,w=0,h=0,d=0; int tu=0,tv=0; bool mirror=false; bool alt=false; float grow=0; };
struct EModelDef{ std::string name,tex,tex2; int tw=64,th=32; float baseY=24,yawOff=0; bool hurtTiles=true; std::vector<EPart> parts; std::vector<EBox> boxes; std::vector<std::array<uint16_t,6>> tiles,tilesH,tilesF; };
struct PartPose{ float dx=0,dy=0,dz=0,rx=0,ry=0,rz=0; };

std::array<EModelDef,(size_t)EType::Count> g_models; bool g_built=false;
int part(EModelDef&m,float px,float py,float pz,float rx=0,float ry=0,float rz=0,int parent=-1,float s=1){ EPart p; p.px=px;p.py=py;p.pz=pz;p.rx=rx;p.ry=ry;p.rz=rz;p.parent=parent;p.s=s; m.parts.push_back(p); return (int)m.parts.size()-1; }
void box(EModelDef&m,int pt,float x,float y,float z,float w,float h,float d,int tu,int tv,bool mirror=false,float grow=0,bool alt=false){ EBox b; b.part=pt;b.x=x;b.y=y;b.z=z;b.w=w;b.h=h;b.d=d;b.tu=tu;b.tv=tv;b.mirror=mirror;b.grow=grow;b.alt=alt; m.boxes.push_back(b); }

void buildModels(){
 if(g_built)return; g_built=true;
 // ---------------- cow (64x64 pack texture, vanilla offsets)
 { EModelDef&m=g_models[(size_t)EType::Cow]; m.name="cow"; m.tex="entity/cow/cow_temperate.png"; m.tw=64; m.th=64;
   int head=part(m,0,4,-8),body=part(m,0,5,2,PI/2),l0=part(m,-4,12,7),l1=part(m,4,12,7),l2=part(m,-4,12,-6),l3=part(m,4,12,-6);
   box(m,head,-4,-4,-6,8,8,6,0,0); box(m,head,-5,-5,-5,1,3,1,22,0); box(m,head,4,-5,-5,1,3,1,22,0,true);
   box(m,body,-6,-10,-7,12,18,10,18,4); box(m,body,-2,2,-8,4,6,1,52,0);
   for(int l:{l0,l1,l2,l3})box(m,l,-2,0,-2,4,12,4,0,16); }
 // ---------------- pig
 { EModelDef&m=g_models[(size_t)EType::Pig]; m.name="pig"; m.tex="entity/pig/pig_temperate.png"; m.tw=64; m.th=64;
   int head=part(m,0,12,-6),body=part(m,0,11,2,PI/2),l0=part(m,-3,18,7),l1=part(m,3,18,7),l2=part(m,-3,18,-5),l3=part(m,3,18,-5);
   box(m,head,-4,-4,-8,8,8,8,0,0); box(m,head,-2,0,-9,4,3,1,16,16); box(m,body,-5,-10,-7,10,16,8,28,8);
   for(int l:{l0,l1,l2,l3})box(m,l,-2,0,-2,4,6,4,0,16); }
 // ---------------- chicken
 { EModelDef&m=g_models[(size_t)EType::Chicken]; m.name="chicken"; m.tex="entity/chicken/chicken_temperate.png";
   int head=part(m,0,15,-4),body=part(m,0,16,0,PI/2),l0=part(m,-2,19,1),l1=part(m,1,19,1),w0=part(m,-4,13,0),w1=part(m,4,13,0);
   box(m,head,-2,-6,-2,4,6,3,0,0); box(m,head,-2,-4,-4,4,2,2,14,0); box(m,head,-1,-2,-3,2,2,2,14,4); box(m,body,-3,-4,-3,6,8,6,0,9);
   box(m,l0,-1,0,-3,3,5,3,26,0); box(m,l1,-1,0,-3,3,5,3,26,0); box(m,w0,0,0,-3,1,4,6,24,13); box(m,w1,-1,0,-3,1,4,6,24,13); }
 // ---------------- creeper
 { EModelDef&m=g_models[(size_t)EType::Creeper]; m.name="creeper"; m.tex="entity/creeper/creeper.png";
   int head=part(m,0,6,0),body=part(m,0,6,0),l0=part(m,-2,18,4),l1=part(m,2,18,4),l2=part(m,-2,18,-4),l3=part(m,2,18,-4);
   box(m,head,-4,-8,-4,8,8,8,0,0); box(m,body,-4,0,-2,8,12,4,16,16); for(int l:{l0,l1,l2,l3})box(m,l,-2,0,-2,4,6,4,0,16); }
 // ---------------- enderman
 { EModelDef&m=g_models[(size_t)EType::Enderman]; m.name="enderman"; m.tex="entity/enderman/enderman.png";
   int head=part(m,0,-14,0),body=part(m,0,-14,0),a0=part(m,-5,-12,0),a1=part(m,5,-12,0),l0=part(m,-2,-6,0),l1=part(m,2,-6,0);
   box(m,head,-4,-8,-4,8,8,8,0,0); box(m,body,-4,0,-2,8,12,4,32,16); box(m,a0,-1,-2,-1,2,30,2,56,0); box(m,a1,-1,-2,-1,2,30,2,56,0,true);
   box(m,l0,-1,0,-1,2,30,2,56,0); box(m,l1,-1,0,-1,2,30,2,56,0,true); }
 // ---------------- blaze: head + 12 rods (positions animated)
 { EModelDef&m=g_models[(size_t)EType::Blaze]; m.name="blaze"; m.tex="entity/blaze/blaze.png"; m.baseY=22;
   int head=part(m,0,0,0); box(m,head,-4,-4,-4,8,8,8,0,0);
   for(int i=0;i<12;i++){ int r=part(m,0,0,0); box(m,r,0,0,0,2,8,2,0,16); } }
 // ---------------- end crystal (128x64): base + glass cubes (scaled)
 { EModelDef&m=g_models[(size_t)EType::Crystal]; m.name="end_crystal"; m.tex="entity/end_crystal/end_crystal.png"; m.tw=128; m.th=64; m.hurtTiles=false; m.baseY=0;
   int base=part(m,0,-2,0); box(m,base,-6,0,-6,12,4,12,0,16);
   int glass=part(m,0,-20,0,0,0,0,-1,2.0f); box(m,glass,-4,-4,-4,8,8,8,32,0);
   int core=part(m,0,-20,0,0,0,0,-1,1.45f); box(m,core,-4,-4,-4,8,8,8,32,16); }
 // ---------------- boat (128x64): hull + 4 sides
 { EModelDef&m=g_models[(size_t)EType::Boat]; m.name="boat"; m.tex="entity/boat/oak.png"; m.tw=128; m.th=64; m.hurtTiles=false; m.baseY=14; m.yawOff=PI/2;
   int bottom=part(m,0,3,1,PI/2),back=part(m,-15,4,4,0,3*PI/2),front=part(m,15,4,0,0,PI/2),right=part(m,0,4,-9,0,PI),left=part(m,0,4,9);
   box(m,bottom,-14,-9,-3,28,16,3,0,0); box(m,back,-13,-7,-1,18,6,2,0,19); box(m,front,-8,-7,-1,16,6,2,0,27); box(m,right,-14,-7,-1,28,6,2,0,35); box(m,left,-14,-7,-1,28,6,2,0,43); }
 // ---------------- minecart (64x32)
 { EModelDef&m=g_models[(size_t)EType::Minecart]; m.name="minecart"; m.tex="entity/minecart/minecart.png"; m.hurtTiles=false; m.baseY=13;
   int bottom=part(m,0,4,0,PI/2),front=part(m,-9,4,0,0,3*PI/2),back=part(m,9,4,0,0,PI/2),left=part(m,0,4,-7,0,PI),right=part(m,0,4,7);
   box(m,bottom,-10,-8,-1,20,16,2,0,10); box(m,front,-8,-9,-1,16,8,2,0,0); box(m,back,-8,-9,-1,16,8,2,0,0); box(m,left,-8,-9,-1,16,8,2,0,0); box(m,right,-8,-9,-1,16,8,2,0,0); }
 // ---------------- ender dragon (256x256)
 { EModelDef&m=g_models[(size_t)EType::Dragon]; m.name="dragon"; m.tex="entity/enderdragon/dragon.png"; m.tw=256; m.th=256; m.hurtTiles=false; m.baseY=56; m.yawOff=0;
   // body (part 0), head (1), jaw (2, child of head), neck 5 (3..7), tail 12 (8..19), wings 2 x (bone + membrane) (20,21), legs
   int body=part(m,0,0,0); box(m,body,-12,0,-16,24,24,64,0,0); box(m,body,-1,-6,10,2,6,12,220,53); box(m,body,-1,-6,26,2,6,12,220,53); box(m,body,-1,-6,42,2,6,12,220,53);
   int head=part(m,0,0,0); box(m,head,-6,-1,-24,12,5,16,176,44); box(m,head,-8,-8,-10,16,16,16,112,30);
   int jaw=part(m,0,0,0,0,0,0,head); box(m,jaw,-6,0,-16,12,4,16,176,65);
   for(int i=0;i<5;i++){ int n=part(m,0,0,0); box(m,n,-5,-5,-5,10,10,10,192,104); box(m,n,-1,-9,-3,2,4,6,48,0); }
   for(int i=0;i<12;i++){ int t=part(m,0,0,0); box(m,t,-5,-5,-5,10,10,10,192,104); box(m,t,-1,-9,-3,2,4,6,48,0); }
   int wl=part(m,12,5,2),wr=part(m,-12,5,2);
   box(m,wl,0,-4,-4,56,8,8,112,88,true); box(m,wl,0,0,2,56,0,56,-56,88,true); box(m,wr,-56,-4,-4,56,8,8,112,88); box(m,wr,-56,0,2,56,0,56,-56,88);
   // legs: front pair (2 parts each: leg, tibia+foot) and hind pair
   for(int side=0;side<2;side++){ const float sx=side?-12:12; int f=part(m,sx,16,-2); box(m,f,-4,-8,-4,8,24,8,112,104,side==0); box(m,f,-3,16,-3,6,24,6,226,138,side==0); box(m,f,-4,40,-12,8,4,16,144,104,side==0);
    int h=part(m,sx,16,40); box(m,h,-8,-4,-8,16,32,16,0,0,side==0); box(m,h,-6,28,0,12,32,12,196,0,side==0); box(m,h,-9,60,-20,18,6,24,112,0,side==0); } }

 // ---------------- zombie (humanoid, 64x64 layout)
 { EModelDef&m=g_models[(size_t)EType::Zombie]; m.name="zombie"; m.tex="entity/zombie/zombie.png"; m.tw=64; m.th=64;
   int head=part(m,0,0,0),body=part(m,0,0,0),ra=part(m,-5,2,0),la=part(m,5,2,0),rl=part(m,-1.9f,12,0),ll=part(m,1.9f,12,0);
   box(m,head,-4,-8,-4,8,8,8,0,0); box(m,body,-4,0,-2,8,12,4,16,16); box(m,ra,-3,-2,-2,4,12,4,40,16); box(m,la,-1,-2,-2,4,12,4,32,48); box(m,rl,-2,0,-2,4,12,4,0,16); box(m,ll,-2,0,-2,4,12,4,16,48); }
 // ---------------- skeleton (64x32)
 { EModelDef&m=g_models[(size_t)EType::Skeleton]; m.name="skeleton"; m.tex="entity/skeleton/skeleton.png"; m.tw=64; m.th=32;
   int head=part(m,0,0,0),body=part(m,0,0,0),ra=part(m,-5,2,0),la=part(m,5,2,0),rl=part(m,-2,12,0),ll=part(m,2,12,0);
   box(m,head,-4,-8,-4,8,8,8,0,0); box(m,body,-4,0,-2,8,12,4,16,16); box(m,ra,-1,-2,-1,2,12,2,40,16); box(m,la,-1,-2,-1,2,12,2,40,16,true); box(m,rl,-1,0,-1,2,12,2,0,16); box(m,ll,-1,0,-1,2,12,2,0,16,true); }
 // ---------------- spider (64x32): head, neck, body, 8 legs
 { EModelDef&m=g_models[(size_t)EType::Spider]; m.name="spider"; m.tex="entity/spider/spider.png"; m.tw=64; m.th=32; m.baseY=24;
   int head=part(m,0,15,-3),neck=part(m,0,15,0),body=part(m,0,15,9);
   box(m,head,-4,-4,-8,8,8,8,32,4); box(m,neck,-3,-3,-3,6,6,6,0,0); box(m,body,-5,-4,-6,10,8,12,0,12);
   const float ry[4]={0.7854f,0.3927f,-0.3927f,-0.7854f},rz[4]={0.7854f,0.58f,0.58f,0.7854f},zz[4]={2,1,0,-1};
   for(int i=0;i<4;i++){ int l=part(m,-4,15,zz[i],0,ry[i],-rz[i]); box(m,l,-15,-1,-1,16,2,2,18,0); int r=part(m,4,15,zz[i],0,-ry[i],rz[i]); box(m,r,-1,-1,-1,16,2,2,18,0,true); } }
 // ---------------- sheep (64x32) with its wool layer
 { EModelDef&m=g_models[(size_t)EType::Sheep]; m.name="sheep"; m.tex="entity/sheep/sheep.png"; m.tex2="entity/sheep/sheep_wool.png"; m.tw=64; m.th=32;
   int head=part(m,0,6,-8),body=part(m,0,5,2,PI/2),l0=part(m,-3,12,7),l1=part(m,3,12,7),l2=part(m,-3,12,-5),l3=part(m,3,12,-5);
   box(m,head,-3,-4,-6,6,6,8,0,0); box(m,body,-4,-10,-7,8,16,6,28,8); for(int l:{l0,l1,l2,l3})box(m,l,-2,0,-2,4,12,4,0,16);
   box(m,head,-3,-4,-4,6,6,6,0,0,false,0.6f,true); box(m,body,-4,-10,-7,8,16,6,28,8,false,1.75f,true); for(int l:{l0,l1,l2,l3})box(m,l,-2,0,-2,4,6,4,0,16,false,0.5f,true); }
 // ---------------- ghast (a 64x32 layout in a 128x64 file)
 { EModelDef&m=g_models[(size_t)EType::Ghast]; m.name="ghast"; m.tex="entity/ghast/ghast.png"; m.tw=64; m.th=32; m.baseY=36;
   int body=part(m,0,17,0); box(m,body,-8,-8,-8,16,16,16,0,0);
   for(int i=0;i<9;i++){ const float x=float(i%3-1)*5.f,z=float(i/3-1)*5.f; int t=part(m,x,25,z); box(m,t,-1,0,-1,2,float(8+(i*5)%4),2,0,0); } }
 // ---------------- slime (64x32): inner body, eyes, mouth, translucent outer cube
 { EModelDef&m=g_models[(size_t)EType::Slime]; m.name="slime"; m.tex="entity/slime/slime.png"; m.tw=64; m.th=32; m.baseY=24;
   int b=part(m,0,0,0); box(m,b,-3,16,-3,6,6,6,0,16); box(m,b,-3.25f,18,-3.5f,2,2,2,32,0); box(m,b,1.25f,18,-3.5f,2,2,2,32,4); box(m,b,0,21,-3.5f,1,1,1,32,8); box(m,b,-4,16,-4,8,8,8,0,0); }
 // ---------------- wolf (64x32)
 { EModelDef&m=g_models[(size_t)EType::Wolf]; m.name="wolf"; m.tex="entity/wolf/wolf.png"; m.tw=64; m.th=32;
   int head=part(m,-1,13.5f,-7),body=part(m,0,14,2,PI/2),mane=part(m,-1,14,-3,PI/2),l0=part(m,-2.5f,16,7),l1=part(m,0.5f,16,7),l2=part(m,-2.5f,16,-4),l3=part(m,0.5f,16,-4),tail=part(m,-1,12,8,PI/5);
   box(m,head,-2,-3,-2,6,6,4,0,0); box(m,head,-2,-5,0,2,2,1,16,14); box(m,head,2,-5,0,2,2,1,16,14); box(m,head,-0.5f,0,-5,3,3,4,0,10);
   box(m,body,-3,-2,-3,6,9,6,18,14); box(m,mane,-3,-3,-3,8,6,7,21,0);
   for(int l:{l0,l1,l2,l3})box(m,l,-1,0,-1,2,8,2,0,18); box(m,tail,-1,0,-1,2,8,2,9,18); }
 // ---------------- villager (64x64)
 { EModelDef&m=g_models[(size_t)EType::Villager]; m.name="villager"; m.tex="entity/villager/villager.png"; m.tw=64; m.th=64;
   int head=part(m,0,0,0),nose=part(m,0,-2,0),body=part(m,0,0,0),arms=part(m,0,3,-1,-0.75f),l0=part(m,-2,12,0),l1=part(m,2,12,0);
   box(m,head,-4,-10,-4,8,10,8,0,0); box(m,nose,-1,-1,-6,2,4,2,24,0); box(m,body,-4,0,-3,8,12,6,16,20); box(m,body,-4,0,-3,8,18,6,0,38,false,0.5f);
   box(m,arms,-8,-2,-2,4,8,4,44,22); box(m,arms,4,-2,-2,4,8,4,44,22,true); box(m,arms,-4,2,-2,8,4,4,40,38); box(m,l0,-2,0,-2,4,12,4,0,22); box(m,l1,-2,0,-2,4,12,4,0,22,true); }
 g_built=true;
}

// face rectangles (vanilla box layout) in the model texture; order of faces: 0 down 1 up 2 west(-x) 3 east(+x) 4 north(-z) 5 south(+z)
struct Rect{int x,y,w,h;};
Rect faceRect(const EBox&b,int f){
 const int w=(int)std::lround(b.w),h=(int)std::lround(b.h),d=(int)std::lround(b.d);
 switch(f){ case 1: return {b.tu+d,b.tv,w,d}; case 0: return {b.tu+d+w,b.tv,w,d}; case 2: return {b.tu,b.tv+d,d,h}; case 4: return {b.tu+d,b.tv+d,w,h}; case 3: return {b.tu+d+w,b.tv+d,d,h}; default: return {b.tu+d+w+d,b.tv+d,w,h}; }
}
bool faceExists(const EBox&b,int f){ const Rect r=faceRect(b,f); return r.w>0&&r.h>0; }
}

void entityTextureRequests(std::vector<TexReq>&out){
 buildModels();
 for(size_t t=0;t<g_models.size();t++){ const EModelDef&m=g_models[t]; if(m.name.empty())continue;
  for(size_t bi=0;bi<m.boxes.size();bi++)for(int f=0;f<6;f++){ if(!faceExists(m.boxes[bi],f))continue; const Rect r=faceRect(m.boxes[bi],f); TexReq q;
   q.key="e:"+m.name+":"+std::to_string(bi)+":"+std::to_string(f); q.file=m.boxes[bi].alt?m.tex2:m.tex; q.x=r.x;q.y=r.y;q.w=r.w;q.h=r.h; q.texW=m.tw;q.texH=m.th; q.hurt=m.hurtTiles; q.flash=(m.name=="creeper"); out.push_back(q); } }
}
void entityResolveTiles(const TexturePack&pack){
 buildModels();
 for(auto&m:g_models){ if(m.name.empty())continue; const std::array<uint16_t,6> z{0,0,0,0,0,0}; m.tiles.assign(m.boxes.size(),z); m.tilesH.assign(m.boxes.size(),z); m.tilesF.assign(m.boxes.size(),z);
  for(size_t bi=0;bi<m.boxes.size();bi++)for(int f=0;f<6;f++){ const std::string k="e:"+m.name+":"+std::to_string(bi)+":"+std::to_string(f); if(pack.has(k))m.tiles[bi][f]=pack.id(k); if(pack.has(k+":h"))m.tilesH[bi][f]=pack.id(k+":h"); if(pack.has(k+":f"))m.tilesF[bi][f]=pack.id(k+":f"); } }
}

namespace{
struct Mtx{ float r[9]={1,0,0,0,1,0,0,0,1}; float t[3]={0,0,0}; };
Mtx mulM(const Mtx&a,const Mtx&b){ Mtx o; for(int i=0;i<3;i++){ for(int j=0;j<3;j++)o.r[i*3+j]=a.r[i*3]*b.r[j]+a.r[i*3+1]*b.r[3+j]+a.r[i*3+2]*b.r[6+j]; o.t[i]=a.r[i*3]*b.t[0]+a.r[i*3+1]*b.t[1]+a.r[i*3+2]*b.t[2]+a.t[i]; } return o; }
Mtx rotZYX(float rx,float ry,float rz){   // R = Rz * Ry * Rx (x first)
 const float cx=std::cos(rx),sx=std::sin(rx),cy=std::cos(ry),sy=std::sin(ry),cz=std::cos(rz),sz=std::sin(rz);
 Mtx X; X.r[4]=cx;X.r[5]=-sx;X.r[7]=sx;X.r[8]=cx;
 Mtx Y; Y.r[0]=cy;Y.r[2]=sy;Y.r[6]=-sy;Y.r[8]=cy;
 Mtx Z; Z.r[0]=cz;Z.r[1]=-sz;Z.r[3]=sz;Z.r[4]=cz;
 return mulM(Z,mulM(Y,X));
}
void poseFor(const Entity&e,float time,const EModelDef&m,std::vector<PartPose>&p){
 p.assign(m.parts.size(),PartPose{});
 const float sw=std::sin(e.walk)*e.walkSpeed,sw2=std::sin(e.walk+PI)*e.walkSpeed;
 switch(e.type){
  case EType::Cow: case EType::Pig: { p[0].rx=e.pitch; p[2].rx=sw*1.4f; p[3].rx=sw2*1.4f; p[4].rx=sw2*1.4f; p[5].rx=sw*1.4f; break; }
  case EType::Chicken: { p[0].rx=e.pitch; p[2].rx=sw*1.4f; p[3].rx=sw2*1.4f; const float flap=e.onGround?0.f:(0.6f+std::sin(time*30.f)*0.7f); p[4].rz=flap; p[5].rz=-flap; break; }
  case EType::Creeper: { p[0].rx=e.pitch; p[2].rx=sw*1.4f; p[3].rx=sw2*1.4f; p[4].rx=sw2*1.4f; p[5].rx=sw*1.4f; break; }
  case EType::Enderman: { p[0].rx=e.pitch; p[2].rx=sw2*0.9f; p[3].rx=sw*0.9f; p[4].rx=sw*0.9f; p[5].rx=sw2*0.9f; if(e.s1){ p[2].rx=p[3].rx=-1.05f; p[2].rz=0.05f; p[3].rz=-0.05f; }   // s1: arms raised while carrying a block
    if(e.s0==2){ p[0].dy=-1.f; } break; }
  case EType::Blaze: { const float a=time*2.2f; int k=1;
    for(int i=0;i<4;i++,k++){ const float ang=a+i*PI/2; p[k].dx=std::cos(ang)*9.f-1.f; p[k].dz=std::sin(ang)*9.f-1.f; p[k].dy=-2.f+std::cos((i*2+time*20.f)*0.25f); }
    for(int i=4;i<8;i++,k++){ const float ang=a*-0.8f+(i-4)*PI/2+0.7f; p[k].dx=std::cos(ang)*7.f-1.f; p[k].dz=std::sin(ang)*7.f-1.f; p[k].dy=2.f+std::cos((i*2+time*20.f)*0.25f); }
    for(int i=8;i<12;i++,k++){ const float ang=a*0.6f+(i-8)*PI/2+0.3f; p[k].dx=std::cos(ang)*5.f-1.f; p[k].dz=std::sin(ang)*5.f-1.f; p[k].dy=11.f+std::cos((i*1.5f+time*20.f)*0.5f); }
    p[0].ry=0; break; }
  case EType::Crystal: { p[0].dy=0; p[1].ry=time*1.6f; p[2].ry=time*1.6f; p[1].rx=0.6f; p[2].rx=0.6f; const float bob=std::sin(time*2.f)*1.5f; p[1].dy=bob; p[2].dy=bob; break; }
  case EType::Dragon: { // 0 body,1 head,2 jaw,3..7 neck,8..19 tail,20/21 wings,22.. legs
    const float flap=std::sin(time*3.4f)*0.6f*(e.s0==3?0.f:1.f); const float bob=std::sin(time*3.4f)*2.f;
    p[0].dy=bob; const float ph=e.s0==3?0.f:1.f;
    p[1].dz=-70.f; p[1].dy=bob+(e.s0==3?18.f:4.f); p[1].rx=e.pitch*0.5f; p[2].rx=0.25f+std::sin(time*2.f)*0.15f+(e.t1>0?0.5f:0.f);
    for(int i=0;i<5;i++){ p[3+i].dz=-16.f-i*10.f; p[3+i].dy=bob+(i*(e.s0==3?4.f:1.f))+std::sin(time*2.f+i*0.6f)*2.f*ph; }
    for(int i=0;i<12;i++){ p[8+i].dz=48.f+i*10.f; p[8+i].dy=bob+std::sin(time*2.f-i*0.4f)*(3.f+i*0.5f)*ph; p[8+i].dx=std::sin(time*1.5f-i*0.5f)*i*0.6f*ph; }
    p[20].rz=-flap-0.15f*ph; p[21].rz=flap+0.15f*ph; p[20].dy=p[21].dy=bob;
    // legs 22..25 : two parts each (front l, front r, hind l, hind r are parts 22..25)
    for(size_t i=22;i<p.size();i++){ p[i].dy=bob; p[i].rx=(e.s0==3?0.f:0.9f); }
    break; }
  case EType::Zombie: { p[0].rx=e.pitch; p[2].rx=-1.5f+sw*0.05f; p[3].rx=-1.5f-sw*0.05f; p[4].rx=sw*1.2f; p[5].rx=sw2*1.2f; break; }
  case EType::Skeleton: { p[0].rx=e.pitch; if(e.s0==1){ p[2].rx=-1.45f; p[3].rx=-1.2f; p[3].ry=0.4f; } else { p[2].rx=sw2*0.9f; p[3].rx=sw*0.9f; } p[4].rx=sw*1.2f; p[5].rx=sw2*1.2f; break; }
  case EType::Spider: { const float a=std::sin(e.walk*1.3f)*e.walkSpeed*0.5f; for(int i=0;i<8;i++){ p[3+i].ry=((i%2)?a:-a)*(i<4?1.f:-1.f); p[3+i].rz=((i%2)?-1.f:1.f)*std::fabs(a)*0.4f; } p[0].rx=e.pitch; break; }
  case EType::Sheep: { for(size_t b=0;b<2;b++)(void)b; p[0].rx=e.pitch; p[2].rx=sw*1.4f; p[3].rx=sw2*1.4f; p[4].rx=sw2*1.4f; p[5].rx=sw*1.4f; break; }
  case EType::Ghast: { for(int i=1;i<=9;i++){ p[(size_t)i].rx=std::sin(time*1.6f+i)*0.25f; p[(size_t)i].rz=std::cos(time*1.3f+i*1.7f)*0.25f; } p[0].dy=std::sin(time*1.1f)*2.f; break; }
  case EType::Slime: { const float sq=1.f+std::max(0.f,-e.vel.y)*0.015f; (void)sq; p[0].dy=e.onGround?0.f:-2.f; break; }
  case EType::Wolf: { p[0].rx=e.pitch; p[3].rx=sw*1.4f; p[4].rx=sw2*1.4f; p[5].rx=sw2*1.4f; p[6].rx=sw*1.4f; p[7].rx=PI/5+(e.s0==1?-0.4f:0.f)+std::sin(time*3.f)*0.1f; break; }
  case EType::Villager: { p[0].ry=std::sin(time*0.7f)*0.15f; p[4].rx=sw*1.0f; p[5].rx=sw2*1.0f; break; }
  default: break; }
}
}


// local coordinates (blocks, relative to the entity origin) -> packed vertices; counter-clockwise outward order is derived from the outward normal n
static void emitQuadLocal(ChunkMesh&out,const float Q[4][3],const float UV[4][2],int tile,const float n[3]){
 float e1[3],e2[3]; for(int k=0;k<3;k++){e1[k]=Q[1][k]-Q[0][k];e2[k]=Q[2][k]-Q[0][k];}
 const float cx=e1[1]*e2[2]-e1[2]*e2[1],cyy=e1[2]*e2[0]-e1[0]*e2[2],cz=e1[0]*e2[1]-e1[1]*e2[0];
 const bool ccw=(cx*n[0]+cyy*n[1]+cz*n[2])>=0;
 const float ax=std::fabs(n[0]),ay=std::fabs(n[1]),az=std::fabs(n[2]);
 const int face=(ay>=ax&&ay>=az)?(n[1]>0?1:0):(ax>=az?(n[0]>0?3:2):(n[2]>0?5:4));
 Vertex vt[4];
 for(int i=0;i<4;i++){
  const long qx=std::clamp(std::lround((Q[i][0]+kEntityMeshOffX)*16.f),0l,511l),qy=std::clamp(std::lround((Q[i][1]+kEntityMeshOffY)*16.f),0l,4095l),qz=std::clamp(std::lround((Q[i][2]+kEntityMeshOffZ)*16.f),0l,511l);
  const long qu=std::clamp(std::lround(UV[i][0]),0l,511l),qv=std::clamp(std::lround(UV[i][1]),0l,511l);
  vt[i].a=(uint32_t)qx|((uint32_t)qz<<9)|((uint32_t)qy<<18)|((uint32_t)(face&3)<<30);
  vt[i].b=(uint32_t)qu|((uint32_t)qv<<9)|((uint32_t)tile<<18)|((uint32_t)(face>>2)<<30); }
 if(ccw)out.vertices.insert(out.vertices.end(),{vt[0],vt[1],vt[2],vt[3]}); else out.vertices.insert(out.vertices.end(),{vt[0],vt[3],vt[2],vt[1]});
}

bool buildBeamPiece(const Vec3&from,const Vec3&to,int piece,uint16_t tile,float time,ChunkMesh&out,Vec3&origin){
 out.vertices.clear(); for(auto&b:out.bucketEnd)b=0; out.maxY=0;
 Vec3 d{to.x-from.x,to.y-from.y,to.z-from.z}; const float L=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z); if(L<0.5f||!tile)return false;
 const float seg=12.f; const int n=(int)std::ceil(L/seg); if(piece<0||piece>=n)return false;
 const Vec3 u{d.x/L,d.y/L,d.z/L}; const float t0=piece*seg,t1=std::min(L,(piece+1)*seg);
 const Vec3 a{from.x+u.x*t0,from.y+u.y*t0,from.z+u.z*t0}; origin=Vec3{a.x-kEntityMeshOffX,a.y-kEntityMeshOffY,a.z-kEntityMeshOffZ};
 const float len=t1-t0; const float hx=std::fabs(u.y)<0.9f?0.f:1.f,hy=std::fabs(u.y)<0.9f?1.f:0.f;
 Vec3 p{u.y*0.f-u.z*hy,u.z*hx-u.x*0.f,u.x*hy-u.y*hx}; { const float pl=std::sqrt(p.x*p.x+p.y*p.y+p.z*p.z); p.x/=pl;p.y/=pl;p.z/=pl; }
 const Vec3 q{u.y*p.z-u.z*p.y,u.z*p.x-u.x*p.z,u.x*p.y-u.y*p.x};
 const float w=0.10f+0.03f*std::sin(time*12.f);
 const float cs[4][2]={{1,1},{-1,1},{-1,-1},{1,-1}};
 for(int k=0;k<4;k++){ const int k2=(k+1)%4; float Q[4][3]; Vec3 c0{p.x*cs[k][0]*w+q.x*cs[k][1]*w,p.y*cs[k][0]*w+q.y*cs[k][1]*w,p.z*cs[k][0]*w+q.z*cs[k][1]*w},c1{p.x*cs[k2][0]*w+q.x*cs[k2][1]*w,p.y*cs[k2][0]*w+q.y*cs[k2][1]*w,p.z*cs[k2][0]*w+q.z*cs[k2][1]*w};
  const float ps[4][3]={{c0.x,c0.y,c0.z},{c0.x+u.x*len,c0.y+u.y*len,c0.z+u.z*len},{c1.x+u.x*len,c1.y+u.y*len,c1.z+u.z*len},{c1.x,c1.y,c1.z}}; for(int i=0;i<4;i++)for(int a2=0;a2<3;a2++)Q[i][a2]=ps[i][a2];
  const float nn[3]={c0.x+c1.x,c0.y+c1.y,c0.z+c1.z}; const float UV[4][2]={{0,0},{16,0},{16,16},{0,16}}; emitQuadLocal(out,Q,UV,tile,nn); }
 for(auto&b:out.bucketEnd)b=(uint32_t)out.vertices.size(); return !out.vertices.empty();
}

bool buildEntityMesh(const Entity&e,float time,uint16_t itemTile,ChunkMesh&out){
 buildModels();
 out.vertices.clear(); for(auto&b:out.bucketEnd)b=0; out.maxY=0;
 auto put=[&](const float P[4][3],const float UV[4][2],int tile,const float nOut[3],float yaw){
  const float cy=std::cos(yaw),sy=std::sin(yaw); float Q[4][3]; float n[3];
  for(int k=0;k<4;k++){ Q[k][0]=P[k][0]*cy+P[k][2]*sy; Q[k][1]=P[k][1]; Q[k][2]=-P[k][0]*sy+P[k][2]*cy; }
  n[0]=nOut[0]*cy+nOut[2]*sy; n[1]=nOut[1]; n[2]=-nOut[0]*sy+nOut[2]*cy;
  emitQuadLocal(out,Q,UV,tile,n);
 };
 float yaw=e.yaw;
 if(e.type==EType::Item){   // dropped item: a spinning cube (blocks) or a double sided sprite
  const float bob=0.18f+std::sin(e.age*3.f)*0.05f; const float spin=e.age*1.6f;
  if(e.item<kItemBase){ const BlockInfo&bi=Registry::get().info((BlockId)e.item); const float s=0.125f;   // 0.25 block cube
   static const float cube[6][4][3]={ {{-1,-1,-1},{1,-1,-1},{1,-1,1},{-1,-1,1}}, {{-1,1,1},{1,1,1},{1,1,-1},{-1,1,-1}}, {{-1,-1,1},{-1,-1,-1},{-1,1,-1},{-1,1,1}}, {{1,-1,-1},{1,-1,1},{1,1,1},{1,1,-1}}, {{-1,-1,-1},{-1,1,-1},{1,1,-1},{1,-1,-1}}, {{1,-1,1},{1,1,1},{-1,1,1},{-1,-1,1}} };
   static const float nrm[6][3]={{0,-1,0},{0,1,0},{-1,0,0},{1,0,0},{0,0,-1},{0,0,1}};
   for(int f=0;f<6;f++){ float P[4][3]; for(int i=0;i<4;i++){ P[i][0]=cube[f][i][0]*s; P[i][1]=cube[f][i][1]*s+bob+s; P[i][2]=cube[f][i][2]*s; }
    const float UV[4][2]={{0,16},{16,16},{16,0},{0,0}}; put(P,UV,bi.texture[f]?bi.texture[f]:bi.texture[1],nrm[f],spin); }
  } else if(itemTile){ const float s=0.25f; const float P[4][3]={{-s,bob,0},{s,bob,0},{s,bob+2*s,0},{-s,bob+2*s,0}}; const float UV[4][2]={{0,16},{16,16},{16,0},{0,0}};
   const float n[3]={0,0,1}; put(P,UV,itemTile,n,spin); const float Q[4][3]={{s,bob,0},{-s,bob,0},{-s,bob+2*s,0},{s,bob+2*s,0}}; const float UV2[4][2]={{0,16},{16,16},{16,0},{0,0}}; const float n2[3]={0,0,-1}; put(Q,UV2,itemTile,n2,spin); }
  else return false;
  return !out.vertices.empty()?(out.bucketEnd[0]=out.bucketEnd[1]=out.bucketEnd[2]=out.bucketEnd[3]=out.bucketEnd[4]=out.bucketEnd[5]=out.bucketEnd[6]=out.bucketEnd[7]=(uint32_t)out.vertices.size(),true):false;
 }
 if(e.type==EType::Fireball||e.type==EType::DragonFireball){   // a small glowing cube
  const float s=e.type==EType::DragonFireball?0.3f:(e.variant==2?0.28f:0.1f); const uint16_t t=itemTile; if(!t)return false; static const float cube[6][4][3]={ {{-1,-1,-1},{1,-1,-1},{1,-1,1},{-1,-1,1}}, {{-1,1,1},{1,1,1},{1,1,-1},{-1,1,-1}}, {{-1,-1,1},{-1,-1,-1},{-1,1,-1},{-1,1,1}}, {{1,-1,-1},{1,-1,1},{1,1,1},{1,1,-1}}, {{-1,-1,-1},{-1,1,-1},{1,1,-1},{1,-1,-1}}, {{1,-1,1},{1,1,1},{-1,1,1},{-1,-1,1}} };
  static const float nrm[6][3]={{0,-1,0},{0,1,0},{-1,0,0},{1,0,0},{0,0,-1},{0,0,1}};
  for(int f=0;f<6;f++){ float P[4][3]; for(int i=0;i<4;i++){ P[i][0]=cube[f][i][0]*s; P[i][1]=cube[f][i][1]*s+s; P[i][2]=cube[f][i][2]*s; } const float UV[4][2]={{0,16},{16,16},{16,0},{0,0}}; put(P,UV,t,nrm[f],e.age*5.f); }
  for(auto&b:out.bucketEnd)b=(uint32_t)out.vertices.size(); return true; }


 if(e.type==EType::Arrow){ const uint16_t t=itemTile; if(!t)return false; const float sx=0.035f,sz=0.32f;
  static const float cube[6][4][3]={ {{-1,-1,-1},{1,-1,-1},{1,-1,1},{-1,-1,1}}, {{-1,1,1},{1,1,1},{1,1,-1},{-1,1,-1}}, {{-1,-1,1},{-1,-1,-1},{-1,1,-1},{-1,1,1}}, {{1,-1,-1},{1,-1,1},{1,1,1},{1,1,-1}}, {{-1,-1,-1},{-1,1,-1},{1,1,-1},{1,-1,-1}}, {{1,-1,1},{1,1,1},{-1,1,1},{-1,-1,1}} };
  static const float nrm[6][3]={{0,-1,0},{0,1,0},{-1,0,0},{1,0,0},{0,0,-1},{0,0,1}};
  for(int f=0;f<6;f++){ float P[4][3]; for(int i=0;i<4;i++){ P[i][0]=cube[f][i][0]*sx; P[i][1]=cube[f][i][1]*sx+sx; P[i][2]=cube[f][i][2]*sz; } const float UV[4][2]={{0,16},{16,16},{16,0},{0,0}}; put(P,UV,t,nrm[f],e.yaw); }
  for(auto&b:out.bucketEnd)b=(uint32_t)out.vertices.size(); return true; }
 const EModelDef&m=g_models[(size_t)e.type]; if(m.name.empty()||m.tiles.empty())return false;
 std::vector<PartPose> pose; poseFor(e,time,m,pose);
 // part matrices (model space, pixels, y down)
 std::vector<Mtx> pm(m.parts.size());
 for(size_t i=0;i<m.parts.size();i++){ const EPart&pt=m.parts[i]; const PartPose&po=pose[i]; Mtx r=rotZYX(pt.rx+po.rx,pt.ry+po.ry,pt.rz+po.rz); r.t[0]=pt.px+po.dx; r.t[1]=pt.py+po.dy; r.t[2]=pt.pz+po.dz; pm[i]=pt.parent>=0?mulM(pm[(size_t)pt.parent],r):r; }
 const float scale=(e.type==EType::Slime?float(std::max(1,e.variant)):1.f)/16.f;
 const bool hurtNow=e.hurt>0.f&&!m.tilesH.empty(); const bool flashNow=e.type==EType::Creeper&&e.s0==2&&std::fmod(e.t1*6.f,1.f)<0.5f&&!m.tilesF.empty();
 const auto&TT=hurtNow?m.tilesH:(flashNow?m.tilesF:m.tiles);
 for(size_t bi=0;bi<m.boxes.size();bi++){ const EBox&b=m.boxes[bi]; const EPart&pt=m.parts[(size_t)b.part]; const Mtx&M=pm[(size_t)b.part];
  const float g=b.grow; const float x0=(b.x-g)*pt.s,x1=(b.x+b.w+g)*pt.s,y0=(b.y-g)*pt.s,y1=(b.y+b.h+g)*pt.s,z0=(b.z-g)*pt.s,z1=(b.z+b.d+g)*pt.s;
  // corners per face: TL,TR,BR,BL as seen from outside with the texture upright (model space, y down)
  const float C[6][4][3]={ {{x0,y1,z0},{x1,y1,z0},{x1,y1,z1},{x0,y1,z1}}, {{x1,y0,z0},{x0,y0,z0},{x0,y0,z1},{x1,y0,z1}}, {{x0,y0,z1},{x0,y0,z0},{x0,y1,z0},{x0,y1,z1}}, {{x1,y0,z0},{x1,y0,z1},{x1,y1,z1},{x1,y1,z0}}, {{x0,y0,z0},{x1,y0,z0},{x1,y1,z0},{x0,y1,z0}}, {{x1,y0,z1},{x0,y0,z1},{x0,y1,z1},{x1,y1,z1}} };
  static const float N[6][3]={{0,1,0},{0,-1,0},{-1,0,0},{1,0,0},{0,0,-1},{0,0,1}};   // outward normals in model space (y down)
  for(int f=0;f<6;f++){ uint16_t tile=TT[bi][(size_t)f]; if(!tile)tile=m.tiles[bi][(size_t)f]; if(!tile)continue;
   float P[4][3],n[3]; 
   auto tr=[&](const float*v,float*o){ const float X=M.r[0]*v[0]+M.r[1]*v[1]+M.r[2]*v[2]+M.t[0],Y=M.r[3]*v[0]+M.r[4]*v[1]+M.r[5]*v[2]+M.t[1],Z=M.r[6]*v[0]+M.r[7]*v[1]+M.r[8]*v[2]+M.t[2]; o[0]=-X*scale; o[1]=(m.baseY-Y)*scale; o[2]=Z*scale; };
   for(int i=0;i<4;i++)tr(C[f][i],P[i]);
   { const float a[3]={M.r[0]*N[f][0]+M.r[1]*N[f][1]+M.r[2]*N[f][2],M.r[3]*N[f][0]+M.r[4]*N[f][1]+M.r[5]*N[f][2],M.r[6]*N[f][0]+M.r[7]*N[f][1]+M.r[8]*N[f][2]}; n[0]=-a[0]; n[1]=-a[1]; n[2]=a[2]; }
   float UV[4][2]={{0,0},{16,0},{16,16},{0,16}}; if(b.mirror){ std::swap(UV[0][0],UV[1][0]); std::swap(UV[3][0],UV[2][0]); }
   put(P,UV,tile,n,yaw+m.yawOff); } }
 for(auto&bk:out.bucketEnd)bk=(uint32_t)out.vertices.size();
 return !out.vertices.empty();
}
