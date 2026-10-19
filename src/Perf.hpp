#pragma once
// Freeze finder: PerfScope prints "[hypervox][SLOW] <what> took X ms" to the terminal when a section runs longer than its limit.
#include <chrono>
#include <cstdio>
struct PerfScope{
 const char*name; double limitMs; std::chrono::steady_clock::time_point t0;
 PerfScope(const char*n,double lim):name(n),limitMs(lim),t0(std::chrono::steady_clock::now()){}
 ~PerfScope(){ const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t0).count(); if(ms>limitMs)std::fprintf(stderr,"[hypervox][SLOW] %s took %.1f ms (limit %.0f ms)\n",name,ms,limitMs); }
};
