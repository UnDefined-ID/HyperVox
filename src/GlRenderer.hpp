#pragma once
#include <algorithm>
#include <iterator>
#include <map>
#include <unordered_map>
#include "GlFuncs.hpp"
#include "Renderer.hpp"

// Fast OpenGL 3.3 back end (same features as the Vulkan one: reverse-Z when hvglClipControl exists, translucent pass, animated tiles).
class GlRenderer: public Renderer{
public:
 ~GlRenderer()override{shutdown();}
 const char* name()const override{return "OpenGL";}
 bool init(GLFWwindow*w,const AtlasImage&blockAtlas,const AtlasImage&uiAtlas,const PanoramaImage&pano)override;
 bool reloadTextures(const AtlasImage&blockAtlas,const AtlasImage&uiAtlas,const PanoramaImage&pano)override;
 void draw(const FrameParams&fp,const UiVertex*ui,uint32_t skyQuads,uint32_t panoQuads,uint32_t totalQuads)override;
 void trim()override{}
 void updateTiles(const std::vector<TileUpdate>&t)override;
 void shutdown()override;
 void setVsync(bool on)override{vsync_=on;}
 void setMipmaps(bool)override{m_mips=false;}      // OpenGL does its own distance filtering in the shader (hardware mip maps broke on some drivers)
 Mat4 projection(float fov,float aspect,float zn,float zf)const override{ return clipControl_?perspectiveRev(fov,aspect,zn,zf,true):perspectiveGL(fov,aspect,std::max(zn,0.1f),zf); }
 bool upload(int cx,int cz,const ChunkMesh&m)override;
 void release(int cx,int cz)override;
 void clear()override;
 size_t chunkCount()const override{return list_.size();}
 uint32_t drawnChunks()const override{return lastDrawn_;}
 uint32_t vertexCapacity()const override{return vArena_.capacity;}
 uint32_t vertexUsed()const override{return vArena_.capacity-vArena_.freeUnits();}
private:
 struct Arena{
  uint32_t capacity=0; std::map<uint32_t,uint32_t> freeList;
  void init(uint32_t n){capacity=n;freeList.clear();freeList[0]=n;}
  uint32_t alloc(uint32_t n){ for(auto it=freeList.begin();it!=freeList.end();++it)if(it->second>=n){uint32_t off=it->first,rem=it->second-n;freeList.erase(it);if(rem)freeList[off+n]=rem;return off;} return UINT32_MAX; }
  void release(uint32_t off,uint32_t n){ auto it=freeList.lower_bound(off); if(it!=freeList.begin()){auto p=std::prev(it);if(p->first+p->second==off){off=p->first;n+=p->second;freeList.erase(p);}} if(it!=freeList.end()&&off+n==it->first){n+=it->second;it=freeList.erase(it);} freeList[off]=n; }
  uint32_t freeUnits()const{uint32_t t=0;for(auto&kv:freeList)t+=kv.second;return t;}
 };
 struct GpuChunk{int cx,cz;uint32_t vOff,vCnt,iOff,iCnt,tCnt;int maxY;};
 bool buildPrograms(); bool createTextures(const AtlasImage&b,const AtlasImage&u,const PanoramaImage&p); void destroyTextures();
 GLFWwindow* win_=nullptr; bool ok_=false,vsync_=false,lastVsync_=true,clipControl_=false;
 HVgluint progBlock_=0,progUi_=0,progLine_=0,texBlock_=0,texUi_=0,texPano_=0,vaoChunk_=0,vaoUi_=0,vaoLine_=0,vboChunk_=0,iboChunk_=0,vboUi_=0,iboUi_=0,vboLine_=0;
 HVglint uBlockVP_=-1,uBlockOrigin_=-1,uBlockAtlas_=-1,uUiVP_=-1,uUiTex_=-1,uLineVP_=-1;
 Arena vArena_,iArena_; std::vector<GpuChunk> list_; std::unordered_map<uint64_t,uint32_t> index_; uint32_t lastDrawn_=0;
 static constexpr uint32_t kMaxUiQuads=16384;
};
