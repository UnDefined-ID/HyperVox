#include <GLFW/glfw3.h>
#include <filesystem>
#include <iostream>
#include <memory>
#include "Game.hpp"
#include "GlRenderer.hpp"
#include "VulkanRenderer.hpp"

static GLFWwindow* makeWindow(bool opengl){
 glfwDefaultWindowHints(); glfwWindowHint(GLFW_RESIZABLE,GLFW_TRUE);
 if(opengl){
  glfwWindowHint(GLFW_CLIENT_API,GLFW_OPENGL_API); glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
  glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE); glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT,GLFW_TRUE); glfwWindowHint(GLFW_SRGB_CAPABLE,GLFW_TRUE);
 } else glfwWindowHint(GLFW_CLIENT_API,GLFW_NO_API);
 return glfwCreateWindow(1280,720,"hypervox",nullptr,nullptr);
}

int main(int argc,char**argv){
 if(argc>0){std::error_code ec;auto exe=std::filesystem::weakly_canonical(argv[0],ec);if(!ec&&exe.has_parent_path())std::filesystem::current_path(exe.parent_path(),ec);}
 if(!glfwInit()){std::cerr<<"GLFW init failed\n";return 1;}
 Settings pre; pre.load();
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
   if(!game.loadResources()){std::cerr<<"Could not load the texture pack (put minecraft-assets.zip in assets/ and rebuild).\n";rc=1;}
   else{
    std::unique_ptr<Renderer> renderer; if(tryGL)renderer=std::make_unique<GlRenderer>(); else renderer=std::make_unique<VulkanRenderer>();
    renderer->setVsync(game.settings().vsync); renderer->setMipmaps(game.settings().mipmaps);
    if(!renderer->init(window,game.blockAtlas(),game.uiAtlasImage(),game.panorama())){
     std::cerr<<renderer->name()<<" initialization failed.\n"; renderer->shutdown(); renderer.reset();
     if(!tryGL)fallback=true;                                       // retry the whole start-up with OpenGL
    }else{
     std::cout<<"Renderer: "<<renderer->name()<<"\n";
     game.attach(renderer.get());
     while(!glfwWindowShouldClose(window)){glfwPollEvents();game.frame();}
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
