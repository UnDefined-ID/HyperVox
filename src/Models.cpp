// Detailed render models (per-face textures / texture rectangles / quarter turns, thin planes, tilted parts) for the blocks the generated block list could only make as plain boxes.
// Collision and the selection outline still use the simple Box shapes; BlockInfo::model only changes how the block is drawn (see World::buildMesh).
// All coordinates are pixels (1/16 block).  Facing models are written for ONE orientation and turned with rotY().
#include "Registry.hpp"
#include "TexturePack.hpp"
#include <algorithm>
#include <array>
#include <cmath>

namespace{
MBox mbox(float x0,float y0,float z0,float x1,float y1,float z1){ MBox b; b.x0=x0;b.y0=y0;b.z0=z0;b.x1=x1;b.y1=y1;b.z1=z1; return b; }
void allF(MBox&b,int t){ for(auto&f:b.f){f.tex=(int16_t)t;f.autoUV=true;} }
void sideF(MBox&b,int t){ for(int i=2;i<6;i++){b.f[i].tex=(int16_t)t;b.f[i].autoUV=true;} }
void oneF(MBox&b,int fi,int t){ b.f[fi].tex=(int16_t)t; b.f[fi].autoUV=true; }
void rectF(MBox&b,int fi,int t,float uo,float vo,int rot=0,float tw=0,float th=0){ MFace&f=b.f[fi]; f.tex=(int16_t)t; f.uo=uo; f.vo=vo; f.rot=(uint8_t)rot; f.tw=tw; f.th=th; f.autoUV=false; }
void rectSides(MBox&b,int t,float uo,float vo,float tw=0,float th=0){ for(int i=2;i<6;i++)rectF(b,i,t,uo,vo,0,tw,th); }

// position based texture rectangle (what a vanilla cube element does)
void resolveAuto(MBox&b){
 for(int i=0;i<6;i++){ MFace&f=b.f[i]; if(f.tex<0||!f.autoUV)continue;
  switch(i){ case 0: case 1: f.uo=b.x0; f.vo=b.z0; break; case 2: f.uo=b.z0; f.vo=16-b.y1; break; case 3: f.uo=16-b.z1; f.vo=16-b.y1; break; case 4: f.uo=16-b.x1; f.vo=16-b.y1; break; default: f.uo=b.x0; f.vo=16-b.y1; break; }
  f.autoUV=false; }
}
void finalize(Model&m){ for(auto&b:m.boxes)resolveAuto(b); }

// turn a model 90 degrees clockwise (seen from above) k times about the block centre: north -> east -> south -> west
Model rotY(Model m,int k){
 k&=3; finalize(m);
 for(int s=0;s<k;s++){
  for(auto&b:m.boxes){ MBox n=b; n.x0=16-b.z1; n.x1=16-b.z0; n.z0=b.x0; n.z1=b.x1;
   // old face -> new face: -X(2)->-Z(4), +X(3)->+Z(5), -Z(4)->+X(3), +Z(5)->-X(2)
   static const int map[6]={0,1,4,5,3,2};
   for(int i=0;i<6;i++){ n.f[map[i]]=b.f[i]; }
   n.f[1].rot=(uint8_t)((n.f[1].rot+1)&3); n.f[0].rot=(uint8_t)((n.f[0].rot+3)&3);
   b=n; }
  for(auto&q:m.quads)for(auto&p:q.p){ const float x=p[0],z=p[2]; p[0]=16-z; p[2]=x; }
 }
 return m;
}

// a box turned by `deg` about an axis through (ox,oy,oz): emitted as 6 free quads (faces with a tex only); texture rectangles as for MBox
void tilted(Model&m,MBox b,char axis,float deg,float ox,float oy,float oz){
 resolveAuto(b);
 const float c=std::cos(deg*3.14159265f/180.f),s=std::sin(deg*3.14159265f/180.f);
 auto rot=[&](float p[3]){ float dx=p[0]-ox,dy=p[1]-oy,dz=p[2]-oz; float nx=dx,ny=dy,nz=dz;
  if(axis=='z'){ nx=dx*c+dy*s; ny=-dx*s+dy*c; } else if(axis=='x'){ ny=dy*c-dz*s; nz=dy*s+dz*c; } else { nx=dx*c+dz*s; nz=-dx*s+dz*c; }
  p[0]=ox+nx;p[1]=oy+ny;p[2]=oz+nz; };
 auto rotv=[&](float p[3]){ float dx=p[0],dy=p[1],dz=p[2],nx=dx,ny=dy,nz=dz;
  if(axis=='z'){ nx=dx*c+dy*s; ny=-dx*s+dy*c; } else if(axis=='x'){ ny=dy*c-dz*s; nz=dy*s+dz*c; } else { nx=dx*c+dz*s; nz=-dx*s+dz*c; }
  p[0]=nx;p[1]=ny;p[2]=nz; };
 static const float nom[6][3]={{0,-1,0},{0,1,0},{-1,0,0},{1,0,0},{0,0,-1},{0,0,1}};
 for(int fi=0;fi<6;fi++){ const MFace&f=b.f[fi]; if(f.tex<0)continue;
  float org[3],U[3]={0,0,0},V[3]={0,0,0},w,h;
  switch(fi){
   case 0: org[0]=b.x0;org[1]=b.y0;org[2]=b.z0; U[0]=1;V[2]=1; w=b.x1-b.x0;h=b.z1-b.z0; break;
   case 1: org[0]=b.x0;org[1]=b.y1;org[2]=b.z0; U[0]=1;V[2]=1; w=b.x1-b.x0;h=b.z1-b.z0; break;
   case 2: org[0]=b.x0;org[1]=b.y0;org[2]=b.z0; U[2]=1;V[1]=1; w=b.z1-b.z0;h=b.y1-b.y0; break;
   case 3: org[0]=b.x1;org[1]=b.y0;org[2]=b.z0; U[2]=1;V[1]=1; w=b.z1-b.z0;h=b.y1-b.y0; break;
   case 4: org[0]=b.x0;org[1]=b.y0;org[2]=b.z0; U[0]=1;V[1]=1; w=b.x1-b.x0;h=b.y1-b.y0; break;
   default: org[0]=b.x0;org[1]=b.y0;org[2]=b.z1; U[0]=1;V[1]=1; w=b.x1-b.x0;h=b.y1-b.y0; break; }
  static const float du[4]={0,1,1,0},dv[4]={0,0,1,1};
  MQuad q; q.tex=f.tex; q.twoSided=b.twoSided;
  for(int i=0;i<4;i++){ const float a=du[i]*w,bb=dv[i]*h; for(int k=0;k<3;k++)q.p[i][k]=org[k]+U[k]*a+V[k]*bb; faceUV(fi,w,h,a,bb,f,q.uv[i][0],q.uv[i][1]); rot(q.p[i]); }
  float n[3]={nom[fi][0],nom[fi][1],nom[fi][2]}; rotv(n);
  float e1[3],e2[3]; for(int k=0;k<3;k++){e1[k]=q.p[1][k]-q.p[0][k];e2[k]=q.p[2][k]-q.p[0][k];}
  const float cx=e1[1]*e2[2]-e1[2]*e2[1],cy=e1[2]*e2[0]-e1[0]*e2[2],cz=e1[0]*e2[1]-e1[1]*e2[0];
  if(cx*n[0]+cy*n[1]+cz*n[2]<0){ std::swap(q.p[1],q.p[3]); std::swap(q.uv[1],q.uv[3]); }
  m.quads.push_back(q); }
}
// a vertical double sided plane
MQuad plane(float x0,float y0,float z0,float x1,float y1,float z1,int tex,float u0,float v0,float u1,float v1){   // (x0,z0)-(x1,z1) at the bottom, y0..y1; uv rect corners
 MQuad q; q.tex=(int16_t)tex; q.twoSided=true;
 const float P[4][3]={{x0,y0,z0},{x1,y0,z1},{x1,y1,z1},{x0,y1,z0}}; const float UV[4][2]={{u0,v1},{u1,v1},{u1,v0},{u0,v0}};
 for(int i=0;i<4;i++){ for(int k=0;k<3;k++)q.p[i][k]=P[i][k]; q.uv[i][0]=UV[i][0]; q.uv[i][1]=UV[i][1]; }
 return q;
}
// a flat horizontal quad facing up at height y (one sided)
MQuad flatUp(float x0,float z0,float x1,float z1,float y,int tex,float u0,float v0,float u1,float v1){
 MQuad q; q.tex=(int16_t)tex; q.twoSided=false;
 const float P[4][3]={{x0,y,z0},{x0,y,z1},{x1,y,z1},{x1,y,z0}}; const float UV[4][2]={{u0,v0},{u0,v1},{u1,v1},{u1,v0}};
 for(int i=0;i<4;i++){ for(int k=0;k<3;k++)q.p[i][k]=P[i][k]; q.uv[i][0]=UV[i][0]; q.uv[i][1]=UV[i][1]; }
 return q;
}
}

void Registry::initModels(const TexturePack&pack){
 m_models.clear(); for(auto&b:m_blocks)b.model=0;
 auto T=[&](const std::string&n)->int{ return pack.has(n)?(int)pack.id(n):-1; };
 auto idOf=[&](const std::string&n)->BlockId{ auto it=m_blockIds.find(n); return it==m_blockIds.end()?BlockId(0):it->second; };
 auto setM=[&](BlockId id,Model m){ if(!id||id>=m_blocks.size())return; finalize(m); m_blocks[id].model=addModel(std::move(m)); };
 auto var=[&](BlockId base,int i)->BlockId{ return (base&&base<m_variants.size())?m_variants[base][i&15]:BlockId(0); };
 // facing variants of the kind 2 blocks: slot 0 = south (the base), 1 north, 2 east, 3 west.  The model is written facing south.
 auto setFacing=[&](BlockId base,const Model&south){ if(!base)return; static const int k[4]={0,2,3,1}; for(int s=0;s<4;s++){ BlockId v=s==0?base:var(base,s); if(v)setM(v,rotY(south,k[s])); } };

 // ---- honey / slime: translucent shell + the denser core inside
 { const int top=T("honey_block_top"),bot=T("honey_block_bottom"),side=T("honey_block_side"); const BlockId id=idOf("honey_block");
   if(id&&side>=0){ Model m; MBox o=mbox(0,0,0,16,16,16); o.trans=true; sideF(o,side); oneF(o,1,top>=0?top:side); oneF(o,0,bot>=0?bot:side); MBox in=mbox(1,1,1,15,15,15); in.trans=true; in.cullBorder=false; allF(in,side); m.boxes={o,in}; setM(id,m); } }
 { const int t=T("slime_block"); const BlockId id=idOf("slime_block");
   if(id&&t>=0){ Model m; MBox o=mbox(0,0,0,16,16,16); o.trans=true; allF(o,t); MBox in=mbox(3,3,3,13,13,13); in.trans=true; in.cullBorder=false; allF(in,t); m.boxes={o,in}; setM(id,m); } }

 // ---- heavy core: 8x8x8 with the three panels of its texture
 { const int t=T("heavy_core"); const BlockId id=idOf("heavy_core"); if(id&&t>=0){ Model m; MBox b=mbox(4,0,4,12,8,12); rectF(b,1,t,0,0); rectF(b,0,t,8,0); rectSides(b,t,0,8); m.boxes={b}; setM(id,m); } }

 // ---- hopper
 { const int out=T("hopper_outside"),in=T("hopper_inside"),top=T("hopper_top"); const BlockId id=idOf("hopper");
   if(id&&out>=0&&in>=0){ Model m;
    MBox plate=mbox(0,10,0,16,11,16); allF(plate,out); rectF(plate,1,top>=0?top:out,0,0);
    MBox w1=mbox(0,11,0,2,16,16); allF(w1,out); w1.f[0].tex=-1; oneF(w1,3,in);
    MBox w2=mbox(14,11,0,16,16,16); allF(w2,out); w2.f[0].tex=-1; oneF(w2,2,in);
    MBox w3=mbox(2,11,0,14,16,2); allF(w3,out); w3.f[0].tex=-1; w3.f[2].tex=w3.f[3].tex=-1; oneF(w3,5,in);
    MBox w4=mbox(2,11,14,14,16,16); allF(w4,out); w4.f[0].tex=-1; w4.f[2].tex=w4.f[3].tex=-1; oneF(w4,4,in);
    MBox neck=mbox(4,4,4,12,10,12); allF(neck,out); oneF(neck,1,in);
    MBox spout=mbox(6,0,6,10,4,10); allF(spout,out);
    m.boxes={plate,w1,w2,w3,w4,neck,spout}; setM(id,m); } }

 // ---- cauldron
 { const int side=T("cauldron_side"),inner=T("cauldron_inner"),top=T("cauldron_top"),bot=T("cauldron_bottom"); const BlockId id=idOf("cauldron");
   if(id&&side>=0&&inner>=0){ Model m;
    auto wall=[&](float x0,float z0,float x1,float z1,int outer,int innerFace){ MBox b=mbox(x0,4,z0,x1,16,z1); allF(b,side); b.f[0].tex=-1; oneF(b,innerFace,inner); oneF(b,1,top>=0?top:side); (void)outer; return b; };
    m.boxes.push_back(wall(0,0,16,2,4,5)); m.boxes.push_back(wall(0,14,16,16,5,4)); m.boxes.push_back(wall(0,2,2,14,2,3)); m.boxes.push_back(wall(14,2,16,14,3,2));
    { MBox f=mbox(2,3,2,14,4,14); allF(f,inner); m.boxes.push_back(f); }
    const float L[8][6]={{0,0,0,4,3,2},{0,0,2,2,3,4},{12,0,0,16,3,2},{14,0,2,16,3,4},{0,0,14,4,3,16},{0,0,12,2,3,14},{12,0,14,16,3,16},{14,0,12,16,3,14}};
    for(auto&l:L){ MBox b=mbox(l[0],l[1],l[2],l[3],l[4],l[5]); allF(b,side); b.f[1].tex=-1; if(bot>=0)oneF(b,0,bot); m.boxes.push_back(b); }
    setM(id,m); } }

 // ---- composter
 { const int side=T("composter_side"),top=T("composter_top"),bot=T("composter_bottom"); const BlockId id=idOf("composter");
   if(id&&side>=0){ Model m;
    auto wall=[&](float x0,float z0,float x1,float z1,int innerFace){ MBox b=mbox(x0,0,z0,x1,16,z1); allF(b,side); oneF(b,1,top>=0?top:side); if(bot>=0)oneF(b,0,bot); (void)innerFace; return b; };
    m.boxes.push_back(wall(0,0,16,2,5)); m.boxes.push_back(wall(0,14,16,16,4)); m.boxes.push_back(wall(0,2,2,14,3)); m.boxes.push_back(wall(14,2,16,14,2));
    { MBox f=mbox(2,0,2,14,2,14); allF(f,bot>=0?bot:side); m.boxes.push_back(f); }
    setM(id,m); } }

 // ---- campfires: four logs and the flames
 for(int soul=0;soul<2;soul++){ const std::string nm=soul?"soul_campfire":"campfire"; const int log=T(soul?"soul_campfire_log_lit":"campfire_log_lit"),fire=T(soul?"soul_campfire_fire":"campfire_fire"); const BlockId id=idOf(nm);
   if(!id||log<0)continue; Model m;
   for(int i=0;i<2;i++){ const float z=i?11:1; MBox b=mbox(0,0,z,16,4,z+4); rectF(b,2,log,0,4); rectF(b,3,log,0,4); rectF(b,4,log,0,0); rectF(b,5,log,0,0); rectF(b,1,log,0,0); rectF(b,0,log,0,0); m.boxes.push_back(b); }
   for(int i=0;i<2;i++){ const float x=i?11:1; MBox b=mbox(x,3,0,x+4,7,16); rectF(b,4,log,0,4); rectF(b,5,log,0,4); rectF(b,2,log,0,0); rectF(b,3,log,0,0); rectF(b,1,log,0,0,1); rectF(b,0,log,0,0,1); m.boxes.push_back(b); }
   if(fire>=0){ m.quads.push_back(plane(2,3,8,14,16,8,fire,0,0,16,16)); m.quads.push_back(plane(8,3,2,8,16,14,fire,0,0,16,16)); }
   setM(id,m); }

 // ---- azalea bushes: leafy top block on a thin crossed stem
 for(const char*nm:{"azalea","flowering_azalea"}){ const std::string n=nm; const int top=T(n+"_top"),side=T(n+"_side"),plant=T("azalea_plant"); const BlockId id=idOf(n);
   if(!id||top<0||side<0)continue; Model m; MBox b=mbox(0,8,0,16,16,16); allF(b,side); oneF(b,1,top); oneF(b,0,top); m.boxes.push_back(b);
   if(plant>=0){ m.quads.push_back(plane(0,0,8,16,8,8,plant,0,8,16,16)); m.quads.push_back(plane(8,0,0,8,8,16,plant,0,8,16,16)); }
   setM(id,m); }

 // ---- rails: ONE textured quad per shape (flat, ascending, curved); the texture comes from the variant's own tiles
 for(const char*nm:{"rail","powered_rail","detector_rail","activator_rail"}){ const BlockId base=idOf(nm); if(!base)continue; const bool plain=std::string(nm)=="rail"; const int slots=plain?10:12;
  for(int s=0;s<slots;s++){ const BlockId id=s==0?base:var(base,s); if(!id)continue; const int t=m_blocks[id].texture[1]; const int shape=plain?s:(s%6); Model m;
   auto quad=[&](const float P[4][3],const float UV[4][2]){ MQuad q; q.tex=(int16_t)t; q.twoSided=false; for(int i=0;i<4;i++){ for(int k=0;k<3;k++)q.p[i][k]=P[i][k]; q.uv[i][0]=UV[i][0]; q.uv[i][1]=UV[i][1]; } m.quads.push_back(q); };
   // corners in the order (x0,z0) (x0,z1) (x1,z1) (x1,z0); y per corner differs for slopes
   float y00=1,y01=1,y11=1,y10=1; bool alongX=false; int rot=0;
   switch(shape){ case 0: break; case 1: alongX=true; break;
    case 2: alongX=true; y10=17; y11=17; break;      // rises towards +x
    case 3: alongX=true; y00=17; y01=17; break;      // rises towards -x
    case 4: y00=17; y10=17; break;                   // rises towards -z
    case 5: y01=17; y11=17; break;                   // rises towards +z
    case 6: break; case 7: rot=1; break; case 8: rot=2; break; default: rot=3; break; }
   const float P[4][3]={{0,y00,0},{0,y01,16},{16,y11,16},{16,y10,0}}; float UV[4][2];
   for(int i=0;i<4;i++){ const float x=P[i][0],z=P[i][2]; float u=x,v=z; if(alongX){ u=z; v=x; } else if(plain&&shape>=6){ if(rot==1){ u=z; v=16-x; } else if(rot==2){ u=16-x; v=16-z; } else if(rot==3){ u=16-z; v=x; } } UV[i][0]=u; UV[i][1]=v; }
   quad(P,UV); setM(id,m); } }

 // ---- ladders: one double sided plane on the wall (variants: north east south west = the side the ladder faces)
 { const BlockId base=idOf("ladder"); const int t=base?m_blocks[base].texture[2]:-1; if(base&&t>=0){ Model m; MQuad q; q.tex=(int16_t)t; q.twoSided=true;
    const float P[4][3]={{0,0,15},{0,16,15},{16,16,15},{16,0,15}}; const float UV[4][2]={{16,16},{16,0},{0,0},{0,16}}; for(int i=0;i<4;i++){ for(int k=0;k<3;k++)q.p[i][k]=P[i][k]; q.uv[i][0]=UV[i][0]; q.uv[i][1]=UV[i][1]; } m.quads.push_back(q);
    for(int s=0;s<4;s++){ const BlockId v=s==0?base:var(base,s); if(v)setM(v,rotY(m,s)); } } }

 // ---- wall torches: a rod leaning 22.5 degrees away from the wall (written for a wall at -x, turned for the others)
 for(const char*tn:{"torch","soul_torch","redstone_torch","copper_torch"}){ const BlockId head=idOf(tn); if(!head)continue; const int t=T(tn); if(t<0)continue; const bool red=std::string(tn)=="redstone_torch";
   Model m; MBox b=mbox(1,3.5f,7,3,13.5f,9); const float ru=red?6.f:7.f,rv=red?5.f:6.f,rw=red?4.f:2.f; rectSides(b,t,ru,rv,rw,10.f-(red?0.f:0.f)); b.f[1].tex=(int16_t)t; b.f[1].autoUV=false; b.f[1].uo=ru; b.f[1].vo=rv; b.f[1].tw=rw; b.f[1].th=2; b.f[0].tex=(int16_t)t; b.f[0].autoUV=false; b.f[0].uo=ru; b.f[0].vo=13; b.f[0].tw=rw; b.f[0].th=2;
   tilted(m,b,'z',22.5f,2,3.5f,8);
   static const int slotK[5]={0,0,2,1,3};   // slot 1 east, 2 west, 3 south, 4 north
   for(int s=1;s<=4;s++){ const BlockId v=var(head,s); if(v)setM(v,rotY(m,slotK[s])); } }

 // ---- fence gates (vanilla shape): posts, two rails, centre slats; open: leaves swung towards the facing side
 { for(auto&kv:std::vector<std::pair<std::string,BlockId>>(m_blockIds.begin(),m_blockIds.end())){ const std::string&n=kv.first; if(n.size()<11||n.compare(n.size()-10,10,"fence_gate")!=0)continue; const BlockId base=kv.second; const BlockInfo bi0=m_blocks[base]; const int p=bi0.texture[2];
    if(p<0)continue;
    auto mk=[&](float x0,float y0,float z0,float x1,float y1,float z1){ MBox b=mbox(x0,y0,z0,x1,y1,z1); allF(b,p); return b; };
    Model closed; closed.boxes={mk(0,5,7,2,16,9),mk(14,5,7,16,16,9),mk(2,6,7,6,9,9),mk(10,6,7,14,9,9),mk(2,12,7,6,15,9),mk(10,12,7,14,15,9),mk(6,6,7,10,15,9)};
    Model open;   // leaves hinged on the posts, written swinging towards +z
    open.boxes={mk(0,5,7,2,16,9),mk(14,5,7,16,16,9),mk(0,6,9,2,9,14),mk(0,12,9,2,15,14),mk(0,6,14,2,15,16),mk(14,6,9,16,9,14),mk(14,12,9,16,15,14),mk(14,6,14,16,15,16)};
    for(int s=0;s<8;s++){ const BlockId v=s==0?base:var(base,s); if(!v)continue; const int f=s&3; const bool op=(s&4)!=0;
     if(!op)setM(v,rotY(closed,f&1)); else { static const int kk[4]={2,3,0,1}; setM(v,rotY(open,kk[f])); } } } }

 // ---- lever: cobblestone base + tilted handle
 { const BlockId id=idOf("lever"); const int t=T("lever"),c=T("cobblestone"); if(id&&t>=0){ Model m; MBox base=mbox(5,0,4,11,3,12); allF(base,c>=0?c:t); m.boxes.push_back(base);
    MBox h=mbox(7,1,7,9,11,9); rectSides(h,t,7,6,2,10); rectF(h,1,t,7,6,0,2,2); rectF(h,0,t,7,14,0,2,2); tilted(m,h,'x',-45.f,8,1,8); setM(id,m); } }

 // ---- tripwire: a flat strip of string
 { const BlockId id=idOf("tripwire"); const int t=T("tripwire"); if(id&&t>=0){ Model m; MBox b=mbox(4,2,0,12,2,16); b.twoSided=true; rectF(b,1,t,0,0,1); m.boxes.push_back(b); setM(id,m); } }

 // ---- turtle egg
 { const BlockId id=idOf("turtle_egg"); const int t=T("turtle_egg"); if(id&&t>=0){ Model m; MBox b=mbox(3,0,3,12,7,12); rectF(b,1,t,0,0); rectF(b,0,t,0,0); rectSides(b,t,0,9); m.boxes.push_back(b); setM(id,m); } }

 // ---- sea pickle
 { const BlockId id=idOf("sea_pickle"); const int t=T("sea_pickle"); if(id&&t>=0){ Model m; MBox b=mbox(6,0,6,10,6,10); rectF(b,4,t,0,4); rectF(b,3,t,4,4); rectF(b,5,t,8,4); rectF(b,2,t,12,4); rectF(b,1,t,4,0); rectF(b,0,t,4,0); m.boxes.push_back(b); setM(id,m); } }

 // ---- big dripleaf: the leaf plate
 { const BlockId id=idOf("big_dripleaf"); const int top=T("big_dripleaf_top"),side=T("big_dripleaf_side"); if(id&&top>=0){ Model m; MBox b=mbox(0,11,0,16,15,16); rectF(b,1,top,0,0); rectF(b,0,top,0,0); if(side>=0)rectSides(b,side,0,0); else rectSides(b,top,0,0); m.boxes.push_back(b); setM(id,m); } }

 // ---- end portal frame with the eye
 { const BlockId base=idOf("end_portal_frame"); const int eye=T("end_portal_frame_eye"); if(base&&eye>=0){
    for(int s=4;s<8;s++){ const BlockId v=var(base,s); if(!v)continue; const BlockInfo&bi=m_blocks[v]; Model m; MBox f=mbox(0,0,0,16,13,16); for(int i=0;i<6;i++)oneF(f,i,bi.texture[i]); m.boxes.push_back(f);
     MBox e=mbox(4,13,4,12,16,12); e.cullBorder=false; rectF(e,1,eye,4,0); rectSides(e,eye,4,8); m.boxes.push_back(e); setM(v,m); } } }

 // ---- flower pot: walls on all four sides (inner faces too), floor, dirt
 { const BlockId id=idOf("flower_pot"); const int t=T("flower_pot"),d=T("dirt"); if(id&&t>=0){ Model m;
    auto wall=[&](float x0,float z0,float x1,float z1,int innerFace){ MBox b=mbox(x0,0,z0,x1,6,z1); allF(b,t); rectF(b,1,t,5,10); rectF(b,0,t,5,10); (void)innerFace; return b; };
    m.boxes.push_back(wall(5,5,11,6,5)); m.boxes.push_back(wall(5,10,11,11,4)); m.boxes.push_back(wall(5,6,6,10,3)); m.boxes.push_back(wall(10,6,11,10,2));
    { MBox f=mbox(6,0,6,10,1,10); allF(f,t); rectF(f,1,t,5,10); rectF(f,0,t,5,10); m.boxes.push_back(f); }
    if(d>=0){ MBox dirt=mbox(6,1,6,10,4,10); for(int i=0;i<6;i++)dirt.f[i].tex=-1; oneF(dirt,1,d); m.boxes.push_back(dirt); }
    setM(id,m); } }

 // ---- sculk sensors and the shrieker: slab body + tendrils / crystals / jaws
 { auto tendrils=[&](Model&m,int tex){ if(tex<0)return; m.quads.push_back(plane(0,8,8,16,16,8,tex,0,8,16,16)); m.quads.push_back(plane(8,8,0,8,16,16,tex,0,8,16,16)); };
   { const BlockId id=idOf("sculk_sensor"); const int top=T("sculk_sensor_top"),side=T("sculk_sensor_side"),bot=T("sculk_sensor_bottom"); if(id&&top>=0&&side>=0){ Model m; MBox b=mbox(0,0,0,16,8,16); sideF(b,side); oneF(b,1,top); oneF(b,0,bot>=0?bot:top); m.boxes.push_back(b); tendrils(m,T("sculk_sensor_tendril_inactive")); setM(id,m); } }
   { const BlockId id=idOf("calibrated_sculk_sensor"); const int top=T("calibrated_sculk_sensor_top"),side=T("sculk_sensor_side"),inp=T("calibrated_sculk_sensor_input_side"),bot=T("sculk_sensor_bottom"),am=T("calibrated_sculk_sensor_amethyst");
     if(id&&top>=0&&side>=0){ Model m; MBox b=mbox(0,0,0,16,8,16); sideF(b,side); if(inp>=0)oneF(b,4,inp); oneF(b,1,top); oneF(b,0,bot>=0?bot:top); m.boxes.push_back(b); tendrils(m,T("sculk_sensor_tendril_inactive"));
      if(am>=0){ m.quads.push_back(plane(4,8,8,12,16,8,am,0,0,16,16)); m.quads.push_back(plane(8,8,4,8,16,12,am,0,0,16,16)); } setM(id,m); } }
   { const BlockId id=idOf("sculk_shrieker"); const int top=T("sculk_shrieker_inner_top"),side=T("sculk_shrieker_side"),bot=T("sculk_shrieker_bottom"),claws=T("sculk_shrieker_top");
     if(id&&side>=0){ Model m; MBox b=mbox(0,0,0,16,8,16); rectSides(b,side,0,8); oneF(b,1,top>=0?top:side); oneF(b,0,bot>=0?bot:side); m.boxes.push_back(b);
      MBox j=mbox(0,8,0,16,16,16); j.cullBorder=false; rectSides(j,side,0,0); m.boxes.push_back(j);
      if(claws>=0){ MBox c=mbox(0,8,0,16,8,16); rectF(c,1,claws,0,0); m.boxes.push_back(c); } setM(id,m); } } }

 // ---- bell
 { const BlockId id=idOf("bell"); const int side=T("bell_side"),top=T("bell_top"),bot=T("bell_bottom"); if(id&&side>=0){ Model m;
    MBox body=mbox(5,6,5,11,13,11); rectSides(body,side,0,0); rectF(body,1,top>=0?top:side,0,0); m.boxes.push_back(body);
    MBox rim=mbox(4,4,4,12,6,12); rectSides(rim,side,0,12); rectF(rim,0,bot>=0?bot:side,0,0); m.boxes.push_back(rim);
    MBox hang=mbox(7,13,7,9,16,9); rectSides(hang,side,0,0,2,3); rectF(hang,1,side,0,0,0,2,2); m.boxes.push_back(hang); setM(id,m); } }

 // ---- banners: pole + crossbar + the coloured flag
 { const int planks=T("oak_planks"); for(auto&kv:std::vector<std::pair<std::string,BlockId>>(m_blockIds.begin(),m_blockIds.end())){ const std::string&n=kv.first; if(n.size()<8||n.compare(n.size()-7,7,"_banner")!=0)continue; const BlockId base=kv.second; const int wool=m_blocks[base].texture[2]; if(wool<0)continue;
    Model m; MBox pole=mbox(7,0,7,9,14,9); allF(pole,planks>=0?planks:wool); MBox bar=mbox(1,14,7,15,16,9); allF(bar,planks>=0?planks:wool); MBox flag=mbox(2,1,9,14,14,10); allF(flag,wool); m.boxes={pole,bar,flag};
    for(int s=0;s<4;s++){ const BlockId v=s==0?base:var(base,s); if(v)setM(v,rotY(m,s>=2?1:0)); } } }

 // ---- hanging signs: planks board on two chains
 { for(auto&kv:std::vector<std::pair<std::string,BlockId>>(m_blockIds.begin(),m_blockIds.end())){ const std::string&n=kv.first; if(n.size()<14||n.compare(n.size()-13,13,"_hanging_sign")!=0)continue; const BlockId base=kv.second; const std::string w=n.substr(0,n.size()-13);
    const int planks=T(w+"_planks"),chain=T("iron_chain_u")>=0?T("iron_chain_u"):T("iron_chain"); if(planks<0)continue;
    Model m; MBox board=mbox(1,2,7,15,12,9); allF(board,planks); m.boxes.push_back(board);
    for(float x:{2.f,13.f}){ MBox c=mbox(x,12,7.5f,x+1,16,8.5f); if(chain>=0){ rectSides(c,chain,7,0,1,4); rectF(c,1,chain,7,0,0,1,1); } else allF(c,planks); m.boxes.push_back(c); }
    for(int s=0;s<4;s++){ const BlockId v=s==0?base:var(base,s); if(v)setM(v,rotY(m,s>=2?1:0)); } } }

 // ---- anvils: the long side of the top plate follows the way you look; the top texture's strip runs along it
 for(const char*an:{"anvil","chipped_anvil","damaged_anvil"}){ const BlockId base=idOf(an); if(!base)continue; const BlockInfo bi=m_blocks[base]; const int side=bi.texture[2],top=bi.texture[1]; if(side<0)continue; Model m;
   MBox b0=mbox(2,0,2,14,4,14); allF(b0,side); MBox b1=mbox(4,4,3,12,5,13); allF(b1,side); MBox b2=mbox(6,5,4,10,10,12); allF(b2,side);
   MBox b3=mbox(0,10,3,16,16,13); allF(b3,side); rectF(b3,1,top,3,0,1);   // plate runs along x: the strip (u 3..13) is turned to follow it
   m.boxes={b0,b1,b2,b3}; setFacing(base,m); }

 // ---- stonecutter: low table with the saw blade
 { const BlockId base=idOf("stonecutter"); const int side=T("stonecutter_side"),top=T("stonecutter_top"),bot=T("stonecutter_bottom"),saw=T("stonecutter_saw"); if(base&&side>=0){ Model m;
    MBox b=mbox(0,0,0,16,9,16); rectSides(b,side,0,7); oneF(b,1,top>=0?top:side); oneF(b,0,bot>=0?bot:side); m.boxes.push_back(b);
    if(saw>=0){ MBox s=mbox(1,9,7,15,16,9); s.cullBorder=false; rectF(s,4,saw,1,9); rectF(s,5,saw,1,9); m.boxes.push_back(s); }
    setFacing(base,m); } }

 // ---- grindstone: two wooden legs and the wheel (floor mounted)
 { const BlockId base=idOf("grindstone"); const int side=T("grindstone_side"),round=T("grindstone_round"),piv=T("grindstone_pivot"); if(base&&side>=0&&round>=0){ Model m;
    MBox wheel=mbox(4,2,2,12,14,14); rectF(wheel,2,side,2,2); rectF(wheel,3,side,2,2); rectF(wheel,1,round,0,2,1); rectF(wheel,0,round,0,2,1); rectF(wheel,4,round,0,2); rectF(wheel,5,round,0,2); m.boxes.push_back(wheel);
    for(float x:{2.f,12.f}){ MBox leg=mbox(x,0,5,x+2,12,11); if(piv>=0){ for(int i=0;i<6;i++)rectF(leg,i,piv,0,0,0,4,4); } else allF(leg,side); m.boxes.push_back(leg); }
    setFacing(base,m); } }

 // ---- repeater / comparator: slab with torches
 { auto torchBox=[&](float x0,float z0,float x1,float z1,float h,int t){ MBox b=mbox(x0,2,z0,x1,2+h,z1); rectSides(b,t,7,10,2,h); rectF(b,1,t,7,5,0,2,2); return b; };
   for(int cmp=0;cmp<2;cmp++){ const BlockId id=idOf(cmp?"comparator":"repeater"); const int top=T(cmp?"comparator":"repeater"),torch=T("redstone_torch"),stone=T("smooth_stone")>=0?T("smooth_stone"):T("stone"); if(!id||top<0)continue;
    Model m; MBox b=mbox(0,0,0,16,2,16); allF(b,stone>=0?stone:top); rectF(b,1,top,0,0); m.boxes.push_back(b);
    if(torch>=0){ if(!cmp){ m.boxes.push_back(torchBox(7,2,9,4,5,torch)); m.boxes.push_back(torchBox(7,6,9,8,5,torch)); } else { m.boxes.push_back(torchBox(4,11,6,13,5,torch)); m.boxes.push_back(torchBox(10,11,12,13,5,torch)); m.boxes.push_back(torchBox(7,2,9,4,3,torch)); } }
    setM(id,m); } }

 // ---- redstone wire: a flat quad with the dot sprite
 { const BlockId id=idOf("redstone_wire"); const int t=T("redstone_dust_dot"); if(id&&t>=0){ Model m; m.quads.push_back(flatUp(0,0,16,16,1,t,0,0,16,16)); setM(id,m); } }

 // ---- beds: 16x16x9 body whose side textures already contain the legs; head / foot halves; written head north, turned for the other facings
 { static const char*cols[]={"white","orange","magenta","light_blue","yellow","lime","pink","gray","light_gray","cyan","purple","blue","brown","green","red","black"};
   for(const char*cn:cols){ const std::string col=cn; const BlockId base=idOf(col+"_bed"); if(!base)continue;
    const int hu=T(col+"_bed_head_up"),he=T(col+"_bed_head_east"),hw=T(col+"_bed_head_west"),hn=T("bed_head_north"),fu=T(col+"_bed_foot_up"),fe=T(col+"_bed_foot_east"),fw=T(col+"_bed_foot_west"),fs=T(col+"_bed_foot_south"),down=T("bed_down");
    if(hu<0||fu<0||he<0||fe<0)continue;
    for(int head=0;head<2;head++){ Model m; MBox b=mbox(0,0,0,16,9,16);
     if(head){ rectF(b,1,hu,0,0); rectF(b,3,he,0,7); rectF(b,2,hw,0,7); if(hn>=0)rectF(b,4,hn,0,7); }
     else { rectF(b,1,fu,0,0); rectF(b,3,fe,0,7); rectF(b,2,fw,0,7); if(fs>=0)rectF(b,5,fs,0,7); }
     m.boxes.push_back(b);
     if(down>=0){ MBox u=mbox(0,3,0,16,3,16); rectF(u,0,down,0,0); u.cullBorder=false; m.boxes.push_back(u); }
     for(int f=0;f<4;f++){ const int idx=head*4+f; const BlockId v=idx==0?base:var(base,idx); if(v)setM(v,rotY(m,f)); } } } }

 // ---- redstone dust: flat patterns for every combination of connections (n=1 e=2 s=4 w=8): dot, straight line, corners, T, cross
 { redstoneWire=idOf("redstone_wire"); const int dot=T("redstone_dust_dot"),line=T("redstone_dust_line0"); m_wire.clear();
   if(redstoneWire&&dot>=0&&line>=0){ m_wire.resize(16);
    auto quad=[&](Model&m,float x0,float z0,float x1,float z1,int tex,bool alongX){ MQuad q; q.tex=(int16_t)tex; q.twoSided=false; const float P[4][2]={{x0,z0},{x0,z1},{x1,z1},{x1,z0}};
     for(int i=0;i<4;i++){ q.p[i][0]=P[i][0]; q.p[i][1]=1; q.p[i][2]=P[i][1]; q.uv[i][0]=alongX?P[i][1]:P[i][0]; q.uv[i][1]=alongX?P[i][0]:P[i][1]; } m.quads.push_back(q); };
    for(int mask=0;mask<16;mask++){ Model& m=m_wire[(size_t)mask]; const bool n=mask&1,e=mask&2,s=mask&4,w=mask&8; const int cnt=n+e+s+w;
     if(cnt==0)quad(m,0,0,16,16,dot,false);
     else if(!e&&!w&&(n||s)&&cnt<=2)quad(m,0,0,16,16,line,false);          // straight along z (also a single arm: it runs through the block)
     else if(!n&&!s&&(e||w)&&cnt<=2)quad(m,0,0,16,16,line,true);
     else { quad(m,0,0,16,16,dot,false); if(n)quad(m,0,0,16,8,line,false); if(s)quad(m,0,8,16,16,line,false); if(e)quad(m,8,0,16,16,line,true); if(w)quad(m,0,0,8,16,line,true); }
     finalize(m); } } }

 // ---- beacon: glass shell, obsidian base, the beacon core inside
 { const BlockId id=idOf("beacon"); const int core=T("beacon"),glass=T("glass"),obs=T("obsidian"); if(id&&core>=0){ Model m;
    MBox o=mbox(0,0,0,16,16,16); allF(o,glass>=0?glass:core); MBox b=mbox(2,0,2,14,3,14); allF(b,obs>=0?obs:core); b.cullBorder=false;
    MBox c=mbox(3,3,3,13,13,13); c.cullBorder=false; for(int f=0;f<6;f++)rectF(c,f,core,0,0,0,16,16);
    m.boxes={o,b,c}; setM(id,m); } }

 // ---- brewing stand: rod with its T bar, three base plates
 { const BlockId id=idOf("brewing_stand"); const int t=T("brewing_stand"),base=T("brewing_stand_base"); if(id&&t>=0){ Model m;
    MBox rod=mbox(7,0,7,9,14,9); allF(rod,t); MBox bar=mbox(5,12,7,11,14,9); allF(bar,t);
    m.boxes={rod,bar}; const float P[3][4]={{9,5,15,11},{1,1,7,7},{1,9,7,15}};
    for(auto&p:P){ MBox b=mbox(p[0],0,p[1],p[2],2,p[3]); allF(b,base>=0?base:t); m.boxes.push_back(b); } setM(id,m); } }

 // ---- chains: two crossed 3 wide planes (texture columns 0..3 and 3..6)
 for(const char*cn:{"iron_chain","copper_chain","exposed_copper_chain","weathered_copper_chain","oxidized_copper_chain"}){ const BlockId id=idOf(cn); const int t=T(cn); if(!id||t<0)continue; Model m;
   m.quads.push_back(plane(6.5f,0,8,9.5f,16,8,t,0,0,3,16)); m.quads.push_back(plane(8,0,6.5f,8,16,9.5f,t,3,0,6,16)); setM(id,m); }

 // ---- end rod / lightning rods
 { const BlockId id=idOf("end_rod"); const int t=T("end_rod"); if(id&&t>=0){ Model m; MBox s=mbox(7,1,7,9,16,9); rectSides(s,t,0,0,2,15); rectF(s,1,t,2,2,0,2,2); rectF(s,0,t,2,2,0,2,2); MBox b=mbox(6,0,6,10,1,10); for(int f=0;f<6;f++)rectF(b,f,t,3,2,0,4,3); m.boxes={s,b}; setM(id,m); } }
 for(const char*rn:{"lightning_rod","exposed_lightning_rod","weathered_lightning_rod","oxidized_lightning_rod"}){ const BlockId id=idOf(rn); const int t=T(rn); if(!id||t<0)continue; Model m;
   MBox s=mbox(7,0,7,9,12,9); rectSides(s,t,0,0,2,12); rectF(s,1,t,0,0,0,2,2); rectF(s,0,t,0,0,0,2,2);
   MBox head=mbox(6,12,6,10,15,10); rectSides(head,t,0,0,4,3); rectF(head,1,t,0,0,0,4,4); rectF(head,0,t,0,0,0,4,4);
   MBox tip=mbox(7,15,7,9,16,9); rectSides(tip,t,0,0,2,1); rectF(tip,1,t,0,0,0,2,2); m.boxes={s,head,tip}; setM(id,m); }

 // ---- lily pad: one flat quad just above the water
 { const BlockId id=idOf("lily_pad"); const int t=T("lily_pad"); if(id&&t>=0){ Model m; m.quads.push_back(flatUp(0,0,16,16,1,t,0,0,16,16)); setM(id,m); } }

 // ---- end portal: the starry surface, 12/16 up, visible from both sides
 { const BlockId id=idOf("end_portal"); const int t=T("end_portal_surface"); if(id&&t>=0){ Model m; MBox b=mbox(0,12,0,16,12,16); b.twoSided=true; b.cullBorder=false; rectF(b,1,t,0,0); m.boxes.push_back(b); setM(id,m); } }

 // ---- dried ghast: 10x10x10 body with its face textures, four stubby tentacles
 { const BlockId id=idOf("dried_ghast"); if(id){ const char*sf[6]={"bottom","top","west","east","north","south"}; int tx[6]; for(int i=0;i<6;i++)tx[i]=T(std::string("dried_ghast_hydration_0_")+sf[i]);
    if(tx[0]>=0&&tx[4]>=0){ Model m; MBox b=mbox(3,3,3,13,13,13); for(int f=0;f<6;f++)rectF(b,f,tx[f],3,3); m.boxes.push_back(b);
     for(auto&p:std::vector<std::array<float,2>>{{4,4},{10,4},{4,10},{10,10}}){ MBox tn=mbox(p[0],0,p[1],p[0]+2,3,p[1]+2); allF(tn,tx[0]); m.boxes.push_back(tn); } setM(id,m); } } }

 // ---- decorated pot: round body, narrow neck, lip
 { const BlockId id=idOf("decorated_pot"); int t=T("decorated_pot_side"); if(t<0)t=T("terracotta"); if(id&&t>=0){ Model m;
    MBox body=mbox(1,0,1,15,13,15); allF(body,t); MBox neck=mbox(5,13,5,11,15,11); allF(neck,t); MBox lip=mbox(4,15,4,12,16,12); allF(lip,t); m.boxes={body,neck,lip}; setM(id,m); } }

 // ---- cocoa pod (against a tree): box + leaf, sizes grow with the stage texture used by the block
 { const BlockId id=idOf("cocoa"); if(id){ const int t=m_blocks[id].texture[2]; if(t>=0){ Model m; MBox pod=mbox(5,3,9,11,12,15); for(int f=0;f<6;f++)rectF(pod,f,t,0,0,0,8,8); m.boxes.push_back(pod); setM(id,m); } } }

 // ---- shelf mushroom: two stacked caps (best effort: the pack ships it as a 32x32 sheet)
 { const BlockId id=idOf("shelf_mushroom"); int t=T("shelf_mushroom_stage1"); if(t<0)t=T("shelf_mushroom_stage0"); if(id&&t>=0){ Model m;
    MBox c1=mbox(2,3,6,14,5,15); rectF(c1,1,t,0,0,0,16,8); for(int f=2;f<6;f++)rectF(c1,f,t,0,8,0,16,4); rectF(c1,0,t,0,8,0,16,8);
    MBox c2=mbox(4,7,8,13,9,15); rectF(c2,1,t,0,0,0,16,8); for(int f=2;f<6;f++)rectF(c2,f,t,0,8,0,16,4); rectF(c2,0,t,0,8,0,16,8); m.boxes={c1,c2}; setM(id,m); } }

 // ---- tripwire hook: the hook plate on a wall plane
 { const BlockId id=idOf("tripwire_hook"); const int t=T("tripwire_hook"); if(id&&t>=0){ Model m; m.quads.push_back(plane(0,0,2.5f,16,16,2.5f,t,0,0,16,16)); setM(id,m); } }
}

int Registry::modelBoxes(BlockId id,Box*out,int maxN)const{
 const Model*md=modelOf(id); if(!md)return 0; int n=0;
 for(const MBox&b:md->boxes){
  if(b.twoSided||(b.trans&&!b.cullBorder))continue;                 // string, portal surface, the inner cube of honey / slime
  const float dx=b.x1-b.x0,dy=b.y1-b.y0,dz=b.z1-b.z0; if(dx<0.5f||dy<0.5f||dz<0.5f)continue;
  if(dx*dz<16.f)continue;                                           // legs, rods, torches: too small to matter
  if(n>=maxN)break; auto q=[](float v){ return (uint8_t)std::clamp((int)std::lround(v),0,255); };
  out[n++]=Box{q(b.x0),q(b.y0),q(b.z0),q(b.x1),q(b.y1),q(b.z1)}; }
 return n;
}
