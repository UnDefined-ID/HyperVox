#include "Log.hpp"
#include "Settings.hpp"
#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#else
#include <fcntl.h>
#include <unistd.h>
#if defined(__GLIBC__)
#include <execinfo.h>
#endif
#endif
namespace fs=std::filesystem;
namespace{
int g_logFd=-1,g_latestFd=-1,g_origErr=-1;
void writeAll(int fd,const char*p,size_t n){
#ifdef _WIN32
 if(fd>=0)_write(fd,p,(unsigned)n);
#else
 while(fd>=0&&n>0){ ssize_t w=::write(fd,p,n); if(w<=0)break; p+=w; n-=(size_t)w; }
#endif
}
void crashNote(const char*what){
 char buf[256]; const int n=std::snprintf(buf,sizeof buf,"\n[hypervox][CRASH] %s\n",what);
 writeAll(g_logFd,buf,(size_t)n); writeAll(g_latestFd,buf,(size_t)n); writeAll(g_origErr,buf,(size_t)n);
#if !defined(_WIN32)&&defined(__GLIBC__)
 void*bt[48]; const int cnt=backtrace(bt,48); backtrace_symbols_fd(bt,cnt,g_logFd); backtrace_symbols_fd(bt,cnt,g_latestFd); backtrace_symbols_fd(bt,cnt,g_origErr);
#endif
}
void onSignal(int sig){
 char what[64]; std::snprintf(what,sizeof what,"fatal signal %d (%s)",sig,sig==SIGSEGV?"segmentation fault":sig==SIGABRT?"abort":sig==SIGFPE?"arithmetic error":sig==SIGILL?"illegal instruction":"signal");
 crashNote(what); std::signal(sig,SIG_DFL); std::raise(sig);
}
void onTerminate(){
 std::string w="uncaught exception";
 try{ if(auto e=std::current_exception())std::rethrow_exception(e); }catch(const std::exception&ex){ w+=": "; w+=ex.what(); }catch(...){}
 crashNote(w.c_str()); std::abort();
}
}
void logInit(){
 std::error_code ec; const fs::path dir=Settings::dataDir()/"logs"; fs::create_directories(dir,ec);
 // keep at most 10 logs: delete the oldest ones (names sort by date) so that the new file makes 10
 std::vector<fs::path> old; for(auto&e:fs::directory_iterator(dir,ec)){ const std::string n=e.path().filename().string(); if(n.rfind("hypervox-",0)==0&&e.path().extension()==".log")old.push_back(e.path()); }
 std::sort(old.begin(),old.end()); while(old.size()>9){ fs::remove(old.front(),ec); old.erase(old.begin()); }
 const std::time_t t=std::time(nullptr); char stamp[40]; std::strftime(stamp,sizeof stamp,"%Y-%m-%d_%H-%M-%S",std::localtime(&t));
 const fs::path file=dir/(std::string("hypervox-")+stamp+".log"),latest=dir/"latest.log";
#ifdef _WIN32
 g_logFd=_open(file.string().c_str(),_O_WRONLY|_O_CREAT|_O_TRUNC|_O_BINARY,0644); g_latestFd=_open(latest.string().c_str(),_O_WRONLY|_O_CREAT|_O_TRUNC|_O_BINARY,0644);
 if(g_logFd>=0){ _dup2(g_logFd,1); _dup2(g_logFd,2); }
#else
 g_logFd=::open(file.c_str(),O_WRONLY|O_CREAT|O_TRUNC,0644); g_latestFd=::open(latest.c_str(),O_WRONLY|O_CREAT|O_TRUNC,0644);
 if(g_logFd>=0){
  const int out=::dup(1); g_origErr=::dup(2); int p[2];
  if(out>=0&&g_origErr>=0&&::pipe(p)==0){
   ::dup2(p[1],1); ::dup2(p[1],2); ::close(p[1]); std::setvbuf(stdout,nullptr,_IOLBF,0);
   const int rd=p[0],lf=g_logFd,la=g_latestFd;
   std::thread([rd,out,lf,la]{ char buf[4096]; for(;;){ const ssize_t n=::read(rd,buf,sizeof buf); if(n<=0)break; writeAll(out,buf,(size_t)n); writeAll(lf,buf,(size_t)n); writeAll(la,buf,(size_t)n); } }).detach();   // tee: terminal + both log files
  }
 }
#endif
 std::signal(SIGSEGV,onSignal); std::signal(SIGABRT,onSignal); std::signal(SIGFPE,onSignal); std::signal(SIGILL,onSignal);
#ifdef SIGBUS
 std::signal(SIGBUS,onSignal);
#endif
 std::set_terminate(onTerminate);
 std::cout<<"[hypervox] log file: "<<file.string()<<"  (the 10 newest logs are kept)\n";
}
