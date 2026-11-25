#include "VulkanRenderer.hpp"
#include "Embedded.hpp"
#include <cstring>
#include "Mips.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <stdexcept>
#include <string>

static constexpr uint32_t TexturePackCols=64;   // tiles per atlas row (TexturePack::kCols)
static void vkCheck(VkResult r,const char*what){if(r!=VK_SUCCESS)throw std::runtime_error(what);}
uint32_t VulkanRenderer::findMemory(uint32_t bits,VkMemoryPropertyFlags props){VkPhysicalDeviceMemoryProperties p;vkGetPhysicalDeviceMemoryProperties(gpu,&p);for(uint32_t i=0;i<p.memoryTypeCount;i++)if((bits&(1u<<i))&&(p.memoryTypes[i].propertyFlags&props)==props)return i;throw std::runtime_error("No suitable Vulkan memory type");}
bool VulkanRenderer::createBuffer(VkDeviceSize size,VkBufferUsageFlags usage,VkMemoryPropertyFlags props,VkBuffer&b,VkDeviceMemory&m){VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};bi.size=size;bi.usage=usage;bi.sharingMode=VK_SHARING_MODE_EXCLUSIVE;vkCheck(vkCreateBuffer(device,&bi,nullptr,&b),"buffer");VkMemoryRequirements mr;vkGetBufferMemoryRequirements(device,b,&mr);VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ai.allocationSize=mr.size;ai.memoryTypeIndex=findMemory(mr.memoryTypeBits,props);vkCheck(vkAllocateMemory(device,&ai,nullptr,&m),"buffer memory");vkBindBufferMemory(device,b,m,0);return true;}
void VulkanRenderer::copyTo(VkDeviceMemory m,const void*data,size_t size){void*p;vkMapMemory(device,m,0,size,0,&p);std::memcpy(p,data,size);vkUnmapMemory(device,m);}
VkCommandBuffer VulkanRenderer::beginOnce(){VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ca.commandPool=commandPool;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;VkCommandBuffer c=VK_NULL_HANDLE;vkCheck(vkAllocateCommandBuffers(device,&ca,&c),"once alloc");VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;vkBeginCommandBuffer(c,&bi);return c;}
void VulkanRenderer::endOnce(VkCommandBuffer c){vkEndCommandBuffer(c);VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};si.commandBufferCount=1;si.pCommandBuffers=&c;vkCheck(vkQueueSubmit(graphicsQueue,1,&si,VK_NULL_HANDLE),"once submit");vkQueueWaitIdle(graphicsQueue);vkFreeCommandBuffers(device,commandPool,1,&c);}
bool VulkanRenderer::uploadToDevice(const void*data,VkDeviceSize size,VkBufferUsageFlags usage,VkBuffer&b,VkDeviceMemory&m){VkBuffer sb=VK_NULL_HANDLE;VkDeviceMemory sm=VK_NULL_HANDLE;createBuffer(size,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,sb,sm);copyTo(sm,data,(size_t)size);createBuffer(size,usage|VK_BUFFER_USAGE_TRANSFER_DST_BIT,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,b,m);VkCommandBuffer c=beginOnce();VkBufferCopy bc{};bc.size=size;vkCmdCopyBuffer(c,sb,b,1,&bc);endOnce(c);vkDestroyBuffer(device,sb,nullptr);vkFreeMemory(device,sm,nullptr);return true;}
bool VulkanRenderer::createInstance(){VkApplicationInfo ai{VK_STRUCTURE_TYPE_APPLICATION_INFO};ai.pApplicationName="hypervox";ai.applicationVersion=1;ai.pEngineName="hypervox";ai.engineVersion=1;ai.apiVersion=VK_API_VERSION_1_2;uint32_t n=0;const char**e=glfwGetRequiredInstanceExtensions(&n);VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ci.pApplicationInfo=&ai;ci.enabledExtensionCount=n;ci.ppEnabledExtensionNames=e;return vkCreateInstance(&ci,nullptr,&instance)==VK_SUCCESS;}
bool VulkanRenderer::createSurface(){return glfwCreateWindowSurface(instance,m_window,nullptr,&surface)==VK_SUCCESS;}
bool VulkanRenderer::pickDevice(){uint32_t n=0;vkEnumeratePhysicalDevices(instance,&n,nullptr);if(!n)return false;std::vector<VkPhysicalDevice>d(n);vkEnumeratePhysicalDevices(instance,&n,d.data());VkPhysicalDevice best=VK_NULL_HANDLE;uint32_t bg=0,bp=0;int bestScore=-1;for(auto x:d){uint32_t qn=0;vkGetPhysicalDeviceQueueFamilyProperties(x,&qn,nullptr);std::vector<VkQueueFamilyProperties>q(qn);vkGetPhysicalDeviceQueueFamilyProperties(x,&qn,q.data());int g=-1,p=-1;for(uint32_t i=0;i<qn;i++){if(q[i].queueFlags&VK_QUEUE_GRAPHICS_BIT)g=i;VkBool32 sup=0;vkGetPhysicalDeviceSurfaceSupportKHR(x,i,surface,&sup);if(sup)p=i;}if(g>=0&&p>=0){VkPhysicalDeviceProperties pr;vkGetPhysicalDeviceProperties(x,&pr);int score=pr.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU?3:(pr.deviceType==VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU?2:1);if(score>bestScore){bestScore=score;best=x;bg=(uint32_t)g;bp=(uint32_t)p;}}}if(!best)return false;gpu=best;graphicsFamily=bg;presentFamily=bp;{VkPhysicalDeviceProperties pr;vkGetPhysicalDeviceProperties(gpu,&pr);std::cout<<"GPU: "<<pr.deviceName<<"\n";}return true;}
bool VulkanRenderer::createDevice(){float pr=1;std::set<uint32_t> fam={graphicsFamily,presentFamily};std::vector<VkDeviceQueueCreateInfo> qs;for(auto f:fam){VkDeviceQueueCreateInfo q{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};q.queueFamilyIndex=f;q.queueCount=1;q.pQueuePriorities=&pr;qs.push_back(q);}const char*ext="VK_KHR_swapchain";VkPhysicalDeviceFeatures feats{};feats.samplerAnisotropy=VK_FALSE;{VkPhysicalDeviceFeatures sup{};vkGetPhysicalDeviceFeatures(gpu,&sup);if(sup.multiDrawIndirect&&sup.drawIndirectFirstInstance){feats.multiDrawIndirect=VK_TRUE;feats.drawIndirectFirstInstance=VK_TRUE;mdi=true;}}VkDeviceCreateInfo ci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};ci.queueCreateInfoCount=(uint32_t)qs.size();ci.pQueueCreateInfos=qs.data();ci.enabledExtensionCount=1;ci.ppEnabledExtensionNames=&ext;ci.pEnabledFeatures=&feats;if(vkCreateDevice(gpu,&ci,nullptr,&device)!=VK_SUCCESS)return false;vkGetDeviceQueue(device,graphicsFamily,0,&graphicsQueue);vkGetDeviceQueue(device,presentFamily,0,&presentQueue);return true;}
bool VulkanRenderer::createSwapchain(){VkSurfaceCapabilitiesKHR cap;vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu,surface,&cap);uint32_t n=0;vkGetPhysicalDeviceSurfaceFormatsKHR(gpu,surface,&n,nullptr);std::vector<VkSurfaceFormatKHR>fs(n);vkGetPhysicalDeviceSurfaceFormatsKHR(gpu,surface,&n,fs.data());swapFormat=fs[0].format;VkColorSpaceKHR swapColorSpace=fs[0].colorSpace;for(auto&f:fs)if(f.format==VK_FORMAT_B8G8R8A8_SRGB&&f.colorSpace==VK_COLOR_SPACE_SRGB_NONLINEAR_KHR){swapFormat=f.format;swapColorSpace=f.colorSpace;break;}extent=cap.currentExtent;if(extent.width==UINT32_MAX){extent={1280,720};}uint32_t pn=0;vkGetPhysicalDeviceSurfacePresentModesKHR(gpu,surface,&pn,nullptr);std::vector<VkPresentModeKHR>pm(pn);vkGetPhysicalDeviceSurfacePresentModesKHR(gpu,surface,&pn,pm.data());VkPresentModeKHR mode=VK_PRESENT_MODE_FIFO_KHR;bool hasImm=false,hasMail=false;for(auto x:pm){if(x==VK_PRESENT_MODE_IMMEDIATE_KHR)hasImm=true;if(x==VK_PRESENT_MODE_MAILBOX_KHR)hasMail=true;}if(!vsync){if(hasImm)mode=VK_PRESENT_MODE_IMMEDIATE_KHR;else if(hasMail)mode=VK_PRESENT_MODE_MAILBOX_KHR;}std::cout<<"Present mode: "<<(mode==VK_PRESENT_MODE_IMMEDIATE_KHR?"IMMEDIATE (uncapped)":mode==VK_PRESENT_MODE_MAILBOX_KHR?"MAILBOX (uncapped)":"FIFO (vsync, capped to monitor refresh)")<<"\n";uint32_t count=std::max(cap.minImageCount+1,3u);if(cap.maxImageCount)count=std::min(count,cap.maxImageCount);VkSwapchainCreateInfoKHR ci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};ci.surface=surface;ci.minImageCount=count;ci.imageFormat=swapFormat;ci.imageColorSpace=swapColorSpace;ci.imageExtent=extent;ci.imageArrayLayers=1;ci.imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;uint32_t fam[]={graphicsFamily,presentFamily};if(graphicsFamily!=presentFamily){ci.imageSharingMode=VK_SHARING_MODE_CONCURRENT;ci.queueFamilyIndexCount=2;ci.pQueueFamilyIndices=fam;}else ci.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE;ci.preTransform=cap.currentTransform;ci.compositeAlpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;ci.presentMode=mode;ci.clipped=VK_TRUE;vkCheck(vkCreateSwapchainKHR(device,&ci,nullptr,&swapchain),"swapchain");vkGetSwapchainImagesKHR(device,swapchain,&n,nullptr);swapImages.resize(n);vkGetSwapchainImagesKHR(device,swapchain,&n,swapImages.data());swapViews.resize(n);for(size_t i=0;i<n;i++){VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};vi.image=swapImages[i];vi.viewType=VK_IMAGE_VIEW_TYPE_2D;vi.format=swapFormat;vi.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};vkCheck(vkCreateImageView(device,&vi,nullptr,&swapViews[i]),"view");}return true;}
bool VulkanRenderer::createDepth(){VkImageCreateInfo i{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};i.imageType=VK_IMAGE_TYPE_2D;i.extent={extent.width,extent.height,1};i.mipLevels=1;i.arrayLayers=1;i.format=depthFormat;i.tiling=VK_IMAGE_TILING_OPTIMAL;i.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;i.usage=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;i.samples=VK_SAMPLE_COUNT_1_BIT;i.sharingMode=VK_SHARING_MODE_EXCLUSIVE;vkCheck(vkCreateImage(device,&i,nullptr,&depthImage),"depth image");VkMemoryRequirements mr;vkGetImageMemoryRequirements(device,depthImage,&mr);VkMemoryAllocateInfo a{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};a.allocationSize=mr.size;a.memoryTypeIndex=findMemory(mr.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);vkCheck(vkAllocateMemory(device,&a,nullptr,&depthMemory),"depth memory");vkBindImageMemory(device,depthImage,depthMemory,0);VkImageViewCreateInfo v{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};v.image=depthImage;v.viewType=VK_IMAGE_VIEW_TYPE_2D;v.format=depthFormat;v.subresourceRange={VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,1};return vkCreateImageView(device,&v,nullptr,&depthView)==VK_SUCCESS;}
bool VulkanRenderer::createRenderPass(){VkAttachmentDescription a{};a.format=swapFormat;a.samples=VK_SAMPLE_COUNT_1_BIT;a.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;a.storeOp=VK_ATTACHMENT_STORE_OP_STORE;a.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;a.finalLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;VkAttachmentDescription d{};d.format=depthFormat;d.samples=VK_SAMPLE_COUNT_1_BIT;d.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;d.storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;d.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;d.finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;VkAttachmentDescription at[]={a,d};VkAttachmentReference ar{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};VkAttachmentReference dr{1,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};VkSubpassDescription s{};s.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;s.colorAttachmentCount=1;s.pColorAttachments=&ar;s.pDepthStencilAttachment=&dr;VkSubpassDependency dep{};dep.srcSubpass=VK_SUBPASS_EXTERNAL;dep.dstSubpass=0;dep.srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;dep.dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;dep.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;VkRenderPassCreateInfo ci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};ci.attachmentCount=2;ci.pAttachments=at;ci.subpassCount=1;ci.pSubpasses=&s;ci.dependencyCount=1;ci.pDependencies=&dep;return vkCreateRenderPass(device,&ci,nullptr,&renderPass)==VK_SUCCESS;}
struct PushData{ Mat4 vp; float fogColor[4]; float fogParams[4]; };   // 96 bytes: matrix + fog colour + (start, end, on)
static std::vector<char> loadSpv(const char*p){std::ifstream f(p,std::ios::binary|std::ios::ate);if(!f){ for(const char*pre:{"../","build/"}){ f.clear(); f.open(std::string(pre)+p,std::ios::binary|std::ios::ate); if(f)break; } } if(!f){ const char*b=std::strrchr(p,'/'); const EmbeddedBlob e=embeddedBlob(b?b+1:p); if(e.p)return std::vector<char>((const char*)e.p,(const char*)e.p+e.n); return{}; } size_t n=static_cast<size_t>(static_cast<std::streamoff>(f.tellg()));f.seekg(0);std::vector<char>b(n);f.read(b.data(),static_cast<std::streamsize>(n));return b;}
bool VulkanRenderer::createFramebuffers(){framebuffers.resize(swapViews.size());for(size_t i=0;i<swapViews.size();i++){VkFramebufferCreateInfo f{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};f.renderPass=renderPass;VkImageView at[]={swapViews[i],depthView};f.attachmentCount=2;f.pAttachments=at;f.width=extent.width;f.height=extent.height;f.layers=1;vkCheck(vkCreateFramebuffer(device,&f,nullptr,&framebuffers[i]),"fb");}return true;}
bool VulkanRenderer::createCommandPool(){VkCommandPoolCreateInfo p{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};p.queueFamilyIndex=graphicsFamily;p.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;vkCheck(vkCreateCommandPool(device,&p,nullptr,&commandPool),"pool");cmd.resize(framebuffers.size());VkCommandBufferAllocateInfo a{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};a.commandPool=commandPool;a.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;a.commandBufferCount=(uint32_t)cmd.size();return vkAllocateCommandBuffers(device,&a,cmd.data())==VK_SUCCESS;}
bool VulkanRenderer::createSync(){imageAvailable.resize(2);renderFinished.resize(2);inFlight.resize(2);VkSemaphoreCreateInfo s{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};VkFenceCreateInfo f{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};f.flags=VK_FENCE_CREATE_SIGNALED_BIT;for(int i=0;i<2;i++){vkCreateSemaphore(device,&s,nullptr,&imageAvailable[i]);vkCreateSemaphore(device,&s,nullptr,&renderFinished[i]);vkCreateFence(device,&f,nullptr,&inFlight[i]);}return true;}
void VulkanRenderer::destroySwapchain(){for(auto f:framebuffers)vkDestroyFramebuffer(device,f,nullptr);for(auto v:swapViews)vkDestroyImageView(device,v,nullptr);if(swapchain)vkDestroySwapchainKHR(device,swapchain,nullptr);framebuffers.clear();swapViews.clear();swapchain=VK_NULL_HANDLE;}
void VulkanRenderer::recreate(){int w=0,h=0;glfwGetFramebufferSize(m_window,&w,&h);while(w==0||h==0){glfwGetFramebufferSize(m_window,&w,&h);glfwWaitEvents();}vkDeviceWaitIdle(device);destroySwapchain();if(depthView)vkDestroyImageView(device,depthView,nullptr);if(depthImage)vkDestroyImage(device,depthImage,nullptr);if(depthMemory)vkFreeMemory(device,depthMemory,nullptr);depthView=VK_NULL_HANDLE;depthImage=VK_NULL_HANDLE;depthMemory=VK_NULL_HANDLE;createSwapchain();createDepth();createFramebuffers();}

// ---------------------------------------------------------------------------------------------------------------------
// descriptors / pipelines
// ---------------------------------------------------------------------------------------------------------------------
bool VulkanRenderer::createDescriptors(){
 VkDescriptorSetLayoutBinding b{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT};
 VkDescriptorSetLayoutCreateInfo l{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};l.bindingCount=1;l.pBindings=&b;
 vkCheck(vkCreateDescriptorSetLayout(device,&l,nullptr,&descLayout),"desc layout");
 VkDescriptorPoolSize ps{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,3};
 VkDescriptorPoolCreateInfo p{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};p.maxSets=3;p.poolSizeCount=1;p.pPoolSizes=&ps;
 vkCheck(vkCreateDescriptorPool(device,&p,nullptr,&descPool),"desc pool");
 VkDescriptorSetLayout layouts[3]={descLayout,descLayout,descLayout}; VkDescriptorSet sets[3]{};
 VkDescriptorSetAllocateInfo a{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};a.descriptorPool=descPool;a.descriptorSetCount=3;a.pSetLayouts=layouts;
 vkCheck(vkAllocateDescriptorSets(device,&a,sets),"desc sets");
 descSet=sets[0];uiSet=sets[1];panoSet=sets[2];
 return true;
}

bool VulkanRenderer::createPipeline(){
 auto vsB=loadSpv("shaders/block.vert.spv"),fsB=loadSpv("shaders/block.frag.spv"),vsU=loadSpv("shaders/ui.vert.spv"),fsU=loadSpv("shaders/ui.frag.spv"),vsL=loadSpv("shaders/line.vert.spv"),fsL=loadSpv("shaders/line.frag.spv");
 if(vsB.empty()||fsB.empty()||vsU.empty()||fsU.empty()||vsL.empty()||fsL.empty()){std::cerr<<"Compiled shaders (*.spv) not found next to the executable.\n";return false;}
 auto mod=[&](const std::vector<char>&code)->VkShaderModule{
  VkShaderModuleCreateInfo sm{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};sm.codeSize=code.size();sm.pCode=(const uint32_t*)code.data();
  VkShaderModule m=VK_NULL_HANDLE; if(vkCreateShaderModule(device,&sm,nullptr,&m)!=VK_SUCCESS)return VK_NULL_HANDLE; return m;
 };
 VkShaderModule vmB=mod(vsB),fmB=mod(fsB),vmU=mod(vsU),fmU=mod(fsU),vmL=mod(vsL),fmL=mod(fsL);
 if(!vmB||!fmB||!vmU||!fmU||!vmL||!fmL)return false;

 VkVertexInputBindingDescription bindB[2]={{0,sizeof(Vertex),VK_VERTEX_INPUT_RATE_VERTEX},{1,sizeof(float)*4,VK_VERTEX_INPUT_RATE_INSTANCE}};
 VkVertexInputAttributeDescription attrB[3]={{0,0,VK_FORMAT_R32_UINT,0},{1,0,VK_FORMAT_R32_UINT,4},{2,1,VK_FORMAT_R32G32B32A32_SFLOAT,0}};
 VkPipelineVertexInputStateCreateInfo viB{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};viB.vertexBindingDescriptionCount=2;viB.pVertexBindingDescriptions=bindB;viB.vertexAttributeDescriptionCount=3;viB.pVertexAttributeDescriptions=attrB;
 VkVertexInputBindingDescription bindU{0,sizeof(UiVertex),VK_VERTEX_INPUT_RATE_VERTEX};
 VkVertexInputAttributeDescription attrU[3]={{0,0,VK_FORMAT_R32G32_SFLOAT,offsetof(UiVertex,x)},{1,0,VK_FORMAT_R32G32_SFLOAT,offsetof(UiVertex,u)},{2,0,VK_FORMAT_R8G8B8A8_UNORM,offsetof(UiVertex,rgba)}};
 VkPipelineVertexInputStateCreateInfo viU{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};viU.vertexBindingDescriptionCount=1;viU.pVertexBindingDescriptions=&bindU;viU.vertexAttributeDescriptionCount=3;viU.pVertexAttributeDescriptions=attrU;
 VkVertexInputBindingDescription bindL{0,sizeof(float)*3,VK_VERTEX_INPUT_RATE_VERTEX};
 VkVertexInputAttributeDescription attrL[1]={{0,0,VK_FORMAT_R32G32B32_SFLOAT,0}};
 VkPipelineVertexInputStateCreateInfo viL{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};viL.vertexBindingDescriptionCount=1;viL.pVertexBindingDescriptions=&bindL;viL.vertexAttributeDescriptionCount=1;viL.pVertexAttributeDescriptions=attrL;

 VkViewport vp{0,0,(float)extent.width,(float)extent.height,0,1};VkRect2D sc{{0,0},extent};
 VkPipelineViewportStateCreateInfo vsx{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};vsx.viewportCount=1;vsx.pViewports=&vp;vsx.scissorCount=1;vsx.pScissors=&sc;
 VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};rs.polygonMode=VK_POLYGON_MODE_FILL;rs.cullMode=VK_CULL_MODE_BACK_BIT;rs.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE;rs.lineWidth=1;
 VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
 VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};ds.depthTestEnable=VK_TRUE;ds.depthWriteEnable=VK_TRUE;ds.depthCompareOp=VK_COMPARE_OP_GREATER;   // reverse-Z
 VkPipelineColorBlendAttachmentState cb{};cb.colorWriteMask=0xF;
 VkPipelineColorBlendStateCreateInfo cbs{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};cbs.attachmentCount=1;cbs.pAttachments=&cb;
 VkDynamicState dyn[]={VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};
 VkPipelineDynamicStateCreateInfo dsi{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};dsi.dynamicStateCount=2;dsi.pDynamicStates=dyn;
 VkPushConstantRange pc{VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(PushData)};
 VkPipelineLayoutCreateInfo pli{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};pli.setLayoutCount=1;pli.pSetLayouts=&descLayout;pli.pushConstantRangeCount=1;pli.pPushConstantRanges=&pc;
 vkCheck(vkCreatePipelineLayout(device,&pli,nullptr,&pipelineLayout),"layout");

 auto build=[&](VkShaderModule vs,VkShaderModule fs,const VkPipelineVertexInputStateCreateInfo&vi,VkPrimitiveTopology topo,bool ui,VkPipeline&out,bool trans=false)->bool{
  VkPipelineShaderStageCreateInfo st[2]{};
  st[0].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;st[0].stage=VK_SHADER_STAGE_VERTEX_BIT;st[0].module=vs;st[0].pName="main";
  st[1].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;st[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;st[1].module=fs;st[1].pName="main";
  VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};ia.topology=topo;
  VkPipelineRasterizationStateCreateInfo r2=rs;VkPipelineDepthStencilStateCreateInfo d2=ds;VkPipelineColorBlendAttachmentState c2=cb;VkPipelineColorBlendStateCreateInfo cb2=cbs;
  if(ui){
   r2.cullMode=VK_CULL_MODE_NONE;d2.depthTestEnable=VK_FALSE;d2.depthWriteEnable=VK_FALSE;
   c2.blendEnable=VK_TRUE;c2.srcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;c2.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;c2.colorBlendOp=VK_BLEND_OP_ADD;
   c2.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE;c2.dstAlphaBlendFactor=VK_BLEND_FACTOR_ZERO;c2.alphaBlendOp=VK_BLEND_OP_ADD;
  }
  if(trans){   // water / stained glass / tinted glass: alpha blended, depth tested but not written
   d2.depthWriteEnable=VK_FALSE;
   c2.blendEnable=VK_TRUE;c2.srcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;c2.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;c2.colorBlendOp=VK_BLEND_OP_ADD;
   c2.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE;c2.dstAlphaBlendFactor=VK_BLEND_FACTOR_ZERO;c2.alphaBlendOp=VK_BLEND_OP_ADD;
  }
  if(topo==VK_PRIMITIVE_TOPOLOGY_LINE_LIST)r2.cullMode=VK_CULL_MODE_NONE;
  cb2.pAttachments=&c2;
  VkGraphicsPipelineCreateInfo pi{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
  pi.stageCount=2;pi.pStages=st;pi.pVertexInputState=&vi;pi.pInputAssemblyState=&ia;pi.pViewportState=&vsx;pi.pRasterizationState=&r2;pi.pMultisampleState=&ms;pi.pDepthStencilState=&d2;pi.pColorBlendState=&cb2;pi.pDynamicState=&dsi;
  pi.layout=pipelineLayout;pi.renderPass=renderPass;pi.subpass=0;
  return vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&pi,nullptr,&out)==VK_SUCCESS;
 };
 bool ok=build(vmB,fmB,viB,VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,false,pipeline)&&build(vmB,fmB,viB,VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,false,transPipeline,true)&&build(vmU,fmU,viU,VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,true,uiPipeline)&&build(vmL,fmL,viL,VK_PRIMITIVE_TOPOLOGY_LINE_LIST,false,linePipeline);
 for(VkShaderModule m:{vmB,fmB,vmU,fmU,vmL,fmL})vkDestroyShaderModule(device,m,nullptr);
 return ok;
}

// ---------------------------------------------------------------------------------------------------------------------
// textures: ONE image per atlas, uploaded once (and again only when the resource pack changes)
// ---------------------------------------------------------------------------------------------------------------------
bool VulkanRenderer::createTexture(Tex&t,const uint8_t*rgba,uint32_t W,uint32_t Hh,bool linear,VkDescriptorSet set,bool mips){
 const uint32_t lv=(mips&&m_mips)?5u:1u; t.levels=lv; std::vector<size_t> mipOff; const std::vector<uint8_t> all=buildMipChain(rgba,W,Hh,(int)lv,mipOff); const VkDeviceSize bytes=(VkDeviceSize)all.size();
 VkBuffer sb=VK_NULL_HANDLE;VkDeviceMemory sm=VK_NULL_HANDLE;
 createBuffer(bytes,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,sb,sm);copyTo(sm,all.data(),all.size());
 VkImageCreateInfo i{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};i.imageType=VK_IMAGE_TYPE_2D;i.extent={W,Hh,1};i.mipLevels=lv;i.arrayLayers=1;i.format=VK_FORMAT_R8G8B8A8_SRGB;i.tiling=VK_IMAGE_TILING_OPTIMAL;i.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;i.usage=VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;i.samples=VK_SAMPLE_COUNT_1_BIT;i.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
 vkCheck(vkCreateImage(device,&i,nullptr,&t.img),"texture image");
 VkMemoryRequirements mr;vkGetImageMemoryRequirements(device,t.img,&mr);
 VkMemoryAllocateInfo a{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};a.allocationSize=mr.size;a.memoryTypeIndex=findMemory(mr.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
 t.bytes=mr.size; vkCheck(vkAllocateMemory(device,&a,nullptr,&t.mem),"texture memory");vkBindImageMemory(device,t.img,t.mem,0);
 VkCommandBuffer c=beginOnce();
 VkImageMemoryBarrier b1{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b1.srcAccessMask=0;b1.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;b1.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;b1.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;b1.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b1.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b1.image=t.img;b1.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,lv,0,1};
 vkCmdPipelineBarrier(c,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&b1);
 VkBufferImageCopy bic{};bic.imageSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;bic.imageSubresource.mipLevel=0;bic.imageSubresource.baseArrayLayer=0;bic.imageSubresource.layerCount=1;bic.imageExtent.width=W;bic.imageExtent.height=Hh;bic.imageExtent.depth=1;
 { std::vector<VkBufferImageCopy> rg(lv); for(uint32_t l=0;l<lv;l++){ VkBufferImageCopy r=bic; r.bufferOffset=mipOff[l]; r.imageSubresource.mipLevel=l; r.imageExtent={std::max(1u,W>>l),std::max(1u,Hh>>l),1}; rg[l]=r; } vkCmdCopyBufferToImage(c,sb,t.img,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,lv,rg.data()); }
 VkImageMemoryBarrier b2=b1;b2.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;b2.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;b2.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;b2.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
 vkCmdPipelineBarrier(c,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,nullptr,0,nullptr,1,&b2);
 endOnce(c);vkDestroyBuffer(device,sb,nullptr);vkFreeMemory(device,sm,nullptr);
 VkImageViewCreateInfo v{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};v.image=t.img;v.viewType=VK_IMAGE_VIEW_TYPE_2D;v.format=i.format;v.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,lv,0,1};vkCheck(vkCreateImageView(device,&v,nullptr,&t.view),"texture view");
 VkSamplerCreateInfo s{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};s.magFilter=linear?VK_FILTER_LINEAR:VK_FILTER_NEAREST;s.minFilter=s.magFilter;s.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;s.maxLod=float(lv-1);s.addressModeU=s.addressModeV=s.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;vkCheck(vkCreateSampler(device,&s,nullptr,&t.sampler),"sampler");
 VkDescriptorImageInfo di{t.sampler,t.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};w.dstSet=set;w.dstBinding=0;w.descriptorCount=1;w.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;w.pImageInfo=&di;vkUpdateDescriptorSets(device,1,&w,0,nullptr);
 return true;
}
void VulkanRenderer::destroyTexture(Tex&t){
 if(t.sampler)vkDestroySampler(device,t.sampler,nullptr);if(t.view)vkDestroyImageView(device,t.view,nullptr);if(t.img)vkDestroyImage(device,t.img,nullptr);if(t.mem)vkFreeMemory(device,t.mem,nullptr);
 t=Tex{};
}
bool VulkanRenderer::createTextures(const AtlasImage&b,const AtlasImage&u,const PanoramaImage&p){
 if(!createTexture(blockTex,b.rgba.data(),b.width,b.height,false,descSet,true))return false;
 for(int i=0;i<2&&!tileBuf[i];i++){ const VkDeviceSize sz=(VkDeviceSize)256*1364; createBuffer(sz,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,tileBuf[i],tileMem[i]); void*p=nullptr; vkMapMemory(device,tileMem[i],0,sz,0,&p); tileMap[i]=(uint8_t*)p; }
 if(!createTexture(uiTex,u.rgba.data(),u.width,u.height,false,uiSet))return false;
 if(p.valid()){ if(!createTexture(panoTex,p.rgba.data(),p.size,p.size*6,true,panoSet))return false; }
 else { const uint8_t black[16]={0,0,0,255,0,0,0,255,0,0,0,255,0,0,0,255}; if(!createTexture(panoTex,black,2,2,true,panoSet))return false; }
 return true;
}
bool VulkanRenderer::reloadTextures(const AtlasImage&b,const AtlasImage&u,const PanoramaImage&p){
 try{
  vkDeviceWaitIdle(device); destroyTexture(blockTex);destroyTexture(uiTex);destroyTexture(panoTex);
  return createTextures(b,u,p);
 }catch(const std::exception&e){std::cerr<<e.what()<<"\n";return false;}
}

// ---------------------------------------------------------------------------------------------------------------------
// chunk storage: two big persistently mapped arenas (vertices + 16-bit indices) with a free list
// ---------------------------------------------------------------------------------------------------------------------
bool VulkanRenderer::writeVertices(uint32_t vo,const Vertex*vp,uint32_t n){
 const size_t bytes=(size_t)n*sizeof(Vertex);
 if(!vtx.staged){ std::memcpy(vtx.mapped+(size_t)vo*sizeof(Vertex),vp,bytes); return true; }
 const int f=(int)frame;
 if(stageUsed[f]){ vkWaitForFences(device,1,&inFlight[f],VK_TRUE,UINT64_MAX); stageHead[f]=0; stageUsed[f]=false; }   // the GPU has finished the copies recorded last time this slot was used
 if(stageHead[f]+bytes>kStageBytes){ stageBusy=true; return false; }
 std::memcpy(stageMap[f]+stageHead[f],vp,bytes);
 VkBufferCopy cp{};cp.srcOffset=stageHead[f];cp.dstOffset=(VkDeviceSize)vo*sizeof(Vertex);cp.size=bytes; stageCopies[f].push_back(cp);
 stageHead[f]+=(bytes+15)&~(size_t)15; return true;
}
uint32_t VulkanRenderer::chunkUnitsCapped(int rd){
 VkPhysicalDeviceMemoryProperties mp;vkGetPhysicalDeviceMemoryProperties(gpu,&mp); VkDeviceSize vram=0; for(uint32_t i=0;i<mp.memoryHeapCount;i++)if(mp.memoryHeaps[i].flags&VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)vram=std::max(vram,mp.memoryHeaps[i].size);
 uint32_t units=chunkUnitsFor(rd); if(vram>0){ const uint64_t cap=(uint64_t)((double)vram*0.40/sizeof(Vertex)); if(units>cap)units=(uint32_t)cap; }
 return std::max<uint32_t>(units,1u<<20);
}
bool VulkanRenderer::createArena(Arena&a,VkBufferUsageFlags usage,uint32_t units,uint32_t unitSize,uint32_t minUnits){
 VkPhysicalDeviceMemoryProperties mp;vkGetPhysicalDeviceMemoryProperties(gpu,&mp);
 const VkMemoryPropertyFlags HV=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,HC=VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,DL=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
 // 0 = plain VRAM (filled through staging buffers: the GPU reads it at full speed), 1 = VRAM visible to the CPU (a small BAR window), 2 = system memory (the GPU then reads every vertex over PCIe: slow)
 const VkMemoryPropertyFlags need[3]={DL,DL|HV|HC,HV|HC};
 const uint32_t first=units;
 for(int phase=0;phase<2;phase++){
  for(units=first;units>=minUnits;units/=2){
   VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};bi.size=(VkDeviceSize)units*unitSize;bi.usage=usage;bi.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
   if(vkCreateBuffer(device,&bi,nullptr,&a.buf)!=VK_SUCCESS){a.buf=VK_NULL_HANDLE;continue;}
   VkMemoryRequirements mr;vkGetBufferMemoryRequirements(device,a.buf,&mr); int usedPass=-1;
   for(int pass=(phase==0?0:2);pass<=(phase==0?1:2)&&!a.mem;pass++)for(uint32_t t=0;t<mp.memoryTypeCount;t++){
    if(!(mr.memoryTypeBits&(1u<<t)))continue; const VkMemoryPropertyFlags fl=mp.memoryTypes[t].propertyFlags; if((fl&need[pass])!=need[pass])continue; if(pass==0&&(fl&HV))continue;
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ai.allocationSize=mr.size;ai.memoryTypeIndex=t;
    if(vkAllocateMemory(device,&ai,nullptr,&a.mem)==VK_SUCCESS){usedPass=pass;break;} a.mem=VK_NULL_HANDLE;
   }
   if(!a.mem){vkDestroyBuffer(device,a.buf,nullptr);a.buf=VK_NULL_HANDLE;continue;}
   vkBindBufferMemory(device,a.buf,a.mem,0);
   if(usedPass==0){ a.mapped=nullptr; a.staged=true; }
   else { void*p=nullptr; if(vkMapMemory(device,a.mem,0,VK_WHOLE_SIZE,0,&p)!=VK_SUCCESS){vkFreeMemory(device,a.mem,nullptr);vkDestroyBuffer(device,a.buf,nullptr);a.mem=VK_NULL_HANDLE;a.buf=VK_NULL_HANDLE;continue;} a.mapped=(uint8_t*)p; a.staged=false; }
   a.capacity=units;a.freeList.clear();a.freeList[0]=units; std::cout<<"Chunk buffer: "<<(units/1024*unitSize/1024)<<" MB in "<<(usedPass==0?"VRAM":usedPass==1?"VRAM (CPU visible window)":"SYSTEM MEMORY (slow, the GPU reads it over PCIe)")<<"\n"; return true;
  }
 }
 return false;
}
void VulkanRenderer::destroyArena(Arena&a){
 if(a.mem&&a.mapped){vkUnmapMemory(device,a.mem);}if(a.buf)vkDestroyBuffer(device,a.buf,nullptr);if(a.mem)vkFreeMemory(device,a.mem,nullptr);a=Arena{};
}
bool VulkanRenderer::createHostBuffer(VkDeviceSize size,VkBufferUsageFlags usage,VkBuffer&b,VkDeviceMemory&m,void**mapped){
 createBuffer(size,usage,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,b,m);
 return vkMapMemory(device,m,0,VK_WHOLE_SIZE,0,mapped)==VK_SUCCESS;
}
bool VulkanRenderer::createDynamicBuffers(){
 for(int i=0;i<2;i++){
  if(!createHostBuffer((VkDeviceSize)kMaxUiQuads*4*sizeof(UiVertex),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,uiVB[i],uiVM[i],&uiMapped[i]))return false;
  if(!createHostBuffer((VkDeviceSize)kMaxInstances*4*sizeof(float),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,instVB[i],instVM[i],&instMapped[i]))return false;
  if(!createHostBuffer(4096,VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,lineVB[i],lineVM[i],&lineMapped[i]))return false;
 }
 std::vector<uint32_t> ix;ix.reserve((size_t)kMaxUiQuads*6);
 for(uint32_t q=0;q<kMaxUiQuads;q++){uint32_t b=q*4;ix.insert(ix.end(),{b,b+1,b+2,b,b+2,b+3});}
 return uploadToDevice(ix.data(),ix.size()*sizeof(uint32_t),VK_BUFFER_USAGE_INDEX_BUFFER_BIT,uiIB,uiIM);
}

static inline uint64_t chunkKey(int cx,int cz){return (static_cast<uint64_t>(static_cast<uint32_t>(cx))<<32)|static_cast<uint32_t>(cz);}
void VulkanRenderer::retire(const GpuChunk&c){retired.push_back(Retired{c.vOff,c.vCnt,frameCounter});}
void VulkanRenderer::collectRetired(){
 size_t w=0;
 for(size_t i=0;i<retired.size();i++){
  const Retired&r=retired[i];
  if(frameCounter>=r.frame+3){vtx.release(r.vOff,r.vCnt);} else retired[w++]=r;
 }
 retired.resize(w);
}
bool VulkanRenderer::upload(int cx,int cz,const ChunkMesh&m){
 stageBusy=false;
 const uint64_t k=chunkKey(cx,cz); auto it=chunkIndex.find(k);
 if(m.vertices.empty()||m.vertices.size()>(1u<<24)){ release(cx,cz); return true; }     // empty (or absurdly dense) chunk: nothing to draw
 const uint32_t vc=(uint32_t)m.vertices.size();
 // New chunks may only fill the arena to 92%: the rest is reserved so an edited chunk (break / place) can always be re-uploaded.
 if(it==chunkIndex.end()&&uint64_t(vtx.freeUnits())*100<uint64_t(vtx.capacity)*8)return false;
 const uint32_t vo=vtx.alloc(vc); if(vo==UINT32_MAX)return false;
 if(!writeVertices(vo,m.vertices.data(),vc)){ vtx.release(vo,vc); return false; }
 GpuChunk g{cx,cz,vo,vc,{},m.maxY}; for(int i=0;i<kBuckets;i++)g.end[i]=m.bucketEnd[i]; g.ox=m.ox;g.oz=m.oz;g.scale=m.scale;g.size=m.size;
 if(it!=chunkIndex.end()){ retire(chunkList[it->second]); chunkList[it->second]=g; }
 else { chunkIndex[k]=(uint32_t)chunkList.size(); chunkList.push_back(g); }
 return true;
}
void VulkanRenderer::release(int cx,int cz){
 auto it=chunkIndex.find(chunkKey(cx,cz)); if(it==chunkIndex.end())return;
 const uint32_t i=it->second; retire(chunkList[i]); chunkIndex.erase(it);
 if(i!=chunkList.size()-1){ chunkList[i]=chunkList.back(); chunkIndex[chunkKey(chunkList[i].cx,chunkList[i].cz)]=i; }
 chunkList.pop_back();
}
bool VulkanRenderer::uploadEntity(uint32_t id,float wx,float wy,float wz,float radius,const ChunkMesh&m){
 if(m.vertices.empty()||m.vertices.size()>65535){ releaseEntity(id); return true; }
 const uint32_t vc=(uint32_t)m.vertices.size(); const uint32_t vo=vtx.alloc(vc); if(vo==UINT32_MAX)return false;
 if(!writeVertices(vo,m.vertices.data(),vc)){ vtx.release(vo,vc); return false; }
 auto it=ents.find(id); if(it!=ents.end()) retired.push_back(Retired{it->second.vOff,it->second.vCnt,frameCounter});   // the GPU may still read last frame's copy
 EntDraw d; d.vOff=vo;d.vCnt=vc;d.x=wx;d.y=wy;d.z=wz;d.r=radius; ents[id]=d; return true;
}
void VulkanRenderer::releaseEntity(uint32_t id){ auto it=ents.find(id); if(it==ents.end())return; retired.push_back(Retired{it->second.vOff,it->second.vCnt,frameCounter}); ents.erase(it); }
void VulkanRenderer::clearEntities(){ for(auto&kv:ents)retired.push_back(Retired{kv.second.vOff,kv.second.vCnt,frameCounter}); ents.clear(); }
bool VulkanRenderer::recreateChunkBuffer(uint32_t want){
 vkDeviceWaitIdle(device); chunkList.clear(); ents.clear(); chunkIndex.clear(); retired.clear(); destroyArena(vtx); for(int i=0;i<2;i++){ stageCopies[i].clear(); stageHead[i]=0; stageUsed[i]=false; }
 if(!createArena(vtx,VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,want,sizeof(Vertex),512u*1024)){ std::cerr<<"Could not re-create the chunk buffer\n"; return false; }
 std::cout<<"Chunk memory: "<<(vtx.capacity/1024*sizeof(Vertex)/1024)<<" MB vertices\n"; return true;
}
bool VulkanRenderer::resizeChunkMemory(int rdNew){
 m_rd=rdNew; if(chunkUnitsCapped(rdNew)<=vtx.capacity)return false;
 return recreateChunkBuffer(chunkUnitsCapped(rdNew+8));   // grow with headroom so dragging the slider does not re-allocate every step
}
bool VulkanRenderer::growChunkMemory(){
 const uint32_t cap=40u*1024*1024; if(vtx.capacity>=cap)return false; const uint32_t want=std::min<uint32_t>(vtx.capacity+vtx.capacity/2,cap);
 return recreateChunkBuffer(want);
}
void VulkanRenderer::clear(){ for(auto&c:chunkList)retire(c); chunkList.clear(); chunkIndex.clear(); }

// ---------------------------------------------------------------------------------------------------------------------
// frame
// ---------------------------------------------------------------------------------------------------------------------
static bool aabbVisible(const Mat4&m,float x0,float y0,float z0,float x1,float y1,float z1){
 int l=0,r=0,b=0,t=0,n=0,f=0;
 for(int i=0;i<8;i++){
  const float x=(i&1)?x1:x0,y=(i&2)?y1:y0,z=(i&4)?z1:z0;
  const float cx=m.m[0]*x+m.m[4]*y+m.m[8]*z+m.m[12],cy=m.m[1]*x+m.m[5]*y+m.m[9]*z+m.m[13],cz=m.m[2]*x+m.m[6]*y+m.m[10]*z+m.m[14],cw=m.m[3]*x+m.m[7]*y+m.m[11]*z+m.m[15];
  if(cx<-cw)l++; if(cx>cw)r++; if(cy<-cw)b++; if(cy>cw)t++; if(cz<0.f)n++; if(cz>cw)f++;
 }
 return !(l==8||r==8||b==8||t==8||n==8||f==8);
}

void VulkanRenderer::record(VkCommandBuffer c,uint32_t image,const FrameParams&fp){
 VkCommandBufferBeginInfo b{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};vkBeginCommandBuffer(c,&b);
 VkClearValue cv[2]{};cv[0].color.float32[0]=0.45f;cv[0].color.float32[1]=0.65f;cv[0].color.float32[2]=0.95f;cv[0].color.float32[3]=1.0f;cv[1].depthStencil.depth=0.0f;cv[1].depthStencil.stencil=0;
 VkRenderPassBeginInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};rp.renderPass=renderPass;rp.framebuffer=framebuffers[image];rp.renderArea.offset.x=0;rp.renderArea.offset.y=0;rp.renderArea.extent=extent;rp.clearValueCount=2;rp.pClearValues=cv;
 flushTileUpdates(c,(int)frame);   // animated textures: recorded into this frame, no GPU stall
 if(vtx.staged&&!stageCopies[frame].empty()){   // chunk / entity vertices staged since the last frame: copied into VRAM here
  vkCmdCopyBuffer(c,stageBuf[frame],vtx.buf,(uint32_t)stageCopies[frame].size(),stageCopies[frame].data());
  VkBufferMemoryBarrier br{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};br.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;br.dstAccessMask=VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;br.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;br.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;br.buffer=vtx.buf;br.offset=0;br.size=VK_WHOLE_SIZE;
  vkCmdPipelineBarrier(c,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,0,0,nullptr,1,&br,0,nullptr);
  stageCopies[frame].clear(); stageUsed[frame]=true; }
 vkCmdBeginRenderPass(c,&rp,VK_SUBPASS_CONTENTS_INLINE);
 VkViewport vpt{0.f,0.f,(float)extent.width,(float)extent.height,0.f,1.f};VkRect2D scr{{0,0},extent};
 vkCmdSetViewport(c,0,1,&vpt);vkCmdSetScissor(c,0,1,&scr);
 lastDrawn=0;
 auto pushPC=[&](const Mat4&m,bool fog){ PushData pd{}; pd.vp=m; if(fog&&fp.fogEnd>0.f){ pd.fogColor[0]=fp.fogRGB[0];pd.fogColor[1]=fp.fogRGB[1];pd.fogColor[2]=fp.fogRGB[2]; pd.fogParams[0]=fp.fogStart;pd.fogParams[1]=fp.fogEnd;pd.fogParams[2]=1.f; } vkCmdPushConstants(c,pipelineLayout,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(PushData),&pd); };
 if(uiSkyQuads){   // sky (gradient, stars, sun, moon) behind everything
  Mat4 sv;sv.m[0]=2.f/(float)extent.width;sv.m[5]=2.f/(float)extent.height;sv.m[12]=-1.f;sv.m[13]=-1.f;sv.m[15]=1.f;
  vkCmdBindPipeline(c,VK_PIPELINE_BIND_POINT_GRAPHICS,uiPipeline);
  VkDeviceSize o0=0;vkCmdBindVertexBuffers(c,0,1,&uiVB[frame],&o0);vkCmdBindIndexBuffer(c,uiIB,0,VK_INDEX_TYPE_UINT32);
  pushPC(sv,false);
  vkCmdBindDescriptorSets(c,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelineLayout,0,1,&uiSet,0,nullptr);
  vkCmdDrawIndexed(c,uiSkyQuads*6,1,0,0,0);
 }
 if(fp.world&&!chunkList.empty()){
  vkCmdBindPipeline(c,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
  vkCmdBindDescriptorSets(c,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelineLayout,0,1,&descSet,0,nullptr);
  pushPC(fp.vp,true);
  VkBuffer bufs[2]={vtx.buf,instVB[frame]};VkDeviceSize offs[2]={0,0};
  vkCmdBindVertexBuffers(c,0,2,bufs,offs);vkCmdBindIndexBuffer(c,quadIB,0,VK_INDEX_TYPE_UINT16);
  float*inst=static_cast<float*>(instMapped[frame]); uint32_t n=0;
  uint32_t indN=0; auto draw1=[&](uint32_t indexCount,int32_t vOff,uint32_t instIdx){ if(mdi&&indN<kMaxInd){ indMap[frame][indN++]=VkDrawIndexedIndirectCommand{indexCount,1,0,vOff,instIdx}; } else vkCmdDrawIndexed(c,indexCount,1,0,vOff,instIdx); };
  auto draw=[&](uint32_t indexCount,int32_t vOff,uint32_t instIdx){ while(indexCount>98298u){ draw1(98298u,vOff,instIdx); indexCount-=98298u; vOff+=65532; } draw1(indexCount,vOff,instIdx); };   // merged meshes have more than 64K vertices: split into runs the 16-bit quad indices can address
  struct TV{float d;uint32_t gi,n;}; static std::vector<TV> visT; visT.clear();
  for(uint32_t gi=0;gi<(uint32_t)chunkList.size();gi++){
   const GpuChunk&g=chunkList[gi];
   if(n>=kMaxInstances)break;
   if(g.size==16&&g.scale==1.f&&!chunkInRange((g.ox>>4)-fp.chunkX,(g.oz>>4)-fp.chunkZ,fp.renderDist))continue;
   const float ox=float(g.ox)-fp.camera.x,oy=-fp.camera.y,oz=float(g.oz)-fp.camera.z;
   if(!aabbVisible(fp.vp,ox,oy,oz,ox+float(g.size),oy+float(g.maxY),oz+float(g.size)))continue;
   inst[n*4]=ox;inst[n*4+1]=oy;inst[n*4+2]=oz;inst[n*4+3]=g.scale;
   // only the faces that can point at the camera are drawn (the others are back faces anyway)
   forEachBucketRun(g.end,visibleBuckets(ox,oy,oz,g.maxY,float(g.size)),[&](uint32_t first,uint32_t count){ draw(count/4*6,(int32_t)(g.vOff+first),n); });
   if(g.end[7]>g.end[6]){const float dx=ox+float(g.size)*0.5f,dz=oz+float(g.size)*0.5f;visT.push_back(TV{dx*dx+dz*dz,gi,n});}
   n++;
  }
  for(const auto&kv:ents){   // entities (mobs, items, boats ...): each is a small mesh drawn like a chunk
   const EntDraw&d=kv.second; if(n>=kMaxInstances)break;
   const float ox=d.x-fp.camera.x,oy=d.y-fp.camera.y,oz=d.z-fp.camera.z; const float cx=ox+16.f,cy=oy+2.f,cz=oz+16.f;
   if(!aabbVisible(fp.vp,cx-d.r,cy-d.r,cz-d.r,cx+d.r,cy+d.r,cz+d.r))continue;
   inst[n*4]=ox;inst[n*4+1]=oy;inst[n*4+2]=oz;inst[n*4+3]=1.f; draw(d.vCnt/4*6,(int32_t)d.vOff,n); n++;
  }
  if(indN)vkCmdDrawIndexedIndirect(c,indBuf[frame],0,indN,(uint32_t)sizeof(VkDrawIndexedIndirectCommand));   // every opaque chunk and entity in ONE call
  lastDrawn=n;
  if(!visT.empty()){   // translucent pass: far to near, alpha blended, no depth writes
   std::sort(visT.begin(),visT.end(),[](const TV&a,const TV&b){return a.d>b.d;});
   vkCmdBindPipeline(c,VK_PIPELINE_BIND_POINT_GRAPHICS,transPipeline);
   for(const TV&t:visT){const GpuChunk&g=chunkList[t.gi]; uint32_t ci=(g.end[7]-g.end[6])/4*6; int32_t vo=(int32_t)(g.vOff+g.end[6]); while(ci>0){ const uint32_t k=std::min(ci,98298u); vkCmdDrawIndexed(c,k,1,0,vo,t.n); ci-=k; vo+=65532; } }
  }
 }
 if(fp.world&&fp.outline){
  const float e=0.003f; const int nb=fp.outBoxes>0?fp.outBoxes:1; const float cube[6]={0,0,0,1,1,1};
  static const int E[12][2]={{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
  float*lv=static_cast<float*>(lineMapped[frame]); int vc=0;
  for(int bi=0;bi<nb;bi++){ const float*q=fp.outBoxes>0?fp.ob[bi]:cube;
   const float x0=float(fp.ox)-fp.camera.x+q[0]-e,y0=float(fp.oy)-fp.camera.y+q[1]-e,z0=float(fp.oz)-fp.camera.z+q[2]-e,x1=float(fp.ox)-fp.camera.x+q[3]+e,y1=float(fp.oy)-fp.camera.y+q[4]+e,z1=float(fp.oz)-fp.camera.z+q[5]+e;
   const float P[8][3]={{x0,y0,z0},{x1,y0,z0},{x1,y0,z1},{x0,y0,z1},{x0,y1,z0},{x1,y1,z0},{x1,y1,z1},{x0,y1,z1}};
   for(int i=0;i<12;i++)for(int k=0;k<2;k++){ for(int a2=0;a2<3;a2++)lv[vc*3+a2]=P[E[i][k]][a2]; vc++; } }
  vkCmdBindPipeline(c,VK_PIPELINE_BIND_POINT_GRAPHICS,linePipeline);
  VkDeviceSize lo=0;vkCmdBindVertexBuffers(c,0,1,&lineVB[frame],&lo);
  pushPC(fp.vp,false);
  vkCmdDraw(c,(uint32_t)vc,1,0,0);
 }
 if(uiQuadCount){
  Mat4 uiVp;uiVp.m[0]=2.f/(float)extent.width;uiVp.m[5]=2.f/(float)extent.height;uiVp.m[12]=-1.f;uiVp.m[13]=-1.f;uiVp.m[15]=1.f;
  vkCmdBindPipeline(c,VK_PIPELINE_BIND_POINT_GRAPHICS,uiPipeline);
  VkDeviceSize off2=0;vkCmdBindVertexBuffers(c,0,1,&uiVB[frame],&off2);vkCmdBindIndexBuffer(c,uiIB,0,VK_INDEX_TYPE_UINT32);
  pushPC(uiVp,false);
  if(uiPanoQuads){
   vkCmdBindDescriptorSets(c,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelineLayout,0,1,&panoSet,0,nullptr);
   vkCmdDrawIndexed(c,uiPanoQuads*6,1,uiSkyQuads*6,0,0);
  }
  if(uiQuadCount>uiSkyQuads+uiPanoQuads){
   vkCmdBindDescriptorSets(c,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelineLayout,0,1,&uiSet,0,nullptr);
   vkCmdDrawIndexed(c,(uiQuadCount-uiSkyQuads-uiPanoQuads)*6,1,(uiSkyQuads+uiPanoQuads)*6,0,0);
  }
 }
 vkCmdEndRenderPass(c);vkEndCommandBuffer(c);
}

// Records the pending animated-texture uploads (water, lava ...) into this frame's command buffer, including mip levels 1..4.
void VulkanRenderer::flushTileUpdates(VkCommandBuffer c,int frame){
 if(pendingTiles.empty()||!blockTex.img||!tileMap[frame])return;
 const size_t n=std::min<size_t>(pendingTiles.size(),256); std::vector<VkBufferImageCopy> rg; rg.reserve(n*5);
 for(size_t i=0;i<n;i++){
  uint8_t*dst=tileMap[frame]+i*1364; std::memcpy(dst,pendingTiles[i].rgba,1024); tileMips(pendingTiles[i].rgba,dst+1024);
  const int tx=pendingTiles[i].id%TexturePackCols,ty=pendingTiles[i].id/TexturePackCols; size_t off=i*1364;
  for(uint32_t l=0;l<blockTex.levels;l++){ const uint32_t sz=16u>>l; VkBufferImageCopy r{}; r.bufferOffset=off; r.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,l,0,1}; r.imageOffset={int32_t(tx*sz),int32_t(ty*sz),0}; r.imageExtent={sz,sz,1}; rg.push_back(r); off+=(size_t)sz*sz*4; }
 }
 pendingTiles.erase(pendingTiles.begin(),pendingTiles.begin()+(long)n);
 VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.srcAccessMask=VK_ACCESS_SHADER_READ_BIT;b.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;b.oldLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;b.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;b.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.image=blockTex.img;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,blockTex.levels,0,1};
 vkCmdPipelineBarrier(c,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&b);
 vkCmdCopyBufferToImage(c,tileBuf[frame],blockTex.img,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,(uint32_t)rg.size(),rg.data());
 VkImageMemoryBarrier b2=b;b2.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;b2.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;b2.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;b2.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
 vkCmdPipelineBarrier(c,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,nullptr,0,nullptr,1,&b2);
}

bool VulkanRenderer::init(GLFWwindow*w,const AtlasImage&blk,const AtlasImage&ui,const PanoramaImage&pano){
 m_window=w;
 try{
  if(!createInstance()||!createSurface()||!pickDevice()||!createDevice()||!createSwapchain()||!createDepth()||!createRenderPass()||!createDescriptors()||!createPipeline()||!createFramebuffers()||!createCommandPool())return false;
  if(!createTextures(blk,ui,pano))return false;
  if(!createArena(vtx,VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,chunkUnitsCapped(m_rd),sizeof(Vertex),512u*1024)){std::cerr<<"Could not allocate chunk vertex memory\n";return false;}
  if(vtx.staged){ for(int i=0;i<2;i++){ void*mp=nullptr; if(!createHostBuffer(kStageBytes,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,stageBuf[i],stageMem[i],&mp)){ std::cerr<<"Could not allocate the upload staging buffer\n"; return false; } stageMap[i]=(uint8_t*)mp; } }
  if(mdi){ for(int i=0;i<2;i++){ void*ip=nullptr; if(!createHostBuffer((VkDeviceSize)kMaxInd*sizeof(VkDrawIndexedIndirectCommand),VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,indBuf[i],indMem[i],&ip)){ mdi=false; break; } indMap[i]=(VkDrawIndexedIndirectCommand*)ip; } }
  std::cout<<"Draw calls: "<<(mdi?"multi draw indirect (one call for all chunks)":"one call per chunk")<<"\n";
  { std::vector<uint16_t> qi; qi.reserve(16384*6); for(uint32_t q=0;q<16384;q++){ const uint16_t b0=(uint16_t)(q*4); qi.insert(qi.end(),{b0,(uint16_t)(b0+1),(uint16_t)(b0+2),b0,(uint16_t)(b0+2),(uint16_t)(b0+3)}); }
    if(!uploadToDevice(qi.data(),qi.size()*sizeof(uint16_t),VK_BUFFER_USAGE_INDEX_BUFFER_BIT,quadIB,quadIM)){std::cerr<<"Could not create the quad index buffer\n";return false;} }
  std::cout<<"Chunk memory: "<<(vtx.capacity/1024*sizeof(Vertex)/1024)<<" MB vertices (no per-chunk index data)\n";
  if(!createDynamicBuffers()||!createSync())return false;
  return true;
 }catch(const std::exception&e){std::cerr<<e.what()<<"\n";return false;}
}

void VulkanRenderer::trim(){ for(auto it=retired.begin();it!=retired.end();){ if(frameCounter>=it->frame+3){vtx.release(it->vOff,it->vCnt);it=retired.erase(it);} else ++it; } }   // never waits for the GPU
void VulkanRenderer::draw(const FrameParams&fp,const UiVertex*ui,uint32_t skyQuads,uint32_t panoQuads,uint32_t totalQuads){
 if(swapDirty){swapDirty=false;recreate();}
 vkWaitForFences(device,1,&inFlight[frame],VK_TRUE,UINT64_MAX);
 frameCounter++; collectRetired();
 uint32_t image;VkResult r=vkAcquireNextImageKHR(device,swapchain,UINT64_MAX,imageAvailable[frame],VK_NULL_HANDLE,&image);
 if(r==VK_ERROR_OUT_OF_DATE_KHR){recreate();return;}
 vkResetFences(device,1,&inFlight[frame]);
 uiQuadCount=std::min(totalQuads,kMaxUiQuads);uiSkyQuads=std::min(skyQuads,uiQuadCount);uiPanoQuads=std::min(panoQuads,uiQuadCount-uiSkyQuads);
 if(uiQuadCount&&uiMapped[frame])std::memcpy(uiMapped[frame],ui,(size_t)uiQuadCount*4*sizeof(UiVertex));
 vkResetCommandBuffer(cmd[frame],0);record(cmd[frame],image,fp);
 VkPipelineStageFlags wait=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
 VkSubmitInfo s{VK_STRUCTURE_TYPE_SUBMIT_INFO};s.waitSemaphoreCount=1;s.pWaitSemaphores=&imageAvailable[frame];s.pWaitDstStageMask=&wait;s.commandBufferCount=1;s.pCommandBuffers=&cmd[frame];s.signalSemaphoreCount=1;s.pSignalSemaphores=&renderFinished[frame];
 vkQueueSubmit(graphicsQueue,1,&s,inFlight[frame]);
 VkPresentInfoKHR p{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};p.waitSemaphoreCount=1;p.pWaitSemaphores=&renderFinished[frame];p.swapchainCount=1;p.pSwapchains=&swapchain;p.pImageIndices=&image;
 r=vkQueuePresentKHR(presentQueue,&p);if(r==VK_ERROR_OUT_OF_DATE_KHR||r==VK_SUBOPTIMAL_KHR)recreate();
 frame=(frame+1)%2;
}

void VulkanRenderer::shutdown(){
 if(device){
 vkDeviceWaitIdle(device);
 for(int i=0;i<2;i++){ if(tileBuf[i])vkDestroyBuffer(device,tileBuf[i],nullptr); if(tileMem[i]){vkUnmapMemory(device,tileMem[i]);vkFreeMemory(device,tileMem[i],nullptr);} tileBuf[i]=VK_NULL_HANDLE;tileMem[i]=VK_NULL_HANDLE;tileMap[i]=nullptr; }
 for(int i=0;i<2&&i<(int)inFlight.size();i++){vkDestroyFence(device,inFlight[i],nullptr);vkDestroySemaphore(device,imageAvailable[i],nullptr);vkDestroySemaphore(device,renderFinished[i],nullptr);}
 for(int i=0;i<2;i++){ if(indMem[i]){vkUnmapMemory(device,indMem[i]);vkFreeMemory(device,indMem[i],nullptr);indMem[i]=VK_NULL_HANDLE;} if(indBuf[i]){vkDestroyBuffer(device,indBuf[i],nullptr);indBuf[i]=VK_NULL_HANDLE;} }
 for(int i=0;i<2;i++){ if(stageMem[i]){vkUnmapMemory(device,stageMem[i]);vkFreeMemory(device,stageMem[i],nullptr);stageMem[i]=VK_NULL_HANDLE;} if(stageBuf[i]){vkDestroyBuffer(device,stageBuf[i],nullptr);stageBuf[i]=VK_NULL_HANDLE;} }
 destroyArena(vtx); if(quadIB)vkDestroyBuffer(device,quadIB,nullptr); if(quadIM)vkFreeMemory(device,quadIM,nullptr);
 destroyTexture(blockTex);destroyTexture(uiTex);destroyTexture(panoTex);
 for(int i=0;i<2;i++){
  if(uiVB[i])vkDestroyBuffer(device,uiVB[i],nullptr);if(uiVM[i])vkFreeMemory(device,uiVM[i],nullptr);
  if(instVB[i])vkDestroyBuffer(device,instVB[i],nullptr);if(instVM[i])vkFreeMemory(device,instVM[i],nullptr);
  if(lineVB[i])vkDestroyBuffer(device,lineVB[i],nullptr);if(lineVM[i])vkFreeMemory(device,lineVM[i],nullptr);
 }
 if(uiIB)vkDestroyBuffer(device,uiIB,nullptr);if(uiIM)vkFreeMemory(device,uiIM,nullptr);
 if(descPool)vkDestroyDescriptorPool(device,descPool,nullptr);if(descLayout)vkDestroyDescriptorSetLayout(device,descLayout,nullptr);
 if(pipeline)vkDestroyPipeline(device,pipeline,nullptr);if(transPipeline)vkDestroyPipeline(device,transPipeline,nullptr);if(uiPipeline)vkDestroyPipeline(device,uiPipeline,nullptr);if(linePipeline)vkDestroyPipeline(device,linePipeline,nullptr);
 if(pipelineLayout)vkDestroyPipelineLayout(device,pipelineLayout,nullptr);if(renderPass)vkDestroyRenderPass(device,renderPass,nullptr);if(commandPool)vkDestroyCommandPool(device,commandPool,nullptr);
 destroySwapchain();if(depthView)vkDestroyImageView(device,depthView,nullptr);if(depthImage)vkDestroyImage(device,depthImage,nullptr);if(depthMemory)vkFreeMemory(device,depthMemory,nullptr);
 vkDestroyDevice(device,nullptr);device=VK_NULL_HANDLE;
 }
 if(surface){vkDestroySurfaceKHR(instance,surface,nullptr);surface=VK_NULL_HANDLE;}
 if(instance){vkDestroyInstance(instance,nullptr);instance=VK_NULL_HANDLE;}
}
