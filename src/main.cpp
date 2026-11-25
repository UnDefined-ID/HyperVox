#include <GLFW/glfw3.h>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#include <chrono>
#include <thread>
#include <filesystem>
#include <iostream>
#include <memory>
#include "Game.hpp"
#include "GlRenderer.hpp"
#include "Log.hpp"
#include "VulkanRenderer.hpp"

static int g_resX=1280,g_resY=720;
static GLFWwindow* makeWindow(bool opengl){
 glfwDefaultWindowHints(); glfwWindowHint(GLFW_RESIZABLE,GLFW_TRUE);
 if(opengl){
  glfwWindowHint(GLFW_CLIENT_API,GLFW_OPENGL_API); glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
  glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE); glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT,GLFW_TRUE); glfwWindowHint(GLFW_SRGB_CAPABLE,GLFW_TRUE);
 } else glfwWindowHint(GLFW_CLIENT_API,GLFW_NO_API);
 return glfwCreateWindow(g_resX,g_resY,"hypervox",nullptr,nullptr);
}

int main(int argc,char**argv){
 logInit();   // logs folder: every run is written to <data folder>/logs, crashes leave a backtrace there
 { // run from the executable's own folder (shaders, sounds, assets are found there) no matter where or how it was started
  std::error_code ec; std::filesystem::path exe;
#if defined(_WIN32)
  wchar_t buf[4096]; const unsigned long n=GetModuleFileNameW(nullptr,buf,4096); if(n>0&&n<4096)exe=std::filesystem::path(std::wstring(buf,n));
#elif defined(__APPLE__)
  char buf[4096]; uint32_t sz=sizeof buf; if(_NSGetExecutablePath(buf,&sz)==0)exe=std::filesystem::weakly_canonical(buf,ec);
#else
  exe=std::filesystem::read_symlink("/proc/self/exe",ec);
#endif
  if(exe.empty()&&argc>0){ec.clear();exe=std::filesystem::weakly_canonical(argv[0],ec);}
  if(!exe.empty()&&exe.has_parent_path())std::filesystem::current_path(exe.parent_path(),ec);
 }
 if(!glfwInit()){std::cerr<<"GLFW init failed\n";return 1;}
 Settings pre; pre.load(); g_resX=pre.resX; g_resY=pre.resY;
 // Vulkan is the default; OpenGL is used when it is selected in the options, when the device has no Vulkan, or when Vulkan fails to start.
 bool tryGL=pre.renderer==1||!glfwVulkanSupported();
 if(!glfwVulkanSupported())std::cout<<"Vulkan is not available on this device - using OpenGL.\n";
 int rc=1;
 for(int attempt=0;attempt<2;attempt++){
  GLFWwindow*window=makeWindow(tryGL);
  if(!window){ std::cerr<<(tryGL?"Could not create an OpenGL 3.3 window.\n":"Could not create a window.\n"); if(!tryGL){tryGL=true;continue;} break; }
  bool fallback=false;
  {
   Game game(window);
   if(!game.loadResources()){std::cerr<<"Could not load the texture pack (put hypervox_assets.zip into the hypervox data folder and restart).\n";rc=1;}
   else{
    std::unique_ptr<Renderer> renderer; if(tryGL)renderer=std::make_unique<GlRenderer>(); else renderer=std::make_unique<VulkanRenderer>();
    renderer->setVsync(game.settings().vsync); renderer->setMipmaps(game.settings().mipmaps); renderer->setChunkMemory(game.settings().renderDistance);
    if(!renderer->init(window,game.blockAtlas(),game.uiAtlasImage(),game.panorama())){
     std::cerr<<renderer->name()<<" initialization failed.\n"; renderer->shutdown(); renderer.reset();
     if(!tryGL)fallback=true;                                       // retry the whole start-up with OpenGL
    }else{
     std::cout<<"Renderer: "<<renderer->name()<<"\n";
     game.attach(renderer.get());
     { using clk=std::chrono::steady_clock; auto next=clk::now();
      while(!glfwWindowShouldClose(window)){
       glfwPollEvents(); game.frame();
       const int cap=game.settings().maxFps;
       if(cap>0){ next+=std::chrono::duration_cast<clk::duration>(std::chrono::duration<double>(1.0/cap)); const auto now=clk::now();
        if(next<now)next=now; else { if(next-now>std::chrono::milliseconds(2))std::this_thread::sleep_for(next-now-std::chrono::milliseconds(1)); while(clk::now()<next)std::this_thread::yield(); } }
       else next=clk::now(); } }
     game.shutdown(); renderer->shutdown(); rc=0;
    }
   }
  }
  glfwDestroyWindow(window);
  if(fallback){ tryGL=true; std::cout<<"Falling back to OpenGL.\n"; continue; }
  break;
 }
 glfwTerminate(); return rc;
}
