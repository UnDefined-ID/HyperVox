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

// Receives finished chunk meshes (implemented by the Vulkan / OpenGL renderer).  (cx,cz) is just the two halves of a 64-bit mesh key, see keyOf() in Streamer.cpp.
struct ChunkSink{
 virtual ~ChunkSink()=default;
 virtual bool upload(int cx,int cz,const ChunkMesh&m)=0;   // false = out of GPU memory
 virtual void release(int cx,int cz)=0;
 virtual void clear()=0;
 virtual bool uploadBusy()const{return false;}
 virtual float fillRatio()const{return 0.f;}
};

inline bool chunkInRange(int dx,int dz,int r){return dx*dx+dz*dz<=r*r+r;}

// (kept for Game.cpp; the tiered streamer no longer needs a view cone)
struct ViewCone{
 float px=0,pz=0,dx=0,dz=1,halfL=4.f,halfU=4.f; int nearChunks=3;
 static ViewCone make(float px,float pz,float yaw,float halfAngle,float loadMargin,float unloadMargin){
  ViewCone v; v.px=px; v.pz=pz; v.dx=std::sin(yaw); v.dz=-std::cos(yaw); v.halfL=halfAngle+loadMargin; v.halfU=halfAngle+unloadMargin; return v;
 }
 bool sees(int,int,bool)const{return true;}
};

// One drawable mesh.  tier 0 = 16x16x16 section of the player's chunk, 1 = one chunk, 2 = 3x3 chunks, 3 = 9x9 chunks (x,z in units of that tier's size).
struct MeshCell{ int tier=1,x=0,z=0,sec=0; };

// Tiered streaming: the player's chunk is meshed per section, the chunks around it one mesh each, then rings of 3x3-chunk and 9x9-chunk meshes (same idea as a
// clipmap).  Cells sit on a fixed grid, so moving only builds the cells that appear; edits rebuild one section (or one chunk) and flag the big cells above it.
class ChunkStreamer{
public:
 ChunkStreamer(World&w,ChunkSink&s);
 ~ChunkStreamer();
 void reset();
 void setPaused(bool p){m_paused=p;}
 void setCacheRadius(int r){m_cacheR=r;m_needScan=true;}
 void trim();
 void update(int pcx,int pcz,int radius,const ViewCone&view);
 void pump(int maxUploads);
 void markDirty(int cx,int cz,int y=-1);        // block changed (y < 0: unknown, whole chunk)
 bool isMeshed(int cx,int cz)const;
 size_t meshedCount()const{return m_meshed.size();}
 size_t pendingCount();
 bool gpuFull()const{return m_gpuFull;}
private:
 struct Job{MeshCell c;uint32_t ver;};
 struct Result{MeshCell c;uint32_t ver;uint64_t epoch;ChunkMesh mesh;};
 struct Meshed{MeshCell c;uint32_t ver=0;};
 void worker(); void scan(); void releaseCovered(); void rebuildNow(const MeshCell&c); void queueRebuild(const MeshCell&c);
 ChunkMesh buildCell(const MeshCell&c)const;
 World& m_world; ChunkSink& m_sink;
 std::vector<std::thread> m_threads; bool m_stop=false;
 std::mutex m_mu; std::condition_variable m_cv,m_idleCv;
 std::deque<Job> m_queue; std::unordered_set<uint64_t> m_queued,m_inflight; std::vector<Result> m_results; uint64_t m_epoch=0;
 // main thread only
 std::unordered_map<uint64_t,Meshed> m_meshed; std::unordered_map<uint64_t,uint32_t> m_version;
 std::vector<MeshCell> m_want; std::unordered_set<uint64_t> m_wantSet,m_split;
 std::unordered_map<uint64_t,uint32_t> m_editMap; std::vector<uint64_t> m_editOrder;
 bool m_paused=false; int m_cacheR=16; int m_cx=0,m_cz=0,m_r=0,m_playerSecs=0; bool m_haveCenter=false,m_needScan=true,m_gpuFull=false,m_allQueued=false; double m_lastScan=0; size_t m_cap=64; uint32_t m_verCounter=0;
};
