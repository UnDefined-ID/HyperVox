#pragma once
#include <GLFW/glfw3.h>
#include <cstdint>
#include <vector>
#include "Math.hpp"
#include "Png.hpp"
#include "Streamer.hpp"
#include "TexturePack.hpp"
#include "UiAtlas.hpp"
#include "UiTypes.hpp"

struct FrameParams{
 Mat4 vp;                 // projection * view, camera at the origin (everything is drawn camera-relative)
 Vec3 camera;             // world position of the camera
 int chunkX=0,chunkZ=0,renderDist=8;
 bool world=false;        // draw chunks
 bool outline=false; int ox=0,oy=0,oz=0;   // block selection outline
 float fogStart=0,fogEnd=0; float fogRGB[3]={0.f,0.f,0.f};   // distance fog (underwater / in lava); fogEnd<=0 = off
 int outBoxes=0; float ob[10][6]{};           // shape of the targeted block: boxes (x0,y0,z0,x1,y1,z1) in block fractions; 0 = full cube
};

// Common interface of the Vulkan and the OpenGL back end.
class Renderer: public ChunkSink{
protected: bool m_mips=true; int m_rd=16;
 // GPU chunk mesh memory is sized from the render distance (about 3500 vertices per chunk in range, 2M..12M vertices) instead of a fixed 100+ MB
 static uint32_t chunkUnitsFor(int rd){ const double ch=3.14159265*double(rd+1)*double(rd+1);
#ifdef HV_LOW_MEMORY
  const double u=ch*2200.0; return (uint32_t)(u<3.0e5?3.0e5:(u>2.5e6?2.5e6:u));   // console profile: ~2200 vertices per chunk in range, 0.3M..2.5M vertices (2.4..20 MB)
#else
  const double u=ch*16000.0; return (uint32_t)(u<6.0e6?6.0e6:(u>64.0e6?64.0e6:u));   // real terrain averages several thousand vertices per chunk; if it still runs full the buffer grows (growChunkMemory)
#endif
 }
public:
 virtual ~Renderer()=default;
 virtual const char* name()const=0;
 virtual bool init(GLFWwindow*w,const AtlasImage&blockAtlas,const AtlasImage&uiAtlas,const PanoramaImage&pano)=0;
 virtual bool reloadTextures(const AtlasImage&blockAtlas,const AtlasImage&uiAtlas,const PanoramaImage&pano)=0;
 // Quad order in `ui`: [skyQuads: GUI atlas, drawn BEFORE the world][panoQuads: linear filtered panorama texture][rest: GUI atlas].
 virtual void draw(const FrameParams&fp,const UiVertex*ui,uint32_t skyQuads,uint32_t panoQuads,uint32_t totalQuads)=0;
 virtual void setChunkMemory(int renderDist){m_rd=renderDist;}        // call before init()
 virtual bool resizeChunkMemory(int renderDist){(void)renderDist;return false;}   // true = buffers were re-created, every chunk has to be uploaded again
 virtual bool growChunkMemory(){return false;}                        // chunk buffer full: make it 1.5x bigger (up to 40M vertices); true = re-created, every chunk is uploaded again
 // entities: small dynamic meshes in the block vertex format.  (wx,wy,wz) = world position of the mesh origin (local coordinate 0), radius = culling radius around the entity.
 virtual bool uploadEntity(uint32_t id,float wx,float wy,float wz,float radius,const ChunkMesh&m){(void)id;(void)wx;(void)wy;(void)wz;(void)radius;(void)m;return false;}
 virtual void releaseEntity(uint32_t id){(void)id;}
 virtual void clearEntities(){}
 virtual size_t vramBytes()const{return 0;}                          // tracked GPU memory: chunk buffer + textures + render targets
 virtual void setMipmaps(bool on){m_mips=on;}      // call before init(); false = plain nearest sampling at every distance
 virtual void trim()=0;     // give back whatever GPU memory can be given back (retired buffers ...)
 virtual void updateTiles(const std::vector<TileUpdate>&t)=0;
 virtual void shutdown()=0;
 virtual void setVsync(bool on)=0;
 virtual Mat4 projection(float fovRad,float aspect,float zn,float zf)const=0;
 virtual size_t chunkCount()const=0;
 virtual uint32_t drawnChunks()const=0;
 virtual uint32_t vertexCapacity()const=0;
 virtual uint32_t vertexUsed()const=0;
 float fillRatio()const override{ const uint32_t c=vertexCapacity(); return c?float(vertexUsed())/float(c):0.f; }
};
