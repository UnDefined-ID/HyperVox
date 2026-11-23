#include "SteelClient.hpp"
#include "ChunkStore.hpp"
#include "World.hpp"
#include "Registry.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#if defined(_WIN32)&&!defined(HV_STEEL_STATIC)
namespace steel{ const char* status(){return "Steel helper is not supported on this OS yet";} bool available(){return false;} bool generate(uint64_t,int,int,int,Chunk&){return false;} void shutdown(){} }   // POSIX only for now
#else
#ifndef HV_STEEL_STATIC
#include <arpa/inet.h>
#include <csignal>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#else
#include "Embedded.hpp"
extern "C"{ uint8_t* hv_steel_generate(uint64_t seed,int32_t cx,int32_t cz,uint8_t dimension,size_t*out_len); void hv_steel_free(uint8_t*p,size_t n); void hv_steel_shutdown(); }   // steel-provider/src/ffi.rs, linked in
#endif
namespace fs=std::filesystem;
namespace steel{
namespace{
// ---------------------------------------------------------------- minimal JSON (only what block.json needs)
struct JV{ char t='n'; std::string s; double n=0; std::vector<std::pair<std::string,JV>> o; std::vector<JV> a; };
struct JP{ const std::string&s; size_t i=0; bool ok=true;
 void ws(){ while(i<s.size()&&(s[i]==' '||s[i]=='\n'||s[i]=='\r'||s[i]=='\t'))i++; }
 std::string str(){ std::string r; i++; while(i<s.size()&&s[i]!='"'){ if(s[i]=='\\'&&i+1<s.size()){ i++; switch(s[i]){case 'n':r+='\n';break;case 't':r+='\t';break;case 'u':i+=4;r+='?';break;default:r+=s[i];} } else r+=s[i]; i++; } i++; return r; }
 JV val(){ JV v; ws(); if(i>=s.size()){ok=false;return v;} const char c=s[i];
  if(c=='{'){ v.t='o'; i++; ws(); if(s[i]=='}'){i++;return v;} for(;;){ ws(); if(s[i]!='"'){ok=false;return v;} std::string k=str(); ws(); if(s[i]!=':'){ok=false;return v;} i++; JV x=val(); if(!ok)return v; v.o.emplace_back(std::move(k),std::move(x)); ws(); if(s[i]==','){i++;continue;} if(s[i]=='}'){i++;break;} ok=false;return v; } }
  else if(c=='['){ v.t='a'; i++; ws(); if(s[i]==']'){i++;return v;} for(;;){ v.a.push_back(val()); if(!ok)return v; ws(); if(s[i]==','){i++;continue;} if(s[i]==']'){i++;break;} ok=false;return v; } }
  else if(c=='"'){ v.t='s'; v.s=str(); }
  else if(c=='t'||c=='f'||c=='n'){ while(i<s.size()&&std::isalpha((unsigned char)s[i]))i++; v.t='b'; }
  else { size_t j=i; while(j<s.size()&&(std::isdigit((unsigned char)s[j])||s[j]=='-'||s[j]=='+'||s[j]=='.'||s[j]=='e'||s[j]=='E'))j++; if(j==i){ok=false;return v;} v.t='d'; v.n=std::strtod(s.substr(i,j-i).c_str(),nullptr); i=j; }
  return v; } };

struct State{ std::string name,axis,facing,half,type; };
std::string g_status="not checked yet"; std::atomic<long> g_okChunks{0},g_failChunks{0};
std::vector<State> g_states; fs::path g_dir,g_exe; bool g_have=false; std::once_flag g_once;

// block.json (from the data jar):  { "minecraft:stone": { "states": { "<property string>": { "stateId": 1 } } } }
bool loadStatesText(const std::string&txt){
 JP p{txt}; JV root=p.val(); if(!p.ok||root.t!='o')return false;
 for(auto&b:root.o){ const std::string name=b.first.compare(0,10,"minecraft:")==0?b.first.substr(10):b.first; const JV*st=nullptr; for(auto&kv:b.second.o)if(kv.first=="states")st=&kv.second; if(!st)continue;
  for(auto&s:st->o){ int id=-1; for(auto&kv:s.second.o)if(kv.first=="stateId")id=int(kv.second.n); if(id<0)continue; if((size_t)id>=g_states.size())g_states.resize(size_t(id)+1);
   State&S=g_states[(size_t)id]; S.name=name; std::string thick,vdir,lay; int mk=0; const std::string&k=s.first; size_t i=0;   // property string: tolerant scan for key=value pairs
   while(i<k.size()){ while(i<k.size()&&!std::isalpha((unsigned char)k[i]))i++; size_t a=i; while(i<k.size()&&(std::isalnum((unsigned char)k[i])||k[i]=='_'))i++; const std::string key=k.substr(a,i-a); if(i>=k.size()||k[i]!='=')continue; i++; size_t b2=i; while(i<k.size()&&(std::isalnum((unsigned char)k[i])||k[i]=='_'))i++; const std::string val=k.substr(b2,i-b2);
    if(key=="axis")S.axis=val; else if(key=="facing")S.facing=val; else if(key=="half")S.half=val; else if(key=="type")S.type=val; else if(key=="open"&&val=="true")S.type+="open"; else if(key=="hinge"&&val=="right")S.type+="+right"; else if(key=="thickness")thick=val; else if(key=="vertical_direction")vdir=val; else if(key=="layers")lay="L"+val; else if(key=="age")lay="A"+val; else if(key=="part")lay="P"+val; else if(key=="eye")lay="E"+val; else if(val=="true"){ static const char*f6[6]={"north","east","south","west","up","down"}; for(int fi=0;fi<6;fi++)if(key==f6[fi])mk|=1<<fi; } }
   if(!thick.empty())S.type=thick+(vdir.empty()?"":"|"+vdir); else if(mk&&S.type.empty())S.type="mf"+std::to_string(mk); else if(!lay.empty()&&S.type.empty())S.type=lay; } }
 return !g_states.empty(); }
bool loadStates(const fs::path&f){ std::ifstream in(f,std::ios::binary); if(!in)return false; std::stringstream ss; ss<<in.rdbuf(); return loadStatesText(ss.str()); }

void locate(){
 std::error_code ec; g_exe=fs::current_path(ec);
#ifdef HV_STEEL_STATIC   // generator is linked in: only the block-state table is needed (file next to the game, else the copy built into the executable)
 for(const char*d:{"steel","../steel","../../steel",".","data/steel"}){ const fs::path bj=fs::path(d)/"block.json"; if(fs::is_regular_file(bj,ec)&&loadStates(bj)){ g_have=true; g_status="ok (built in)"; return; } }
 { const EmbeddedBlob b=embeddedBlob("block.json"); if(b.p&&b.n&&loadStatesText(std::string((const char*)b.p,b.n))){ g_have=true; g_status="ok (built in)"; return; } }
 g_status="block.json not found and not built in - run build-steel.sh --lib"; return;
#endif
 for(const char*d:{"steel","../steel","../../steel",".","data/steel"}){ const fs::path dir=fs::path(d); const fs::path pr=dir/"steel-provider",bj=dir/"block.json"; if(fs::is_regular_file(pr,ec)&&fs::is_regular_file(bj,ec)){ if(loadStates(bj)){ g_dir=dir; g_have=true; g_status="ok"; return; } else { g_status="steel/block.json could not be read"; std::fprintf(stderr,"[hypervox][steel] could not read %s\n",bj.string().c_str()); } } }
}

// ---------------------------------------------------------------- helper process
#ifndef HV_STEEL_STATIC
pid_t g_pid=-1; int g_port=0; std::once_flag g_spawnOnce; bool g_spawnOk=false;
void spawn(){
 g_port=41000+int(::getpid()%2000); const std::string addr="127.0.0.1:"+std::to_string(g_port), exe=(g_dir/"steel-provider").string();
 g_pid=::fork(); if(g_pid==0){ ::execl(exe.c_str(),exe.c_str(),addr.c_str(),(char*)nullptr); ::_exit(127); }
 g_spawnOk=g_pid>0; if(g_spawnOk)std::fprintf(stderr,"[hypervox][steel] started helper %s on %s\n",exe.c_str(),addr.c_str());
}
int connectTo(){
 for(int t=0;t<300;t++){   // the provider needs a moment to initialise (up to ~60 s)
  int status=0; if(g_pid>0&&::waitpid(g_pid,&status,WNOHANG)==g_pid){ g_pid=-1; std::fprintf(stderr,"[hypervox][steel] helper exited early\n"); return -1; }
  const int fd=::socket(AF_INET,SOCK_STREAM,0); if(fd<0)return -1; sockaddr_in a{}; a.sin_family=AF_INET; a.sin_port=htons((uint16_t)g_port); a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
  if(::connect(fd,(sockaddr*)&a,sizeof a)==0){ int one=1; ::setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,&one,sizeof one); return fd; }
  ::close(fd); std::this_thread::sleep_for(std::chrono::milliseconds(200)); }
 return -1; }
bool sendAll(int fd,const void*p,size_t n){ const char*c=(const char*)p; while(n){ const ssize_t k=::send(fd,c,n,MSG_NOSIGNAL); if(k<=0)return false; c+=k; n-=(size_t)k; } return true; }
bool recvAll(int fd,void*p,size_t n){ char*c=(char*)p; while(n){ const ssize_t k=::recv(fd,c,n,0); if(k<=0)return false; c+=k; n-=(size_t)k; } return true; }

#endif
// ---------------------------------------------------------------- Minecraft network chunk sections
struct Rd{ const uint8_t*p; size_t n,i=0; bool ok=true;
 uint8_t u8(){ if(i>=n){ok=false;return 0;} return p[i++]; }
 uint32_t var(){ uint32_t v=0; int sh=0; for(;;){ const uint8_t b=u8(); if(!ok)return 0; v|=uint32_t(b&0x7F)<<sh; if(!(b&0x80))break; sh+=7; if(sh>35){ok=false;return 0;} } return v; }
 uint64_t u64(){ if(i+8>n){ok=false;return 0;} uint64_t v=0; for(int k=0;k<8;k++)v=(v<<8)|p[i++]; return v; } };
// one paletted container; out gets `entries` values (palette-resolved; direct values are global ids)
thread_local std::string t_why;
bool fail(const char*m){ t_why=m; return false; }
bool container(Rd&r,int entries,int maxIndirect,bool lenPrefix,std::vector<uint32_t>*out){
 const int bits=r.u8(); if(!r.ok||bits>32)return fail("bad bits-per-entry"); std::vector<uint32_t> pal; uint32_t single=0;
 if(bits==0){ single=r.var(); if(lenPrefix){ if(r.var()!=0)return fail("single-value container has a data array"); } if(out)out->assign((size_t)entries,single); return r.ok; }
 const bool direct=bits>maxIndirect; if(!direct){ const uint32_t pn=r.var(); if(!r.ok||pn>(uint32_t)entries)return fail("palette longer than entries"); pal.resize(pn); for(auto&v:pal)v=r.var(); }
 const int per=64/bits; const uint32_t longs=(uint32_t)((entries+per-1)/per); if(lenPrefix){ if(r.var()!=longs)return fail("data array length mismatch"); }
 if(r.i+(size_t)longs*8>r.n)return fail("data array runs past the end"); const uint64_t mask=bits>=64?~0ull:((1ull<<bits)-1); if(out)out->resize((size_t)entries);
 for(uint32_t li=0,e=0;li<longs;li++){ const uint64_t L=r.u64(); for(int k=0;k<per&&e<(uint32_t)entries;k++,e++){ uint32_t v=uint32_t((L>>(k*bits))&mask); if(!direct){ if(v>=pal.size())return fail("palette index out of range"); v=pal[v]; } if(out)(*out)[e]=v; } }
 return r.ok; }
bool decode(const uint8_t*d,size_t n,bool lenPrefix,Chunk&c,int hdr=2,int secBase=0){   // secBase: first section index (nether / end start at Y=0 = our section 4)   // hdr: bytes before each section's block data (block count; 26.x adds a fluid count -> 4)
 Rd r{d,n}; std::vector<uint32_t> blocks; int sec=0; static thread_local std::vector<int32_t> map; if(map.size()!=g_states.size())map.assign(g_states.size(),-2);
 std::vector<std::pair<int,uint16_t>> staged;   // decoded into a scratch list first so a wrong guess of the format never leaves half a chunk behind
 std::vector<uint16_t> grid; grid.reserve(24*4096);
 while(r.i<n&&sec<64){ for(int h=0;h<hdr;h++)r.u8();   // block count (short) [+ fluid count (short)]
  if(!container(r,4096,8,lenPrefix,&blocks)||blocks.size()!=4096)return false; if(!container(r,64,3,lenPrefix,nullptr))return false;
  for(uint32_t id:blocks){ if(id>=g_states.size()){ char b[96]; std::snprintf(b,sizeof b,"block state id %u is not in block.json (%zu states) - wrong data version?",id,g_states.size()); t_why=b; return false; } int32_t&m=map[id]; if(m==-2){ const State&s=g_states[id]; m=s.name.empty()?0:int32_t(mapMcBlock(s.name,s.axis,s.facing,s.half,s.type)); } grid.push_back((uint16_t)m); }
  sec++; }
 if(r.i!=n||sec<8){ char b[96]; std::snprintf(b,sizeof b,"%d sections parsed, %zu of %zu bytes used",sec,r.i,n); t_why=b; return false; }
 for(int s=0;s<sec;s++){ const int y0=(s+secBase)*16; if(y0>=Chunk::HEIGHT)break; for(int k=0;k<4096;k++){ const uint16_t id=grid[(size_t)s*4096+k]; if(!id)continue; const int y=y0+(k>>8),z=(k>>4)&15,x=k&15; c.at(x,y,z)=id; if(y>c.maxY)c.maxY=y; } }
 return true; }
void reportUnknownBlocks(){   // once: which block names of the Steel data version hypervox does not have (they become air or the MISSING block)
 static std::once_flag f; std::call_once(f,[]{ const Registry&R=Registry::get(); const BlockId miss=R.blockByName("missing"); std::vector<std::string> bad; std::string last;
  for(const State&st:g_states){ if(st.name.empty()||st.name==last)continue; last=st.name; if(st.name=="air"||st.name=="cave_air"||st.name=="void_air")continue; const uint16_t id=mapMcBlock(st.name,"","","","");
   if(id==0||(miss&&id==miss))bad.push_back(st.name); }
  std::sort(bad.begin(),bad.end()); bad.erase(std::unique(bad.begin(),bad.end()),bad.end());
  std::fprintf(stderr,"[hypervox][steel] %zu of the data version's block types are not in hypervox (shown as air / MISSING)%s\n",bad.size(),bad.empty()?"":":");
  std::string line; for(size_t i=0;i<bad.size();i++){ line+=bad[i]+" "; if(line.size()>110||i+1==bad.size()){ std::fprintf(stderr,"[hypervox][steel]   %s\n",line.c_str()); line.clear(); } } }); }
}   // namespace

bool available(){ std::call_once(g_once,locate); if(!g_have&&g_status=="not checked yet")g_status="steel/steel-provider or steel/block.json not found - run build-steel.sh (or build with -DHV_STEEL_STATIC=ON)"; return g_have; }
const char* status(){ available(); static thread_local std::string s; s=g_status; if(g_have){ s+=" | chunks from Steel: "+std::to_string(g_okChunks.load())+", failed: "+std::to_string(g_failChunks.load()); } return s.c_str(); }
bool generate(uint64_t seed,int cx,int cz,int dim,Chunk&out){
#ifdef HV_STEEL_STATIC
 if(!available())return false; static std::atomic<bool> dead{false}; if(dead)return false;
 size_t n=0; uint8_t*p=hv_steel_generate(seed,(int32_t)cx,(int32_t)cz,(uint8_t)dim,&n); if(!p){ g_failChunks++; return false; }
 std::vector<uint8_t> buf(4+n,0); std::memcpy(buf.data()+4,p,n); hv_steel_free(p,n); const uint32_t len=(uint32_t)buf.size();
#else
 if(!available())return false; std::call_once(g_spawnOnce,spawn); if(!g_spawnOk)return false;
 static thread_local int fd=-1; static std::atomic<bool> dead{false}; if(dead)return false;
 if(fd<0){ fd=connectTo(); if(fd<0){ dead=true; g_status="helper started but did not answer (see console)"; std::fprintf(stderr,"[hypervox][steel] helper unreachable - using the built-in terrain\n"); return false; } }
 uint8_t rq[17]; for(int i=0;i<8;i++)rq[i]=uint8_t(seed>>(56-8*i)); const uint32_t ux=(uint32_t)cx,uz=(uint32_t)cz; for(int i=0;i<4;i++){ rq[8+i]=uint8_t(ux>>(24-8*i)); rq[12+i]=uint8_t(uz>>(24-8*i)); } rq[16]=(uint8_t)dim;
 uint8_t hdr[4]; if(!sendAll(fd,rq,17)||!recvAll(fd,hdr,4)){ ::close(fd); fd=-1; return false; }
 const uint32_t len=(uint32_t(hdr[0])<<24)|(uint32_t(hdr[1])<<16)|(uint32_t(hdr[2])<<8)|hdr[3]; if(len<4||len>64u*1024*1024){ ::close(fd); fd=-1; return false; }
 std::vector<uint8_t> buf(len); if(!recvAll(fd,buf.data(),len)){ ::close(fd); fd=-1; return false; }
 const uint32_t status=(uint32_t(buf[0])<<24)|(uint32_t(buf[1])<<16)|(uint32_t(buf[2])<<8)|buf[3]; if(status!=0){ static std::once_flag f; std::call_once(f,[&]{ std::fprintf(stderr,"[hypervox][steel] provider error: %.*s\n",int(len-4),(const char*)buf.data()+4); }); return false; }
#endif
 Chunk tmp; static std::atomic<int> fmt{-1};   // which section layout the provider uses (bit 0: long-array length prefix, bit 1: extra fluid-count short) is found out on the first chunk
 const int first=fmt.load(); std::string reasons; for(int tryIdx=0;tryIdx<4;tryIdx++){ const int v=first>=0?first:tryIdx; tmp=Chunk(); t_why.clear(); if(decode(buf.data()+4,len-4,(v&1)!=0,tmp,(v&2)?4:2,dim?4:0)){ fmt=v; out=std::move(tmp); g_okChunks++; reportUnknownBlocks(); return true; } reasons+=std::string("[")+((v&1)?"length prefix":"no length prefix")+((v&2)?", fluid count":"")+": "+t_why+"] "; if(first>=0)break; }
 g_failChunks++; g_status="could not decode a chunk: "+reasons;
 static std::once_flag f2; std::call_once(f2,[&]{ std::fprintf(stderr,"[hypervox][steel] could not decode a chunk %s- flat world\n[hypervox][steel] reply: %u bytes, first 80 bytes:",reasons.c_str(),(unsigned)(len-4)); for(size_t i=0;i<80&&i<len-4;i++)std::fprintf(stderr,"%s%02x",i%16==0?"\n[hypervox][steel]   ":" ",buf[4+i]); std::fprintf(stderr,"\n"); });
 if(g_failChunks>=3&&g_okChunks==0){ dead=true; std::fprintf(stderr,"[hypervox][steel] giving up after 3 failed chunks - flat world (save time: Steel chunks cost ~70 ms each)\n"); }
 return false; }
#ifdef HV_STEEL_STATIC
void shutdown(){ hv_steel_shutdown(); }
#else
void shutdown(){ if(g_pid>0){ ::kill(g_pid,SIGTERM); int st=0; ::waitpid(g_pid,&st,0); g_pid=-1; } }
#endif
}
#endif
