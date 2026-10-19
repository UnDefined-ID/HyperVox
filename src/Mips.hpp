#pragma once
// Mip chains for the block atlas.  The atlas is made of 16x16 tiles, so levels 0..4 (16 -> 1 px per tile) never mix neighbouring tiles.
#include <algorithm>
#include <cstdint>
#include <vector>
inline void mipDown(const uint8_t*src,uint32_t w,uint32_t h,uint8_t*dst){   // 2x2 box filter, colour weighted by alpha
 const uint32_t nw=std::max(1u,w/2),nh=std::max(1u,h/2);
 for(uint32_t y=0;y<nh;y++)for(uint32_t x=0;x<nw;x++){
  uint32_t r=0,g=0,b=0,a=0,aw=0;
  for(int k=0;k<4;k++){ const uint32_t sx=std::min(w-1,x*2+(k&1)),sy=std::min(h-1,y*2+(k>>1)); const uint8_t*p=src+((size_t)sy*w+sx)*4; r+=p[0]*p[3];g+=p[1]*p[3];b+=p[2]*p[3];a+=p[3];aw+=p[3]; }
  uint8_t*d=dst+((size_t)y*nw+x)*4;
  if(aw){d[0]=(uint8_t)(r/aw);d[1]=(uint8_t)(g/aw);d[2]=(uint8_t)(b/aw);} else {d[0]=d[1]=d[2]=0;}
  d[3]=(uint8_t)(a/4);
 }
}
// All levels concatenated; offsets[l] = byte offset of level l.
inline std::vector<uint8_t> buildMipChain(const uint8_t*rgba,uint32_t W,uint32_t H,int levels,std::vector<size_t>&offsets){
 std::vector<uint8_t> all; offsets.clear(); uint32_t w=W,h=H;
 for(int l=0;l<levels;l++){
  offsets.push_back(all.size()); const size_t bytes=(size_t)w*h*4; all.resize(all.size()+bytes);
  if(l==0)std::copy(rgba,rgba+bytes,all.begin()+(long)offsets[0]);
  else mipDown(all.data()+offsets[l-1],std::max(1u,w*2>W?W:w*2),std::max(1u,h*2>H?H:h*2),all.data()+offsets[l]);
  w=std::max(1u,w/2);h=std::max(1u,h/2);
 }
 return all;
}
// One animated 16x16 tile -> its levels 1..4 (8x8, 4x4, 2x2, 1x1) stored back to back: 256+64+16+4 bytes.
inline void tileMips(const uint8_t*t16,uint8_t*out){ mipDown(t16,16,16,out); mipDown(out,8,8,out+256); mipDown(out+256,4,4,out+320); mipDown(out+320,2,2,out+336); }
