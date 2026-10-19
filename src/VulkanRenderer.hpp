#pragma once
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <map>
#include <unordered_map>
#include <vector>
#include "Math.hpp"
#include "Png.hpp"
#include "Renderer.hpp"
#include "World.hpp"

class VulkanRenderer: public Renderer {
public:
 const char* name()const override{return "Vulkan";}
 bool init(GLFWwindow* window,const AtlasImage& blockAtlas,const AtlasImage& uiAtlas,const PanoramaImage& pano)override;
 bool reloadTextures(const AtlasImage& blockAtlas,const AtlasImage& uiAtlas,const PanoramaImage& pano)override;
 void draw(const FrameParams& fp,const UiVertex* ui,uint32_t skyQuads,uint32_t panoQuads,uint32_t totalQuads)override;
 void trim()override;
 void updateTiles(const std::vector<TileUpdate>& t)override{ for(auto&u:t){ bool f=false; for(auto&p:pendingTiles)if(p.id==u.id){p=u;f=true;break;} if(!f)pendingTiles.push_back(u);} }
 void shutdown()override;
 void setVsync(bool on)override{ if(on!=vsync){vsync=on;swapDirty=true;} }
 Mat4 projection(float fov,float aspect,float zn,float zf)const override{ return perspectiveRev(fov,aspect,zn,zf,true); }   // reverse-Z, Vulkan clip space
 bool upload(int cx,int cz,const ChunkMesh& m) override;
 void release(int cx,int cz) override;
 void clear() override;
 size_t chunkCount()const override{return chunkList.size();}
 uint32_t drawnChunks()const override{return lastDrawn;}
 uint32_t vertexCapacity()const override{return vtx.capacity;}
 uint32_t vertexUsed()const override{return vtx.capacity-vtx.freeUnits();}
private:
 struct Arena{
  VkBuffer buf=VK_NULL_HANDLE; VkDeviceMemory mem=VK_NULL_HANDLE; uint8_t* mapped=nullptr; uint32_t capacity=0; std::map<uint32_t,uint32_t> freeList;
  uint32_t alloc(uint32_t n){
   for(auto it=freeList.begin();it!=freeList.end();++it) if(it->second>=n){ uint32_t off=it->first,rem=it->second-n; freeList.erase(it); if(rem)freeList[off+n]=rem; return off; }
   return UINT32_MAX;
  }
  void release(uint32_t off,uint32_t n){
   auto it=freeList.lower_bound(off);
   if(it!=freeList.begin()){ auto p=std::prev(it); if(p->first+p->second==off){ off=p->first; n+=p->second; freeList.erase(p); } }
   if(it!=freeList.end()&&off+n==it->first){ n+=it->second; it=freeList.erase(it); }
   freeList[off]=n;
  }
  uint32_t freeUnits()const{ uint32_t t=0; for(auto&kv:freeList)t+=kv.second; return t; }
 };
 struct Tex{ VkImage img=VK_NULL_HANDLE; VkDeviceMemory mem=VK_NULL_HANDLE; VkImageView view=VK_NULL_HANDLE; VkSampler sampler=VK_NULL_HANDLE; };
 struct GpuChunk{ int cx,cz; uint32_t vOff,vCnt,iOff,iCnt,tCnt; int maxY; };   // indices: [iOff,iOff+iCnt) opaque, then tCnt translucent
 struct Retired{ uint32_t vOff,vCnt,iOff,iCnt; uint64_t frame; };   // iCnt = opaque + translucent
 static constexpr uint32_t kMaxUiQuads=16384, kMaxInstances=65536;

 GLFWwindow* m_window=nullptr; VkInstance instance=VK_NULL_HANDLE; VkSurfaceKHR surface=VK_NULL_HANDLE; VkPhysicalDevice gpu=VK_NULL_HANDLE; VkDevice device=VK_NULL_HANDLE; VkQueue graphicsQueue=VK_NULL_HANDLE,presentQueue=VK_NULL_HANDLE; uint32_t graphicsFamily=0,presentFamily=0;
 VkSwapchainKHR swapchain=VK_NULL_HANDLE; VkFormat swapFormat{}; VkExtent2D extent{}; std::vector<VkImage> swapImages; std::vector<VkImageView> swapViews; VkRenderPass renderPass=VK_NULL_HANDLE; VkImage depthImage=VK_NULL_HANDLE; VkDeviceMemory depthMemory=VK_NULL_HANDLE; VkImageView depthView=VK_NULL_HANDLE; VkFormat depthFormat=VK_FORMAT_D32_SFLOAT; VkPipelineLayout pipelineLayout=VK_NULL_HANDLE; std::vector<VkFramebuffer> framebuffers;
 std::vector<TileUpdate> pendingTiles; VkBuffer tileBuf[2]{}; VkDeviceMemory tileMem[2]{}; uint8_t* tileMap[2]{};   // animated tile uploads are recorded into the frame (no GPU stall)
 void flushTileUpdates(VkCommandBuffer c,int frame);
 VkPipeline pipeline=VK_NULL_HANDLE,transPipeline=VK_NULL_HANDLE,uiPipeline=VK_NULL_HANDLE,linePipeline=VK_NULL_HANDLE;
 VkCommandPool commandPool=VK_NULL_HANDLE; std::vector<VkCommandBuffer> cmd; std::vector<VkSemaphore> imageAvailable,renderFinished; std::vector<VkFence> inFlight; size_t frame=0;
 VkDescriptorSetLayout descLayout=VK_NULL_HANDLE; VkDescriptorPool descPool=VK_NULL_HANDLE; VkDescriptorSet descSet=VK_NULL_HANDLE,uiSet=VK_NULL_HANDLE,panoSet=VK_NULL_HANDLE;
 Tex blockTex,uiTex,panoTex;
 Arena vtx,idx; std::vector<GpuChunk> chunkList; std::unordered_map<uint64_t,uint32_t> chunkIndex; std::vector<Retired> retired; uint64_t frameCounter=0; uint32_t lastDrawn=0;
 VkBuffer uiVB[2]{VK_NULL_HANDLE,VK_NULL_HANDLE},uiIB=VK_NULL_HANDLE,instVB[2]{VK_NULL_HANDLE,VK_NULL_HANDLE},lineVB[2]{VK_NULL_HANDLE,VK_NULL_HANDLE};
 VkDeviceMemory uiVM[2]{VK_NULL_HANDLE,VK_NULL_HANDLE},uiIM=VK_NULL_HANDLE,instVM[2]{VK_NULL_HANDLE,VK_NULL_HANDLE},lineVM[2]{VK_NULL_HANDLE,VK_NULL_HANDLE};
 void *uiMapped[2]{nullptr,nullptr},*instMapped[2]{nullptr,nullptr},*lineMapped[2]{nullptr,nullptr};
 uint32_t uiQuadCount=0,uiPanoQuads=0,uiSkyQuads=0; bool vsync=false,swapDirty=false;

 bool uploadToDevice(const void*data,VkDeviceSize size,VkBufferUsageFlags usage,VkBuffer&b,VkDeviceMemory&m); VkCommandBuffer beginOnce(); void endOnce(VkCommandBuffer c);
 uint32_t findMemory(uint32_t typeBits,VkMemoryPropertyFlags props); bool createBuffer(VkDeviceSize size,VkBufferUsageFlags usage,VkMemoryPropertyFlags props,VkBuffer& b,VkDeviceMemory&m); void copyTo(VkDeviceMemory m,const void*data,size_t size);
 bool createInstance(); bool createSurface(); bool pickDevice(); bool createDevice(); bool createSwapchain(); bool createDepth(); bool createRenderPass(); bool createPipeline(); bool createFramebuffers(); bool createCommandPool(); bool createDescriptors(); bool createSync(); void recreate(); void destroySwapchain();
 bool createTexture(Tex&t,const uint8_t*rgba,uint32_t w,uint32_t h,bool linear,VkDescriptorSet set,bool mips=false); void destroyTexture(Tex&t); bool createTextures(const AtlasImage&b,const AtlasImage&u,const PanoramaImage&p);
 bool createArena(Arena&a,VkBufferUsageFlags usage,uint32_t units,uint32_t unitSize,uint32_t minUnits); void destroyArena(Arena&a);
 bool createHostBuffer(VkDeviceSize size,VkBufferUsageFlags usage,VkBuffer&b,VkDeviceMemory&m,void**mapped); bool createDynamicBuffers();
 void retire(const GpuChunk&c); void collectRetired(); void record(VkCommandBuffer c,uint32_t image,const FrameParams&fp);
};
