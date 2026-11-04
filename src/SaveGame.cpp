#include "SaveGame.hpp"
#include "Settings.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <sstream>
#include <system_error>

namespace fs=std::filesystem;
SaveManager::SaveManager(){
 // worlds live in the data folder (~/.local/share/hypervox/saves, %APPDATA%\\hypervox\\saves), not next to the executable
 m_root=Settings::savesDir(); std::error_code ec;
 // one-time import of worlds saved by older builds (<exe dir>/worlds and the old FastVoxelVK data folder); the originals are left untouched
 std::vector<fs::path> old{fs::current_path(ec)/"worlds"}; const fs::path lg=Settings::legacyDataDir(); if(!lg.empty())old.push_back(lg/"worlds");
 for(auto&o:old){ if(!fs::is_directory(o,ec))continue; std::error_code e2;
  for(auto&e:fs::directory_iterator(o,e2)){ if(!e.is_directory(ec)||!fs::exists(e.path()/"level.dat",ec))continue; const fs::path dst=m_root/e.path().filename(); if(!fs::exists(dst,ec))fs::copy(e.path(),dst,fs::copy_options::recursive,ec); } }
}

std::string SaveManager::sanitize(const std::string&name){
 std::string r; for(char c:name){ if((c>='0'&&c<='9')||(c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='-'||c=='_'||c==' ')r+=c; }
 while(!r.empty()&&r.back()==' ')r.pop_back();
 while(!r.empty()&&r.front()==' ')r.erase(r.begin());
 if(r.empty())r="New World";
 return r;
}
uint64_t SaveManager::seedFromString(const std::string&s){
 if(s.empty()){ return (uint64_t)std::chrono::steady_clock::now().time_since_epoch().count()*0x9E3779B97F4A7C15ULL ^ (uint64_t)std::chrono::system_clock::now().time_since_epoch().count(); }
 bool num=true; size_t i=(s[0]=='-'||s[0]=='+')?1:0; if(i>=s.size())num=false; for(size_t k=i;k<s.size();k++)if(s[k]<'0'||s[k]>'9')num=false;
 if(num&&s.size()<=19){ try{ return (uint64_t)std::stoll(s); }catch(...){} }                    // numbers: used as-is (64 bit, like Minecraft)
 int32_t h=0; for(unsigned char ch:s)h=int32_t(uint32_t(h)*31u+ch); return (uint64_t)(int64_t)h;   // text: Java String.hashCode() like the real game
}

std::vector<WorldInfo> SaveManager::list() const{
 std::vector<WorldInfo> r; std::error_code ec;
 for(auto&e:fs::directory_iterator(m_root,ec)){ if(!e.is_directory())continue; WorldInfo w; if(readLevel(e.path(),w))r.push_back(std::move(w)); }
 std::sort(r.begin(),r.end(),[](const WorldInfo&a,const WorldInfo&b){return a.lastPlayed>b.lastPlayed;});
 return r;
}

bool SaveManager::create(const std::string&name,int mode,uint64_t seed,WorldInfo&out){
 std::string base=sanitize(name),folder=base; std::error_code ec;
 for(int n=1;fs::exists(m_root/folder,ec);n++)folder=base+" ("+std::to_string(n)+")";
 if(!fs::create_directories(m_root/folder,ec))return false;
 out=WorldInfo{}; out.folder=folder; out.name=name.empty()?base:name; out.mode=mode; out.seed=seed;
 out.created=out.lastPlayed=(int64_t)std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
 if(mode==MODE_CREATIVE){ /* creative inventory is infinite, start with an empty hotbar */ }
 return saveLevel(out);
}
bool SaveManager::remove(const std::string&folder){
 if(folder.empty()||folder.find('/')!=std::string::npos||folder.find('\\')!=std::string::npos||folder==".."||folder==".")return false;
 std::error_code ec; fs::remove_all(m_root/folder,ec); return !ec;
}

