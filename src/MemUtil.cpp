#include "MemUtil.hpp"
#include <fstream>
#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#define PSAPI_VERSION 2
#include <psapi.h>
int processRssMB(){ PROCESS_MEMORY_COUNTERS pmc{}; if(K32GetProcessMemoryInfo(GetCurrentProcess(),&pmc,sizeof pmc))return int(pmc.WorkingSetSize/(1024*1024)); return 0; }
void releaseFreeMemory(){ SetProcessWorkingSetSize(GetCurrentProcess(),(SIZE_T)-1,(SIZE_T)-1); }
#elif defined(__APPLE__)
#include <mach/mach.h>
int processRssMB(){ mach_task_basic_info info{}; mach_msg_type_number_t n=MACH_TASK_BASIC_INFO_COUNT; if(task_info(mach_task_self(),MACH_TASK_BASIC_INFO,(task_info_t)&info,&n)==KERN_SUCCESS)return int(info.resident_size/(1024*1024)); return 0; }
void releaseFreeMemory(){}
#else
#include <unistd.h>
#if defined(__GLIBC__)
#include <malloc.h>
#endif
int processRssMB(){ std::ifstream f("/proc/self/statm"); long a=0,b=0; if(f>>a>>b)return int(b*sysconf(_SC_PAGESIZE)/(1024*1024)); return 0; }
void releaseFreeMemory(){
#if defined(__GLIBC__)
 malloc_trim(0);
#endif
}
#endif
