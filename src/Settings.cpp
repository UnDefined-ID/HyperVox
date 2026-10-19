#include "Settings.hpp"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <system_error>

namespace{
// GLFW key codes (stable ABI values) so this file needs no GLFW headers.
constexpr int K_SPACE=32,K_A=65,K_D=68,K_E=69,K_F=70,K_S=83,K_W=87,K_LSHIFT=340,K_LCTRL=341;
const char* kKeyNames[ACT_COUNT]={"key_forward","key_back","key_left","key_right","key_jump","key_descend","key_sprint","key_fly","key_inventory"};
const char* kAppDir="hypervox";
}

void Settings::resetKeys(){
 keys[ACT_FWD]=K_W;keys[ACT_BACK]=K_S;keys[ACT_LEFT]=K_A;keys[ACT_RIGHT]=K_D;keys[ACT_JUMP]=K_SPACE;
 keys[ACT_DOWN]=K_LCTRL;keys[ACT_SPRINT]=K_LSHIFT;keys[ACT_FLY]=K_F;keys[ACT_INVENTORY]=K_E;
}
void Settings::clamp(){
 renderDistance=std::clamp(renderDistance,kMinRender,kHardMaxRender); fov=std::clamp(fov,30,110); mouseSens=std::clamp(mouseSens,0.f,1.f); guiScale=std::clamp(guiScale,0,6); ramLimitMB=std::clamp(ramLimitMB,256,262144); cleanerMode=std::clamp(cleanerMode,0,2);
}

std::filesystem::path Settings::dataDir(){
 namespace fs=std::filesystem; fs::path dir;
 if(const char*o=std::getenv("HYPERVOX_HOME");o&&*o)dir=o;
 else if(const char*o2=std::getenv("FASTVOXEL_HOME");o2&&*o2)dir=o2;
#ifdef _WIN32
 else if(const char*a=std::getenv("APPDATA");a&&*a)dir=fs::path(a)/kAppDir;
#elif defined(__APPLE__)
 else if(const char*h=std::getenv("HOME");h&&*h)dir=fs::path(h)/"Library/Application Support"/kAppDir;
#else
 else if(const char*x=std::getenv("XDG_DATA_HOME");x&&*x)dir=fs::path(x)/kAppDir;
 else if(const char*h=std::getenv("HOME");h&&*h)dir=fs::path(h)/".local/share"/kAppDir;
#endif
 std::error_code ec;
 { const fs::path lg=legacyDataDir(); if(!dir.empty()&&!lg.empty()&&!fs::exists(dir,ec)&&fs::exists(lg,ec)){ fs::create_directories(dir.parent_path(),ec); fs::rename(lg,dir,ec); } }   // FastVoxelVK -> hypervox
 if(dir.empty()||(fs::create_directories(dir,ec),!fs::is_directory(dir,ec))){ dir=fs::current_path(ec)/"data"; fs::create_directories(dir,ec); }
 return dir;
}
std::filesystem::path Settings::file(){return dataDir()/"options.txt";}
std::filesystem::path Settings::legacyDataDir(){
 namespace fs=std::filesystem;
#ifdef _WIN32
 if(const char*a=std::getenv("APPDATA");a&&*a)return fs::path(a)/"FastVoxelVK";
#elif defined(__APPLE__)
 if(const char*h=std::getenv("HOME");h&&*h)return fs::path(h)/"Library/Application Support/FastVoxelVK";
#else
 if(const char*x=std::getenv("XDG_DATA_HOME");x&&*x)return fs::path(x)/"FastVoxelVK";
 if(const char*h=std::getenv("HOME");h&&*h)return fs::path(h)/".local/share/FastVoxelVK";
#endif
 return fs::path();
}
std::filesystem::path Settings::skinsDir(){auto d=dataDir()/"skins";std::error_code ec;std::filesystem::create_directories(d,ec);return d;}
std::filesystem::path Settings::savesDir(){auto d=dataDir()/"saves";std::error_code ec;std::filesystem::create_directories(d,ec);return d;}
std::vector<std::filesystem::path> Settings::packDirs(){
 namespace fs=std::filesystem; std::vector<fs::path> r; std::error_code ec;
 auto add=[&](const fs::path&p){ if(p.empty())return; fs::create_directories(p,ec); for(auto&q:r){std::error_code e2;if(fs::equivalent(q,p,e2))return;} if(fs::is_directory(p,ec))r.push_back(p); };
 add(dataDir()/"resourcepacks");                               // main folder: the "Add Texture Pack" button opens this one
 const fs::path cwd=fs::current_path(ec);
 add(cwd/"resourcepacks"); add(cwd.parent_path()/"resourcepacks");   // next to the executable / the project folder
 const fs::path lg=legacyDataDir(); if(!lg.empty()&&fs::exists(lg/"resourcepacks",ec))add(lg/"resourcepacks");
 // packs you already use with real Minecraft
#ifdef _WIN32
 if(const char*a=std::getenv("APPDATA");a&&*a){ const fs::path m=fs::path(a)/".minecraft"/"resourcepacks"; if(fs::exists(m,ec))add(m); }
#elif defined(__APPLE__)
 if(const char*h=std::getenv("HOME");h&&*h){ const fs::path m=fs::path(h)/"Library/Application Support/minecraft/resourcepacks"; if(fs::exists(m,ec))add(m); }
#else
 if(const char*h=std::getenv("HOME");h&&*h){ const fs::path m=fs::path(h)/".minecraft/resourcepacks"; if(fs::exists(m,ec))add(m); }
#endif
 return r;
}

bool Settings::load(){
 std::ifstream f(file()); if(!f)return false;
 std::string line;
 while(std::getline(f,line)){
  if(!line.empty()&&line.back()=='\r')line.pop_back();
  const size_t c=line.find(':'); if(c==std::string::npos)continue;
  const std::string k=line.substr(0,c),v=line.substr(c+1);
  try{
   if(k=="renderDistance")renderDistance=std::stoi(v);
   else if(k=="fov")fov=std::stoi(v);
   else if(k=="mouseSensitivity")mouseSens=std::stof(v);
   else if(k=="vsync")vsync=(v=="true"||v=="1");
   else if(k=="showDebug")showDebug=(v=="true"||v=="1");
   else if(k=="resourcePack")resourcePack=v;
   else if(k=="guiScale")guiScale=std::stoi(v);
   else if(k=="ramLimitMB")ramLimitMB=std::stoi(v);
   else if(k=="cleanerMode")cleanerMode=std::stoi(v);
   else if(k=="mipmaps")mipmaps=(v!="false"&&v!="0");
   else if(k=="renderer")renderer=(v=="opengl"||v=="1")?1:0;
   else if(k=="username"){if(!v.empty())username=v.substr(0,16);}
   else if(k=="skin"){if(!v.empty())skin=v;}
   else for(int a=0;a<ACT_COUNT;a++)if(k==kKeyNames[a]){const int code=std::stoi(v);if(code>=32)keys[a]=code;}
  }catch(...){}
 }
 clamp(); return true;
}

bool Settings::save() const{
 namespace fs=std::filesystem; const fs::path p=file(), tmp=fs::path(p).concat(".tmp");
 { std::ofstream f(tmp,std::ios::trunc); if(!f)return false;
   f<<"renderDistance:"<<renderDistance<<"\nfov:"<<fov<<"\nmouseSensitivity:"<<mouseSens<<"\nvsync:"<<(vsync?"true":"false")<<"\nshowDebug:"<<(showDebug?"true":"false")<<"\nresourcePack:"<<resourcePack<<"\nguiScale:"<<guiScale<<"\nrenderer:"<<(renderer?"opengl":"vulkan")<<"\nusername:"<<username<<"\nskin:"<<skin<<"\nramLimitMB:"<<ramLimitMB<<"\ncleanerMode:"<<cleanerMode<<"\nmipmaps:"<<(mipmaps?"true":"false")<<"\n";
   for(int a=0;a<ACT_COUNT;a++)f<<kKeyNames[a]<<":"<<keys[a]<<"\n";
   f.flush(); if(!f)return false; }
 std::error_code ec; fs::rename(tmp,p,ec);
 if(ec){ // e.g. Windows refusing to replace: fall back to a direct write
  std::ofstream f(p,std::ios::trunc); if(!f)return false;
  std::ifstream in(tmp); f<<in.rdbuf(); in.close(); fs::remove(tmp,ec);
 }
 return true;
}
