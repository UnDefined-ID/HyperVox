#include "Assets.hpp"
#include "Embedded.hpp"
#include "Settings.hpp"
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <zlib.h>
#include <fstream>
#include <sstream>
#include <system_error>
namespace fs=std::filesystem;

fs::path AssetPaths::resolve(const std::string&rel)const{
 if(!overlay.empty()){fs::path p=overlay/rel;std::error_code ec;if(fs::exists(p,ec))return p;}
 return base/rel;
}
bool AssetPaths::exists(const std::string&rel)const{std::error_code ec;return fs::exists(resolve(rel),ec);}

fs::path AssetPaths::findBase(const fs::path&root){
 std::error_code ec;
 const fs::path c[]={root,root/"assets",root.parent_path(),root/"minecraft",root/"assets/minecraft/textures",root/"minecraft/textures"};
 for(auto&p:c)if(!p.empty()&&fs::exists(p/"block",ec))return p;
 for(auto&e:fs::directory_iterator(root,ec)){ if(e.is_directory(ec)&&fs::exists(e.path()/"block",ec))return e.path(); }   // zip with a top-level folder
 return fs::path();
}
fs::path AssetPaths::normalizePackRoot(const fs::path&dir){
 std::error_code ec;
 if(fs::exists(dir/"assets/minecraft/textures",ec))return dir/"assets/minecraft/textures";
 return dir;
}

static uint64_t fnv(uint64_t h,const void*d,size_t n){const unsigned char*p=(const unsigned char*)d;for(size_t i=0;i<n;i++){h^=p[i];h*=1099511628211ull;}return h;}
uint64_t AssetPaths::signature(uint32_t ver)const{
 uint64_t h=1469598103934665603ull; h=fnv(h,&ver,sizeof ver); h=fnv(h,packName.data(),packName.size());
 const char* dirs[]={"block","item","gui","font"};
 for(const fs::path*root:{&base,&overlay}){
  if(root->empty())continue;
  for(const char*d:dirs){
   std::error_code ec; const fs::path dir=*root/d; if(!fs::exists(dir,ec))continue;
   std::vector<std::string> lines;
   for(fs::recursive_directory_iterator it(dir,ec),end;!ec&&it!=end;it.increment(ec)){
    if(!it->is_regular_file(ec)||it->path().extension()!=".png")continue;
    std::error_code e2; const auto sz=fs::file_size(it->path(),e2); const auto mt=fs::last_write_time(it->path(),e2).time_since_epoch().count();
    lines.push_back(fs::relative(it->path(),*root,e2).generic_string()+"|"+std::to_string(sz)+"|"+std::to_string((long long)mt));
   }
   std::sort(lines.begin(),lines.end());
   for(auto&l:lines)h=fnv(h,l.data(),l.size());
  }
  h=fnv(h,"|",1);
 }
 return h;
}

std::string safeFileName(const std::string&s){std::string r;for(char c:s)r+=((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_')?c:'_';return r.empty()?"pack":r;}

static std::string readDescription(const fs::path&meta){
 std::ifstream f(meta); if(!f)return std::string(); std::stringstream ss; ss<<f.rdbuf(); const std::string s=ss.str();
 size_t k=s.find("\"description\""); if(k==std::string::npos)return std::string();
 k=s.find(':',k); if(k==std::string::npos)return std::string();
 size_t q=s.find('"',k); if(q==std::string::npos)return std::string();
 std::string out; for(size_t i=q+1;i<s.size()&&s[i]!='"';i++){ if(s[i]=='\\'&&i+1<s.size()){i++; if(s[i]=='n'){out+=' ';continue;} } out+=s[i]; }
 return out.substr(0,60);
}

// ---------------------------------------------------------------------------------------------- minimal zip reader (stored + deflate)
namespace{
struct ZipEntry{ std::string name; uint32_t method=0,csize=0,usize=0,lho=0; };
uint32_t rd32(const unsigned char*p){return p[0]|(p[1]<<8)|(p[2]<<16)|((uint32_t)p[3]<<24);} uint32_t rd16(const unsigned char*p){return p[0]|(p[1]<<8);}
bool zipList(std::ifstream&f,std::vector<ZipEntry>&out){
 f.seekg(0,std::ios::end); const std::streamoff size=f.tellg(); if(size<22)return false;
 const std::streamoff tail=std::min<std::streamoff>(size,65557); std::vector<unsigned char> t((size_t)tail); f.seekg(size-tail); f.read((char*)t.data(),tail); if(!f)return false;
 long e=-1; for(long i=(long)tail-22;i>=0;i--)if(rd32(&t[(size_t)i])==0x06054b50u){e=i;break;} if(e<0)return false;
 const uint32_t count=rd16(&t[(size_t)e+10]),cdSize=rd32(&t[(size_t)e+12]),cdOff=rd32(&t[(size_t)e+16]); if(cdOff==0xFFFFFFFFu)return false;
 std::vector<unsigned char> cd(cdSize); f.seekg(cdOff); f.read((char*)cd.data(),cdSize); if(!f)return false;
 size_t p=0; for(uint32_t i=0;i<count&&p+46<=cd.size();i++){
  if(rd32(&cd[p])!=0x02014b50u)break;
  ZipEntry z; z.method=rd16(&cd[p+10]); z.csize=rd32(&cd[p+20]); z.usize=rd32(&cd[p+24]); const uint32_t nl=rd16(&cd[p+28]),xl=rd16(&cd[p+30]),cl=rd16(&cd[p+32]); z.lho=rd32(&cd[p+42]);
  if(p+46+nl>cd.size())break; z.name.assign((const char*)&cd[p+46],nl); p+=46+nl+xl+cl;
  if(z.csize!=0xFFFFFFFFu&&z.usize!=0xFFFFFFFFu&&z.lho!=0xFFFFFFFFu)out.push_back(std::move(z));
 }
 return !out.empty();
}
bool zipRead(std::ifstream&f,const ZipEntry&z,std::vector<unsigned char>&out){
 unsigned char h[30]; f.clear(); f.seekg(z.lho); f.read((char*)h,30); if(!f||rd32(h)!=0x04034b50u)return false;
 f.seekg((std::streamoff)z.lho+30+rd16(h+26)+rd16(h+28)); std::vector<unsigned char> in(z.csize); f.read((char*)in.data(),z.csize); if(!f&&z.csize)return false;
 if(z.method==0){out=std::move(in);return true;} if(z.method!=8)return false;
 out.resize(z.usize); z_stream s{}; if(inflateInit2(&s,-15)!=Z_OK)return false;
 s.next_in=in.data(); s.avail_in=(uInt)in.size(); s.next_out=out.data(); s.avail_out=(uInt)out.size(); const int r=inflate(&s,Z_FINISH); inflateEnd(&s);
 return r==Z_STREAM_END||(r==Z_OK&&s.avail_out==0);
}
// maps a zip entry name to a path inside the unpacked pack ("" = not needed)
std::string packRel(const std::string&n){
 if(n.empty()||n.back()=='/'||n.find("..")!=std::string::npos)return std::string();
 const size_t k=n.find("assets/minecraft/textures/"); if(k!=std::string::npos)return n.substr(k);
 const size_t sl=n.find('/'); const std::string base=sl==std::string::npos?n:n.substr(sl+1);   // pack.mcmeta / pack.png at the top (or inside one wrapper folder)
 if((base=="pack.mcmeta"||base=="pack.png")&&base.find('/')==std::string::npos)return base;
 return std::string();
}
std::string zipText(const fs::path&zip,const char*entry){
 std::ifstream f(zip,std::ios::binary); std::vector<ZipEntry> es; if(!f||!zipList(f,es))return std::string();
 for(auto&z:es)if(packRel(z.name)==entry){std::vector<unsigned char> d; if(zipRead(f,z,d))return std::string(d.begin(),d.end());}
 return std::string();
}
}

bool materializePack(const PackInfo&p){
 if(p.zip.empty())return true;
 std::error_code ec; const std::string stamp=std::to_string((long long)fs::file_size(p.zip,ec))+"|"+std::to_string((long long)fs::last_write_time(p.zip,ec).time_since_epoch().count());
 { std::ifstream s(p.dir/".stamp"); std::string old; if(s&&std::getline(s,old)&&old==stamp)return true; }
 fs::remove_all(p.dir,ec); fs::create_directories(p.dir,ec);
 std::ifstream f(p.zip,std::ios::binary); std::vector<ZipEntry> es; if(!f||!zipList(f,es))return false;
 int n=0;
 for(auto&z:es){ const std::string rel=packRel(z.name); if(rel.empty())continue;
  std::vector<unsigned char> d; if(!zipRead(f,z,d))continue; const fs::path dst=p.dir/rel; fs::create_directories(dst.parent_path(),ec);
  std::ofstream o(dst,std::ios::binary|std::ios::trunc); o.write((const char*)d.data(),(std::streamsize)d.size()); n++; }
 if(!n)return false;
 std::ofstream(p.dir/".stamp")<<stamp<<"\n"; return true;
}

bool extractZipAll(const fs::path&zip,const fs::path&dest){
 std::ifstream f(zip,std::ios::binary); std::vector<ZipEntry> es; if(!f||!zipList(f,es))return false; std::error_code ec; fs::create_directories(dest,ec); int n=0;
 for(auto&z:es){ if(z.name.empty()||z.name.back()=='/'||z.name.find("..")!=std::string::npos)continue;
  std::vector<unsigned char> d; if(!zipRead(f,z,d))continue; const fs::path dst=dest/z.name; fs::create_directories(dst.parent_path(),ec);
  std::ofstream o(dst,std::ios::binary|std::ios::trunc); o.write((const char*)d.data(),(std::streamsize)d.size()); n++; }
 return n>0;
}
fs::path ensureDataFolder(const char*kind){
 std::error_code ec; const bool tex=std::string(kind)=="assets"; const fs::path data=Settings::dataDir(),dest=data/kind;
 auto ready=[&](const fs::path&p){ if(!fs::is_directory(p,ec))return false; if(tex)return !AssetPaths::findBase(p).empty(); for(auto&e:fs::recursive_directory_iterator(p,ec)){ if(e.path().extension()==".ogg")return true; } return false; };
 auto stampOf=[&](const fs::path&z){ const auto t=fs::last_write_time(z,ec).time_since_epoch().count(); return z.filename().string()+"|"+std::to_string((unsigned long long)fs::file_size(z,ec))+"|"+std::to_string((long long)t); };
 // the zip that should be unpacked: one next to the game, else the copy linked into the executable
 std::vector<fs::path> dirs{fs::current_path(ec),fs::current_path(ec)/"assets",fs::current_path(ec).parent_path(),fs::current_path(ec).parent_path()/"assets",data};
 fs::path zip; { const char*fn=tex?"hypervox_assets.zip":"hypervox_sounds.zip"; for(auto&d:dirs){ if(!d.empty()&&fs::is_regular_file(d/fn,ec)){ zip=d/fn; break; } } }
 for(auto&d:dirs){ if(!zip.empty())break; if(!fs::is_directory(d,ec))continue;
  for(auto&e:fs::directory_iterator(d,ec)){ std::string lo=e.path().filename().string(); for(auto&ch:lo)ch=(char)std::tolower((unsigned char)ch);
   if(lo.size()<4||lo.compare(lo.size()-4,4,".zip")!=0||lo.find(tex?"assets":"sound")==std::string::npos||lo[0]=='.')continue; if(zip.empty()||fs::last_write_time(e.path(),ec)>fs::last_write_time(zip,ec))zip=e.path(); } }
 const EmbeddedBlob emb=embeddedBlob(tex?"assets":"sounds"); const std::string want=!zip.empty()?stampOf(zip):(emb.p?std::string("embedded|")+std::to_string(emb.n):std::string());
 auto readStamp=[&]{ std::ifstream f(dest/".source"); std::string s; std::getline(f,s); return s; };
 if(ready(dest)&&(want.empty()||readStamp()==want))return dest;   // up to date (or nothing to compare with)
 if(want.empty())return ready(dest)?dest:fs::path();
 if(fs::exists(dest,ec)){ std::cout<<"[hypervox] the "<<kind<<" zip changed - unpacking the new one\n"; fs::remove_all(dest,ec); }
 bool ok=false;
 if(!zip.empty()){ std::cout<<"[hypervox] first start: unpacking "<<zip<<" into "<<dest<<" ...\n"; ok=extractZipAll(zip,dest)&&ready(dest); }
 else { const fs::path tmp=data/(tex?".embedded-assets.zip":".embedded-sounds.zip"); fs::create_directories(data,ec); { std::ofstream o(tmp,std::ios::binary|std::ios::trunc); o.write((const char*)emb.p,(std::streamsize)emb.n); }
  std::cout<<"[hypervox] first start: unpacking the built-in "<<kind<<" into "<<dest<<" ...\n"; ok=extractZipAll(tmp,dest)&&ready(dest); fs::remove(tmp,ec); }
 if(ok){ std::ofstream(dest/".source")<<want<<"\n"; return dest; }
 return fs::path();
}
bool importPackOrSkin(const fs::path&file,const fs::path&destDir){
 std::error_code ec; fs::create_directories(destDir,ec); fs::path dst=destDir/file.filename();
 for(int i=1;fs::exists(dst,ec);i++)dst=destDir/(file.stem().string()+" ("+std::to_string(i)+")"+file.extension().string());
 if(fs::is_directory(file,ec))fs::copy(file,dst,fs::copy_options::recursive,ec); else fs::copy_file(file,dst,ec);
 return !ec;
}
void openFolder(const fs::path&dir){
 std::error_code ec; fs::create_directories(dir,ec); const std::string p=dir.string();
#ifdef _WIN32
 const std::string cmd="start \"\" \""+p+"\"";
#elif defined(__APPLE__)
 const std::string cmd="open \""+p+"\" &";
#else
 const std::string cmd="xdg-open \""+p+"\" >/dev/null 2>&1 &";
#endif
 if(std::system(cmd.c_str())!=0){}
}

std::vector<PackInfo> listResourcePacks(const AssetPaths&defaults,const std::vector<fs::path>&searchDirs,const fs::path&extractRoot){
 std::vector<PackInfo> r; std::error_code ec;
 PackInfo d; d.name="Default"; d.description="The bundled Minecraft texture pack"; d.dir=defaults.base; d.builtin=true; d.icon=defaults.base/"misc/unknown_pack.png"; r.push_back(d);
 auto known=[&](const std::string&n){for(auto&p:r)if(p.name==n)return true;return false;};
 for(auto&sd:searchDirs){
  std::vector<fs::path> items; for(auto&e:fs::directory_iterator(sd,ec))items.push_back(e.path()); std::sort(items.begin(),items.end());
  for(auto&it:items){
   const std::string fn=it.filename().string(); if(fn.empty()||fn[0]=='.'||known(fn))continue;
   if(fs::is_directory(it,ec)){
    const fs::path root=AssetPaths::normalizePackRoot(it);
    if(!fs::exists(root/"block",ec)&&!fs::exists(root/"item",ec)&&!fs::exists(root/"gui",ec))continue;
    PackInfo p; p.name=fn; p.dir=it; p.description=readDescription(it/"pack.mcmeta"); if(p.description.empty())p.description="Resource pack";
    p.icon=fs::exists(it/"pack.png",ec)?it/"pack.png":defaults.base/"misc/unknown_pack.png"; r.push_back(p);
   }else if(it.extension()==".zip"||it.extension()==".ZIP"){
    PackInfo p; p.name=fn; p.zip=it; p.dir=extractRoot/safeFileName(it.stem().string()); p.icon=defaults.base/"misc/unknown_pack.png";
    const std::string meta=zipText(it,"pack.mcmeta"); std::string desc;
    { const size_t k=meta.find("\"description\""); if(k!=std::string::npos){ size_t q=meta.find('"',meta.find(':',k)+1); if(q!=std::string::npos)for(size_t i=q+1;i<meta.size()&&meta[i]!='"';i++){ if(meta[i]=='\\'&&i+1<meta.size()){i++;if(meta[i]=='n'){desc+=' ';continue;}} desc+=meta[i]; } } }
    p.description=desc.empty()?"Resource pack (zip)":desc.substr(0,60); r.push_back(p);
   }
  }
 }
 return r;
}
