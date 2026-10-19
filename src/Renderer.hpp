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
};

// Common interface of the Vulkan and the OpenGL back end.
class Renderer: public ChunkSink{
protected: bool m_mips=true;
public:
 virtual ~Renderer()=default;
 virtual const char* name()const=0;
 virtual bool init(GLFWwindow*w,const AtlasImage&blockAtlas,const AtlasImage&uiAtlas,const PanoramaImage&pano)=0;
 virtual bool reloadTextures(const AtlasImage&blockAtlas,const AtlasImage&uiAtlas,const PanoramaImage&pano)=0;
 // Quad order in `ui`: [skyQuads: GUI atlas, drawn BEFORE the world][panoQuads: linear filtered panorama texture][rest: GUI atlas].
 virtual void draw(const FrameParams&fp,const UiVertex*ui,uint32_t skyQuads,uint32_t panoQuads,uint32_t totalQuads)=0;
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
};
