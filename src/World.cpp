#include "World.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <mutex>
#include "Perf.hpp"

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

// ---- biomes (vanilla 1.18+ layout: ocean / shore / river bands by continentalness+weirdness, then the temperature x humidity tables)
enum Biome{B_OCEAN,B_COLD_OCEAN,B_FROZEN_OCEAN,B_WARM_OCEAN,B_DEEP_OCEAN,B_DEEP_COLD,B_DEEP_FROZEN,B_BEACH,B_SNOWY_BEACH,B_STONY_SHORE,B_RIVER,B_FROZEN_RIVER,
 B_PLAINS,B_SNOWY_PLAINS,B_FOREST,B_FLOWER_FOREST,B_BIRCH,B_OLD_BIRCH,B_DARK_FOREST,B_TAIGA,B_SNOWY_TAIGA,B_OLD_SPRUCE,B_OLD_PINE,B_DESERT,B_SAVANNA,B_JUNGLE,B_SWAMP,B_BADLANDS,
 B_MEADOW,B_GROVE,B_SNOWY_SLOPES,B_JAGGED_PEAKS,B_FROZEN_PEAKS,B_STONY_PEAKS,B_WINDSWEPT,B_WINDSWEPT_FOREST};
inline int tIdx(float t){return t<-0.45f?0:t<-0.15f?1:t<0.2f?2:t<0.55f?3:4;}
inline int hIdx(float h){return h<-0.35f?0:h<-0.1f?1:h<0.1f?2:h<0.3f?3:4;}
inline int eIdx(float e){return e<-0.78f?0:e<-0.375f?1:e<-0.2225f?2:e<0.05f?3:e<0.45f?4:e<0.55f?5:6;}
int pickBiome(const ClimateSample&s){
 const int t=tIdx(s.t),h=hIdx(s.h),ei=eIdx(s.e); const float c=s.c,w=s.w,aw=std::fabs(w);
 if(c<-0.455f){ static const int deep[5]={B_DEEP_FROZEN,B_DEEP_COLD,B_DEEP_OCEAN,B_DEEP_OCEAN,B_WARM_OCEAN}; return deep[t]; }
 if(c<-0.19f){ static const int oc[5]={B_FROZEN_OCEAN,B_COLD_OCEAN,B_OCEAN,B_OCEAN,B_WARM_OCEAN}; return oc[t]; }
 const bool valley=aw<0.05f;
 if(c<-0.11f){                                           // coast
  if(valley)return t==0?B_FROZEN_RIVER:B_RIVER;
  if(w<=0.27f){ if(w>0.05f&&s.e<-0.2225f)return B_STONY_SHORE; return t==0?B_SNOWY_BEACH:(t==4?B_DESERT:B_BEACH); }
 } else if(valley)return t==0?B_FROZEN_RIVER:B_RIVER;
 // inland
 const bool inlandFar=c>=0.03f;
 static const int mid[5][5]={{B_SNOWY_PLAINS,B_SNOWY_PLAINS,B_SNOWY_PLAINS,B_SNOWY_TAIGA,B_TAIGA},{B_PLAINS,B_PLAINS,B_FOREST,B_TAIGA,B_OLD_SPRUCE},{B_FLOWER_FOREST,B_PLAINS,B_FOREST,B_BIRCH,B_DARK_FOREST},{B_SAVANNA,B_SAVANNA,B_FOREST,B_JUNGLE,B_JUNGLE},{B_DESERT,B_DESERT,B_DESERT,B_DESERT,B_DESERT}};
 static const int plateau[5][5]={{B_SNOWY_PLAINS,B_SNOWY_PLAINS,B_SNOWY_PLAINS,B_SNOWY_TAIGA,B_SNOWY_TAIGA},{B_MEADOW,B_MEADOW,B_FOREST,B_TAIGA,B_OLD_SPRUCE},{B_MEADOW,B_MEADOW,B_MEADOW,B_MEADOW,B_DARK_FOREST},{B_SAVANNA,B_SAVANNA,B_FOREST,B_FOREST,B_JUNGLE},{B_BADLANDS,B_BADLANDS,B_BADLANDS,B_BADLANDS,B_BADLANDS}};
 auto middle=[&](){ int b=mid[t][h]; if(w>=0.f){ if(t==1&&h==4)b=B_OLD_PINE; else if(t==2&&h==3)b=B_OLD_BIRCH; else if(t==0&&h==0)b=B_SNOWY_PLAINS; } return b; };
 const bool peakBand=(aw>=0.5667f&&aw<0.7667f);
 if(inlandFar&&ei<=2&&peakBand){ if(t<=2)return w<0?B_JAGGED_PEAKS:B_FROZEN_PEAKS; if(t==3)return B_STONY_PEAKS; return B_BADLANDS; }
 const bool highBand=(aw>=0.4f&&aw<0.5667f)||(aw>=0.7667f&&aw<0.9333f);
 if(inlandFar&&ei<=1&&highBand){ if(t<3)return h<=1?B_SNOWY_SLOPES:B_GROVE; return plateau[t][h]; }
 if(inlandFar&&ei<=3&&aw>=0.4f&&c>=0.3f&&w<0.f)return plateau[t][h];                        // plateau (meadow ...)
 if(c>=-0.11f&&ei<=1&&aw>=0.2667f){ if(t<4)return h<=1?B_WINDSWEPT:(h==2?B_WINDSWEPT:B_WINDSWEPT_FOREST); return B_BADLANDS; }
 if(ei==6&&t>=1&&t<=3&&h>=3&&c<0.3f)return B_SWAMP;
 return middle();
}
}

World::World(){}
void World::reset(uint64_t seed){std::unique_lock<std::shared_mutex> l(m_mu);m_chunks.clear();m_dirty.clear();m_loader=nullptr;m_seed64=seed;m_seed=uint32_t(seed)^(uint32_t(seed>>32)*2654435761u);m_climate=std::make_shared<Climate>(seed);}
void World::setLoader(Loader l){std::unique_lock<std::shared_mutex> g(m_mu);m_loader=std::move(l);}

// Terrain column: surface height from the vanilla offset spline sampled on the 4-block grid and interpolated (as the real game does), plus local detail.
World::Column World::columnAt(int wx,int wz)const{
 struct Q{float h; ClimateSample s;};
 thread_local const Climate* owner=nullptr; thread_local std::unordered_map<uint64_t,Q> cache;
 const Climate*cl=m_climate.get(); if(owner!=cl||cache.size()>20000){cache.clear();owner=cl;}
 auto quart=[&](int qx,int qz)->const Q&{
  const uint64_t k=(uint64_t(uint32_t(qx))<<32)|uint32_t(qz); auto it=cache.find(k); if(it!=cache.end())return it->second;
  Q q; q.s=cl->at(qx*4,qz*4); q.h=63.52f+128.f*Climate::offset(q.s.c,q.s.e,q.s.w)+0.7f+float(Chunk::Y_OFFSET); return cache.emplace(k,q).first->second; };
 const int qx=wx>>2,qz=wz>>2; const float fx=float(wx-qx*4)/4.f,fz=float(wz-qz*4)/4.f;
 const float h00=quart(qx,qz).h,h10=quart(qx+1,qz).h,h01=quart(qx,qz+1).h,h11=quart(qx+1,qz+1).h;
 float h=(h00+(h10-h00)*fx)*(1.f-fz)+(h01+(h11-h01)*fx)*fz;
 const ClimateSample&s=quart(qx,qz).s;
 const float rough=std::clamp((0.2f-s.e)/1.0f,0.f,1.f); const float det=(fbm(float(wx)*0.025f,float(wz)*0.025f,m_seed+77u,3)-0.5f)*2.f*(h<float(SEA_LEVEL)?2.5f:(2.f+5.f*rough));
 h+=det;
 Column c; c.top=std::clamp((int)std::floor(h+0.5f),6,Chunk::HEIGHT-30); c.biome=pickBiome(s); c.c=s.c;c.e=s.e;c.t=s.t;c.h=s.h;c.w=s.w; return c;
}
int World::terrainHeight(int wx,int wz)const{ return columnAt(wx,wz).top; }

// Debug world: one block of every kind (all slab/stair/rotation variants) every second block, on the plane y = 70 (Minecraft numbering).
void World::generateDebug(int cx,int cz,Chunk&c)const{
 const Registry&R=Registry::get(); std::vector<uint16_t> ids; for(uint16_t i=1;i<R.blockCount();i++)if(i!=R.missing)ids.push_back(i);
 const int gw=32,y=Chunk::Y_OFFSET+70;
 for(int lz=0;lz<16;lz++)for(int lx=0;lx<16;lx++){ const int wx=cx*16+lx,wz=cz*16+lz; if(wx<0||wz<0||(wx&1)||(wz&1))continue; if(wx/2>=gw)continue; const size_t idx=size_t(wz/2)*gw+size_t(wx/2); if(idx<ids.size()){ c.at(lx,y,lz)=ids[idx]; c.maxY=std::max(c.maxY,y); } }
}
void World::generate(int cx,int cz,Chunk&c)const{
 if(m_debug){generateDebug(cx,cz,c);return;}
 PerfScope ps("terrain generation of one chunk",60);
 const Registry&R=Registry::get(); const int S=Chunk::SIZE,H=Chunk::HEIGHT,sea=SEA_LEVEL;
 auto B=[&](const char*n,uint16_t fb){const uint16_t v=R.blockByName(n);return v?v:fb;};
 const uint16_t stone=R.stone,dirt=R.dirt,grass=R.grass,sand=R.sand,water=R.water,gravel=R.gravel?R.gravel:R.dirt,bedrock=R.bedrock?R.bedrock:R.stone;
 const uint16_t deepslate=B("deepslate",stone),sandstone=B("sandstone",sand),redSand=B("red_sand",sand),redSandstone=B("red_sandstone",sandstone),snow=B("snow_block",grass),ice=B("ice",water),lava=B("lava",0),podzol=B("podzol",dirt),coarse=B("coarse_dirt",dirt),mud=B("mud",dirt),clay=B("clay",dirt),packed=B("packed_ice",snow),tuff=B("tuff",stone),calcite=B("calcite",stone);
 const uint16_t terra[7]={B("terracotta",stone),B("orange_terracotta",stone),B("yellow_terracotta",stone),B("red_terracotta",stone),B("white_terracotta",stone),B("brown_terracotta",stone),B("light_gray_terracotta",stone)};
 auto put=[&](int lx,int y,int lz,uint16_t id){ if(y<0||y>=H)return; c.at(lx,y,lz)=id; if(id&&y>c.maxY)c.maxY=y; };
 Column col[16][16];
 for(int lz=0;lz<S;lz++)for(int lx=0;lx<S;lx++)col[lz][lx]=columnAt(cx*S+lx,cz*S+lz);
 for(int lz=0;lz<S;lz++)for(int lx=0;lx<S;lx++){
  const int wx=cx*S+lx,wz=cz*S+lz; const Column&cl=col[lz][lx]; const int top=cl.top; const int bm=cl.biome; const int mcTop=top-Chunk::Y_OFFSET;
  uint16_t surf=grass,sub=dirt; int subDepth=3; bool bands=false;
  switch(bm){
   case B_OCEAN: case B_WARM_OCEAN: if(top>=sea-8){surf=sub=sand;} else if(((hash2(wx>>3,wz>>3,m_seed)>>4)&3u)==0u){surf=sub=clay;} else {surf=sub=gravel;} break;
   case B_DEEP_OCEAN: case B_COLD_OCEAN: case B_DEEP_COLD: surf=sub=(bm==B_DEEP_OCEAN&&((hash2(wx>>3,wz>>3,m_seed)>>4)&3u)==0u)?clay:gravel; if(top>=sea-3&&bm==B_COLD_OCEAN)surf=sub=sand; break;
   case B_FROZEN_OCEAN: case B_DEEP_FROZEN: surf=sub=gravel; break;
   case B_BEACH: surf=sub=sand; subDepth=4; break;
   case B_SNOWY_BEACH: surf=sub=sand; subDepth=4; break;
   case B_STONY_SHORE: surf=sub=stone; subDepth=2; break;
   case B_RIVER: case B_FROZEN_RIVER: surf=sub=(top<=sea)?(((hash2(wx>>2,wz>>2,m_seed+3u)>>4)&1u)?gravel:sand):dirt; break;
   case B_DESERT: surf=sub=sand; subDepth=4; break;
   case B_BADLANDS: surf=redSand; sub=terra[0]; subDepth=8; bands=true; break;
   case B_SAVANNA: surf=grass; sub=dirt; if(((hash2(wx>>2,wz>>2,m_seed+5u)>>5)&7u)==0u)surf=coarse; break;
   case B_SWAMP: surf=grass; sub=dirt; if(top<=sea+1){surf=mud;sub=mud;} break;
   case B_TAIGA: case B_OLD_SPRUCE: case B_OLD_PINE: surf=((hash2(wx>>2,wz>>2,m_seed+9u)>>5)&3u)==0u?podzol:grass; if(bm==B_OLD_SPRUCE||bm==B_OLD_PINE)surf=((hash2(wx>>2,wz>>2,m_seed+9u)>>5)&1u)?podzol:coarse; break;
   case B_SNOWY_PLAINS: case B_SNOWY_TAIGA: surf=snow; sub=dirt; break;
   case B_SNOWY_SLOPES: surf=snow; sub=stone; subDepth=2; break;
   case B_GROVE: surf=snow; sub=dirt; break;
   case B_JAGGED_PEAKS: case B_FROZEN_PEAKS: surf=(mcTop>110)?snow:stone; sub=stone; subDepth=2; if(bm==B_FROZEN_PEAKS&&mcTop>150&&(hash2(wx>>2,wz>>2,m_seed+13u)&3u)==0u)surf=packed; break;
   case B_STONY_PEAKS: surf=sub=stone; subDepth=1; if(((hash2(wx>>2,wz>>2,m_seed+21u)>>3)&7u)==0u)surf=calcite; else if(((hash2(wx>>2,wz>>2,m_seed+21u)>>3)&7u)==1u)surf=tuff; break;
   case B_WINDSWEPT: case B_WINDSWEPT_FOREST: if(mcTop>96){surf=sub=stone;subDepth=1;} else if(((hash2(wx>>3,wz>>3,m_seed+31u)>>3)&7u)<2u){surf=sub=gravel;} break;
   default: break;
  }
  for(int y=0;y<=top;y++){
   uint16_t id;
   if(y==0)id=bedrock; else if(y<=4&&(hash3(wx,y,wz,m_seed)&7u)<uint32_t(5-y))id=bedrock;
   else if(y==top)id=surf;
   else if(y>=top-subDepth){ id=sub; if(bands){ const int band=((y+int(hash2(wx>>4,wz>>4,m_seed+41u)&1u))/2+int(m_seed&7u))%7; id=(y>=top-1)?redSand:terra[band]; } else if((bm==B_DESERT||bm==B_BEACH||bm==B_SNOWY_BEACH)&&y<top-2)id=sandstone; }
   else if(y<64)id=(y>=56&&(hash3(wx,y,wz,m_seed+3u)&7u)>=uint32_t(y-56))?stone:deepslate;
   else id=stone;
   put(lx,y,lz,id);
  }
  const bool frozen=(bm==B_FROZEN_OCEAN||bm==B_DEEP_FROZEN||bm==B_FROZEN_RIVER);
  for(int y=top+1;y<=sea;y++)put(lx,y,lz,(frozen&&y==sea)?ice:water);
 }
 // caves (3D noise, only well below the surface so they never open the sea or the ground by accident)
 for(int lz=0;lz<S;lz++)for(int lx=0;lx<S;lx++){
  const int wx=cx*S+lx,wz=cz*S+lz,top=col[lz][lx].top,maxCave=std::min(top-5,col[lz][lx].top<sea?top-8:top-4);
  for(int y=7;y<=maxCave;y++){
   const float fx=float(wx),fy=float(y),fz=float(wz);
   const float n=vnoise3(fx*0.032f,fy*0.045f,fz*0.032f,m_seed+301u)*0.62f+vnoise3(fx*0.075f,fy*0.095f,fz*0.075f,m_seed+302u)*0.38f;
   const float worm=std::fabs(vnoise3(fx*0.02f,fy*0.03f,fz*0.02f,m_seed+303u)-0.5f)+std::fabs(vnoise3(fx*0.02f,fy*0.03f+40.f,fz*0.02f,m_seed+304u)-0.5f);
   if(n>0.665f||worm<0.028f)put(lx,y,lz,y<=14?lava:0);
  }
 }
 // ores: deterministic veins per chunk (kept inside the chunk)
 struct Ore{const char*name,*deep;int count,size,yMin,yMax;};
 static const Ore ores[]={{"coal_ore","deepslate_coal_ore",20,9,64+0,64+190},{"iron_ore","deepslate_iron_ore",14,6,64-30,64+70},{"copper_ore","deepslate_copper_ore",9,8,64-14,64+110},
  {"gold_ore","deepslate_gold_ore",4,6,0,64+30},{"redstone_ore","deepslate_redstone_ore",6,5,0,64+14},{"lapis_ore","deepslate_lapis_ore",2,5,64-30,64+30},{"diamond_ore","deepslate_diamond_ore",3,4,0,64+14}};
 for(size_t oi=0;oi<sizeof(ores)/sizeof(ores[0]);oi++){
  const Ore&o=ores[oi]; const uint16_t a=R.blockByName(o.name); if(!a)continue; const uint16_t dp=B(o.deep,a);
  for(int n=0;n<o.count;n++){
   uint32_t hh=hash2(cx,cz,m_seed+uint32_t(oi*977+n*131+17)); const int bx=2+(int)(hh%12u),bz=2+(int)((hh>>8)%12u),by=o.yMin+(int)((hh>>16)%uint32_t(std::max(1,o.yMax-o.yMin)));
   for(int k=0;k<o.size;k++){ hh=hash32(hh+0x9e3779b9u); const int x=bx+(int)(hh%3u)-1+(k%2),y=by+(int)((hh>>4)%3u)-1,z=bz+(int)((hh>>8)%3u)-1+(k/2%2);
    if(x<0||x>=S||z<0||z>=S||y<1||y>=H)continue; const uint16_t cur=c.at(x,y,z); if(cur==stone)c.at(x,y,z)=a; else if(cur==deepslate)c.at(x,y,z)=dp; }
  }
 }
 // grass, ferns, flowers, dead bushes
 { const uint16_t pg=B("short_grass",0),pf=B("fern",0),pd=B("dead_bush",0); static const char*fl[]={"dandelion","poppy","azure_bluet","oxeye_daisy","cornflower","red_tulip","pink_tulip","allium"}; uint16_t fid[8]; for(int i=0;i<8;i++)fid[i]=B(fl[i],pg);
   for(int lz=0;lz<S;lz++)for(int lx=0;lx<S;lx++){ const Column&cl=col[lz][lx]; const int t=cl.top; if(t<=sea||t+1>=H||c.at(lx,t+1,lz)!=0)continue; const uint16_t sf=c.at(lx,t,lz); const int wx=cx*S+lx,wz=cz*S+lz; const uint32_t r=hash2(wx,wz,m_seed+999u)%1000u,k=(hash2(wx,wz,m_seed+31u)>>8)&7u;
    uint16_t put1=0; const bool onGrass=sf==grass||sf==podzol||sf==coarse; const bool onSand=(sf==sand||sf==redSand)&&(cl.biome==B_DESERT||cl.biome==B_BADLANDS);
    if(onGrass){ switch(cl.biome){ case B_PLAINS: put1=r<150?pg:(r<165?fid[k]:0); break; case B_FLOWER_FOREST: put1=r<90?pg:(r<260?fid[k]:0); break; case B_MEADOW: put1=r<120?pg:(r<190?fid[k]:0); break; case B_FOREST: case B_BIRCH: case B_OLD_BIRCH: put1=r<90?pg:(r<95?fid[k&3]:0); break; case B_SAVANNA: put1=r<160?pg:0; break; case B_TAIGA: case B_OLD_SPRUCE: case B_OLD_PINE: case B_SNOWY_TAIGA: put1=r<110?((k&1)?pf:pg):0; break; case B_JUNGLE: put1=r<220?((k&1)?pf:pg):0; break; case B_SWAMP: put1=r<70?pg:0; break; case B_DARK_FOREST: put1=r<40?pg:0; break; case B_GROVE: case B_WINDSWEPT: case B_WINDSWEPT_FOREST: put1=r<45?pg:0; break; default: break; } }
    else if(onSand&&r<10)put1=pd;
    if(put1)put(lx,t+1,lz,put1); } }
 // trees / cacti: deterministic per world column, so trees crossing a chunk border are identical from both sides
 auto inside=[&](int x,int z){return x>=cx*S&&x<cx*S+S&&z>=cz*S&&z<cz*S+S;};
 for(int tz=cz*S-3;tz<cz*S+S+3;tz++)for(int tx=cx*S-3;tx<cx*S+S+3;tx++){
  const Column tc=columnAt(tx,tz); const uint32_t hh=hash2(tx,tz,m_seed^0x5bd1e995u);
  uint32_t rate=0; const char*logN="oak_log",*leafN="oak_leaves"; int kind=0;   // 0 oak, 1 birch, 2 spruce, 3 jungle, 4 acacia, 5 cactus, 6 tall birch, 7 tall spruce, 8 dark oak
  switch(tc.biome){
   case B_PLAINS: rate=260; break; case B_MEADOW: rate=320; break; case B_FOREST: rate=14; if((hh>>16)%5==0){logN="birch_log";leafN="birch_leaves";kind=1;} break; case B_FLOWER_FOREST: rate=22; if((hh>>16)%2){logN="birch_log";leafN="birch_leaves";kind=1;} break;
   case B_BIRCH: rate=13; logN="birch_log";leafN="birch_leaves";kind=1; break; case B_OLD_BIRCH: rate=13; logN="birch_log";leafN="birch_leaves";kind=6; break;
   case B_DARK_FOREST: rate=7; logN="dark_oak_log";leafN="dark_oak_leaves";kind=8; break;
   case B_TAIGA: rate=16; logN="spruce_log";leafN="spruce_leaves";kind=2; break; case B_SNOWY_TAIGA: rate=20; logN="spruce_log";leafN="spruce_leaves";kind=2; break;
   case B_OLD_SPRUCE: case B_OLD_PINE: rate=11; logN="spruce_log";leafN="spruce_leaves";kind=7; break; case B_GROVE: rate=28; logN="spruce_log";leafN="spruce_leaves";kind=2; break;
   case B_WINDSWEPT_FOREST: rate=17; logN="spruce_log";leafN="spruce_leaves";kind=2; break; case B_WINDSWEPT: rate=160; logN="spruce_log";leafN="spruce_leaves";kind=2; break;
   case B_JUNGLE: rate=9; logN="jungle_log";leafN="jungle_leaves";kind=3; break; case B_SAVANNA: rate=70; logN="acacia_log";leafN="acacia_leaves";kind=4; break;
   case B_SWAMP: rate=45; break; case B_DESERT: rate=170; kind=5; break; case B_BADLANDS: rate=260; kind=5; break; default: break; }
  if(!rate||hh%rate!=0)continue;
  const int th=tc.top; if(th<=sea||th+26>=H)continue;
  const uint16_t log=B(logN,R.log),leaf=B(leafN,R.leaves); if(!log||!leaf)continue;
  const int r1=(int)((hh>>8)%3u);
  auto leaves=[&](int x,int y,int z){ if(!inside(x,z)||y>=H)return; if(c.at(x-cx*S,y,z-cz*S)==0)put(x-cx*S,y,z-cz*S,leaf); };
  auto trunk=[&](int y){ if(inside(tx,tz))put(tx-cx*S,y,tz-cz*S,log); };
  if(kind==5){ const uint16_t cactus=B("cactus",0); if(cactus&&inside(tx,tz)){ const int hgt=1+r1; for(int y=th+1;y<=th+hgt;y++)put(tx-cx*S,y,tz-cz*S,cactus);} continue; }
  if(kind==2||kind==7){                                                    // spruce: cone (tall for old growth)
   const int trunkH=(kind==7?12:6)+r1*2; for(int y=th+1;y<=th+trunkH;y++)trunk(y);
   for(int dy=2;dy<=trunkH+1;dy++){ const int rad=std::max(0,(trunkH+1-dy)/2+((dy%2)?0:1)); const int rr=std::min(rad,kind==7?4:3); for(int dz=-rr;dz<=rr;dz++)for(int dx=-rr;dx<=rr;dx++){ if(dx==0&&dz==0&&dy<=trunkH)continue; if(std::abs(dx)+std::abs(dz)>rr+(rr>1?1:0))continue; leaves(tx+dx,th+dy,tz+dz);} }
   continue; }
  if(kind==3){ const int trunkH=9+r1*3; for(int y=th+1;y<=th+trunkH;y++)trunk(y);
   for(int dy=trunkH-2;dy<=trunkH+1;dy++){ const int rad=dy>=trunkH?2:3; for(int dz=-rad;dz<=rad;dz++)for(int dx=-rad;dx<=rad;dx++){ if(dx==0&&dz==0&&dy<=trunkH)continue; if(std::abs(dx)==rad&&std::abs(dz)==rad)continue; leaves(tx+dx,th+dy+1,tz+dz);} } continue; }
  if(kind==4){ const int trunkH=5+r1; for(int y=th+1;y<=th+trunkH;y++)trunk(y); for(int dz=-2;dz<=2;dz++)for(int dx=-2;dx<=2;dx++){ if(std::abs(dx)==2&&std::abs(dz)==2)continue; leaves(tx+dx,th+trunkH+1,tz+dz); if(std::abs(dx)<=1&&std::abs(dz)<=1)leaves(tx+dx,th+trunkH+2,tz+dz);} continue; }
  if(kind==8){ const int trunkH=6+r1; for(int y=th+1;y<=th+trunkH;y++)trunk(y); for(int dy=trunkH-2;dy<=trunkH+1;dy++){ const int rad=dy>=trunkH?2:3; for(int dz=-rad;dz<=rad;dz++)for(int dx=-rad;dx<=rad;dx++){ if(dx==0&&dz==0&&dy<=trunkH)continue; if(std::abs(dx)==rad&&std::abs(dz)==rad&&((hash2(tx+dx,tz+dz,uint32_t(dy))>>3)&1u))continue; leaves(tx+dx,th+dy+1,tz+dz);} } continue; }
  const int trunkH=(kind==6?9:(kind==1?5:4))+r1;                           // oak / birch / tall birch
  for(int y=th+1;y<=th+trunkH;y++)trunk(y);
  for(int dy=trunkH-2;dy<=trunkH+1;dy++){
   const int rad=dy>=trunkH?1:2;
   for(int dz=-rad;dz<=rad;dz++)for(int dx=-rad;dx<=rad;dx++){
    const int x=tx+dx,z=tz+dz,y=th+dy+1;
    if(dx==0&&dz==0&&dy<trunkH)continue;
    if(std::abs(dx)==rad&&std::abs(dz)==rad&&(rad==2||dy==trunkH+1)&&((hash2(x,z,uint32_t(y))>>3)&1u))continue;
    leaves(x,y,z);
   }
  }
 }
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
 if(cache){ std::unique_lock<std::shared_mutex> l(m_mu); auto r=m_chunks.emplace(k,std::shared_ptr<const Chunk>(c)); return r.first->second; }
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

void World::pruneCache(int cx,int cz,int radius){
 std::unique_lock<std::shared_mutex> l(m_mu);
 for(auto it=m_chunks.begin();it!=m_chunks.end();){
  const int ccx=(int)(int32_t)(it->first>>32),ccz=(int)(int32_t)(it->first&0xffffffffu);
  if(std::max(std::abs(ccx-cx),std::abs(ccz-cz))>radius)it=m_chunks.erase(it); else ++it;
 }
}
World::DirtyList World::dirtyChunks()const{ std::shared_lock<std::shared_mutex> l(m_mu); DirtyList r; r.reserve(m_dirty.size()); for(auto&kv:m_dirty)r.emplace_back(kv.first,kv.second); return r; }
void World::markSaved(const DirtyList&saved){ std::unique_lock<std::shared_mutex> l(m_mu); for(auto&s:saved){ auto it=m_dirty.find(s.first); if(it!=m_dirty.end()&&it->second==s.second)m_dirty.erase(it); } }
size_t World::dirtyCount()const{std::shared_lock<std::shared_mutex> l(m_mu);return m_dirty.size();}

Vec3 World::findSpawn()const{
 if(m_debug)return Vec3{0.5f,float(Chunk::Y_OFFSET+74),-4.5f};
 for(int r=0;r<=960;r+=16){
  const int steps=r==0?1:std::max(8,r/4);
  for(int i=0;i<steps;i++){
   const float a=6.2831853f*float(i)/float(steps); const int x=(int)std::lround(std::cos(a)*float(r)),z=(int)std::lround(std::sin(a)*float(r));
   const Column cl=columnAt(x,z); if(cl.top>=SEA_LEVEL+2&&cl.biome>=B_PLAINS&&cl.biome<B_MEADOW)return Vec3{float(x)+0.5f,float(cl.top)+1.01f,float(z)+0.5f};   // land that is not water / mountain
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
  if(bi.shape==255){   // crossed plant: two diagonal quads, double sided
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
  if(bi.shape){   // boxes (slabs, stairs, carpets, torches, chests)
   for(const Box&bx:R.boxes(bi.shape)){
    const float fx0=bx.x0/16.f,fy0=bx.y0/16.f,fz0=bx.z0/16.f,fx1=bx.x1/16.f,fy1=bx.y1/16.f,fz1=bx.z1/16.f;
    for(int face=0;face<6;face++){
     // a face lying on the block border is hidden by a solid cube next to it
     const int nx=face==2?-1:face==3?1:0,ny=face==0?-1:face==1?1:0,nz=face==4?-1:face==5?1:0;
     const bool onBorder=(face==0&&bx.y0==0)||(face==1&&bx.y1==16)||(face==2&&bx.x0==0)||(face==3&&bx.x1==16)||(face==4&&bx.z0==0)||(face==5&&bx.z1==16);
     if(onBorder){ const uint16_t n=nb(x+nx,y+ny,z+nz); if(n!=0&&!R.info(n).transparent)continue; }
     const uint32_t tex=bi.texture[face];
     switch(face){
      case 0: addQuad(out,false,0,float(x)+fx0,float(y)+fy0,float(z)+fz0,fx1-fx0,fz1-fz0,tex,fz0,false,fx0); break;
      case 1: addQuad(out,false,1,float(x)+fx0,float(y)+fy1,float(z)+fz0,fx1-fx0,fz1-fz0,tex,fz0,false,fx0); break;
      case 2: addQuad(out,false,2,float(x)+fx0,float(y)+fy0,float(z)+fz0,fz1-fz0,fy1-fy0,tex,1.f-fy1,false,fz0); break;
      case 3: addQuad(out,false,3,float(x)+fx1,float(y)+fy0,float(z)+fz0,fz1-fz0,fy1-fy0,tex,1.f-fy1,false,1.f-fz1); break;
      case 4: addQuad(out,false,4,float(x)+fx0,float(y)+fy0,float(z)+fz0,fx1-fx0,fy1-fy0,tex,1.f-fy1,false,1.f-fx1); break;
      default: addQuad(out,false,5,float(x)+fx0,float(y)+fy0,float(z)+fz1,fx1-fx0,fy1-fy0,tex,1.f-fy1,false,fx0); break;
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
