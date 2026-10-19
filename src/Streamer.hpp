#pragma once
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "World.hpp"

// Receives finished chunk meshes (implemented by the Vulkan renderer).
struct ChunkSink{
 virtual ~ChunkSink()=default;
 virtual bool upload(int cx,int cz,const ChunkMesh&m)=0;   // false = out of GPU memory
 virtual void release(int cx,int cz)=0;
 virtual void clear()=0;
};

inline bool chunkInRange(int dx,int dz,int r){return dx*dx+dz*dz<=r*r+r;}

// Horizontal view cone used to keep only the chunks you can actually see on the GPU.
struct ViewCone{
 float px=0,pz=0,dx=0,dz=1,halfL=4.f,halfU=4.f;                    // halfL/halfU: half angle (rad) to load / to keep; >= pi means "everything"
 int nearChunks=3;                                                // chunks this close are always resident (all directions)
 static ViewCone make(float px,float pz,float yaw,float halfAngle,float loadMargin,float unloadMargin){
  ViewCone v; v.px=px; v.pz=pz; v.dx=std::sin(yaw); v.dz=-std::cos(yaw); v.halfL=halfAngle+loadMargin; v.halfU=halfAngle+unloadMargin; return v;
 }
 bool sees(int cx,int cz,bool unload)const{
  const float A=unload?halfU:halfL; if(A>=3.1415f)return true;
  const float vx=float(cx*16+8)-px,vz=float(cz*16+8)-pz,len2=vx*vx+vz*vz;
  if(len2<float(nearChunks*16)*float(nearChunks*16))return true;
  const float len=std::sqrt(len2),d=(vx*dx+vz*dz)/len;
  const float a=A+std::asin(std::min(1.f,11.5f/len));                // a chunk is ~11.3 blocks wide diagonally
  return a>=3.1415f||d>=std::cos(a);
 }
};

// Streams chunk meshes around the player: closest-first generation + greedy meshing on worker threads,
// uploads on the calling (main) thread.  There is no upper limit on the radius except GPU memory.
class ChunkStreamer{
public:
 ChunkStreamer(World&w,ChunkSink&s);
 ~ChunkStreamer();
 void reset();                                   // forget everything (call before switching worlds)
 void setPaused(bool p){m_paused=p;}                        // memory pressure: stop queueing new chunks
 void setCacheRadius(int r){m_cacheR=r;m_needScan=true;}   // CPU chunk cache radius (chunks)
 void trim();                                               // drop everything outside the exact view cone + shrink the CPU cache now
 void update(int pcx,int pcz,int radius,const ViewCone&view);   // call every frame
 void pump(int maxUploads);                      // upload finished meshes
 void markDirty(int cx,int cz);                  // block changed: re-mesh with top priority
 bool isMeshed(int cx,int cz)const;
 size_t meshedCount()const{return m_meshed.size();}
 size_t pendingCount();
 bool gpuFull()const{return m_gpuFull;}
private:
 struct Job{int cx,cz;uint32_t ver;};
 struct Result{int cx,cz;uint32_t ver;uint64_t epoch;ChunkMesh mesh;};
 static uint64_t key(int cx,int cz){return (static_cast<uint64_t>(static_cast<uint32_t>(cx))<<32)|static_cast<uint32_t>(cz);}
 void worker();
 void scan();
 World& m_world; ChunkSink& m_sink;
 std::vector<std::thread> m_threads; bool m_stop=false;
 std::mutex m_mu; std::condition_variable m_cv,m_idleCv;
 std::deque<Job> m_queue; std::unordered_set<uint64_t> m_queued,m_inflight; std::vector<Result> m_results; uint64_t m_epoch=0;
 // main-thread state
 std::unordered_map<uint64_t,uint32_t> m_meshed; std::unordered_map<uint64_t,uint32_t> m_version;
 std::vector<std::pair<int,int>> m_offsets; int m_offR=-1;
 bool m_paused=false; int m_cacheR=20; ViewCone m_view; float m_scanDx=0,m_scanDz=1; bool wanted(int cx,int cz,bool unload)const{return m_view.sees(cx,cz,unload);}
 int m_cx=0,m_cz=0,m_r=0; bool m_haveCenter=false,m_needScan=true,m_gpuFull=false,m_allQueued=false; double m_lastScan=0; size_t m_cap=64; uint32_t m_verCounter=0;
};
