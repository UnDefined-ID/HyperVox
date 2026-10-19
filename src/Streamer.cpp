#include "Streamer.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>

static double nowSec(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}

ChunkStreamer::ChunkStreamer(World&w,ChunkSink&s):m_world(w),m_sink(s){
 unsigned hc=std::thread::hardware_concurrency(); unsigned n=hc>2?hc-1:1; n=std::min(n,8u);
 m_cap=std::max<size_t>(96,size_t(n)*24);
 for(unsigned i=0;i<n;i++)m_threads.emplace_back([this]{worker();});
}
ChunkStreamer::~ChunkStreamer(){
 { std::lock_guard<std::mutex> l(m_mu); m_stop=true; }
 m_cv.notify_all(); for(auto&t:m_threads)if(t.joinable())t.join();
}

void ChunkStreamer::worker(){
 for(;;){
  Job j; uint64_t epoch;
  { std::unique_lock<std::mutex> l(m_mu);
    m_cv.wait(l,[&]{return m_stop||!m_queue.empty();});
    if(m_stop)return;
    j=m_queue.front(); m_queue.pop_front(); const uint64_t k=key(j.cx,j.cz); m_queued.erase(k); m_inflight.insert(k); epoch=m_epoch; }
  Result r; r.cx=j.cx; r.cz=j.cz; r.ver=j.ver; r.epoch=epoch;
  r.mesh=World::buildMesh(m_world.snapshot(j.cx,j.cz),j.cx,j.cz);
  { std::lock_guard<std::mutex> l(m_mu); m_inflight.erase(key(j.cx,j.cz)); m_results.push_back(std::move(r)); }
  m_idleCv.notify_all();
 }
}

void ChunkStreamer::reset(){
 { std::unique_lock<std::mutex> l(m_mu);
   m_queue.clear(); m_queued.clear(); m_epoch++;
   m_idleCv.wait(l,[&]{return m_inflight.empty();});   // let running jobs finish so the world can be reset safely
   m_results.clear(); }
 m_sink.clear(); m_meshed.clear(); m_version.clear(); m_haveCenter=false; m_needScan=true; m_gpuFull=false; m_allQueued=false;
}

bool ChunkStreamer::isMeshed(int cx,int cz)const{return m_meshed.find(key(cx,cz))!=m_meshed.end();}
size_t ChunkStreamer::pendingCount(){std::lock_guard<std::mutex> l(m_mu);return m_queue.size()+m_inflight.size();}

void ChunkStreamer::markDirty(int cx,int cz){
 const uint64_t k=key(cx,cz); const uint32_t ver=++m_verCounter; m_version[k]=ver;
 std::lock_guard<std::mutex> l(m_mu);
 if(m_queued.count(k)){                     // already waiting: move it to the front (it snapshots when it starts)
  for(auto it=m_queue.begin();it!=m_queue.end();++it)if(it->cx==cx&&it->cz==cz){m_queue.erase(it);break;}
 }
 m_queue.push_front(Job{cx,cz,ver}); m_queued.insert(k);
 m_cv.notify_one();
}

void ChunkStreamer::update(int pcx,int pcz,int radius,const ViewCone&view){
 radius=std::max(1,radius);
 m_view=view;
 if(!m_haveCenter||view.dx*m_scanDx+view.dz*m_scanDz<0.9976f)m_needScan=true;          // turned more than ~4 degrees
 if(!m_haveCenter||pcx!=m_cx||pcz!=m_cz||radius!=m_r){m_cx=pcx;m_cz=pcz;m_r=radius;m_haveCenter=true;m_needScan=true;}
 const double t=nowSec();
 if(m_needScan||(!m_allQueued&&!m_gpuFull&&t-m_lastScan>0.2)){ m_lastScan=t; scan(); }
}

void ChunkStreamer::scan(){
 const int R=m_r; m_scanDx=m_view.dx; m_scanDz=m_view.dz;
 if(m_offR!=R){
  m_offR=R; m_offsets.clear();
  for(int dz=-R;dz<=R;dz++)for(int dx=-R;dx<=R;dx++)if(chunkInRange(dx,dz,R))m_offsets.emplace_back(dx,dz);
  std::sort(m_offsets.begin(),m_offsets.end(),[](const std::pair<int,int>&a,const std::pair<int,int>&b){return a.first*a.first+a.second*a.second<b.first*b.first+b.second*b.second;});
 }
 // 1) unload meshes that are out of range (+2 hysteresis)
 bool freed=false;
 if(m_needScan){
  for(auto it=m_meshed.begin();it!=m_meshed.end();){
   const int cx=(int)(int32_t)(it->first>>32),cz=(int)(int32_t)(it->first&0xffffffffu);
   if(!chunkInRange(cx-m_cx,cz-m_cz,R+2)||!wanted(cx,cz,true)){m_sink.release(cx,cz);m_version.erase(it->first);it=m_meshed.erase(it);freed=true;} else ++it;
  }
  m_world.pruneCache(m_cx,m_cz,std::clamp(R+3,4,m_cacheR));
  if(freed)m_gpuFull=false;
 }
 // 2) queue missing chunks closest-first
 std::vector<Job> jobs; jobs.reserve(m_cap);
 { std::lock_guard<std::mutex> l(m_mu);
   const size_t room=m_cap>m_inflight.size()?m_cap-m_inflight.size():0;
   if(!m_gpuFull)
   for(auto&o:m_offsets){
    if(jobs.size()>=room)break;
    const int cx=m_cx+o.first,cz=m_cz+o.second; const uint64_t k=key(cx,cz);
    if(m_paused||m_meshed.count(k)||m_inflight.count(k)||!wanted(cx,cz,false))continue;
    jobs.push_back(Job{cx,cz,0});
   }
   // replace the not-yet-started part of the queue with the new priority order (keep dirty re-meshes first)
   std::deque<Job> keep; for(auto&j:m_queue)if(m_meshed.count(key(j.cx,j.cz)))keep.push_back(j);
   m_queue=std::move(keep); m_queued.clear(); for(auto&j:m_queue)m_queued.insert(key(j.cx,j.cz));
   for(auto&j:jobs){ const uint32_t ver=++m_verCounter; m_version[key(j.cx,j.cz)]=ver; m_queue.push_back(Job{j.cx,j.cz,ver}); m_queued.insert(key(j.cx,j.cz)); }
   m_allQueued=jobs.empty()&&!m_gpuFull&&m_queue.empty()&&m_inflight.empty();
 }
 m_needScan=false;
 m_cv.notify_all();
}

void ChunkStreamer::pump(int maxUploads){
 std::vector<Result> batch;
 { std::lock_guard<std::mutex> l(m_mu); if(m_results.empty())return; batch.swap(m_results); }
 int done=0; std::vector<Result> later;
 for(auto&r:batch){
  const uint64_t k=key(r.cx,r.cz);
  if(r.epoch!=m_epoch)continue;
  auto vit=m_version.find(k);
  if(vit!=m_version.end()&&r.ver<vit->second)continue;          // a newer job for this chunk exists
  if(!chunkInRange(r.cx-m_cx,r.cz-m_cz,m_r+2)||!wanted(r.cx,r.cz,true))continue;   // moved away / turned away meanwhile
  if(done>=maxUploads){later.push_back(std::move(r));continue;}
  if(m_sink.upload(r.cx,r.cz,r.mesh)){ m_meshed[k]=r.ver; done++; }
  else { m_gpuFull=true; }                                        // GPU arena full: stop requesting more until something is freed
 }
 if(!later.empty()){ std::lock_guard<std::mutex> l(m_mu); for(auto&r:later)m_results.push_back(std::move(r)); }
}

void ChunkStreamer::trim(){
 for(auto it=m_meshed.begin();it!=m_meshed.end();){
  const int cx=(int)(int32_t)(it->first>>32),cz=(int)(int32_t)(it->first&0xFFFFFFFFu);
  if(!chunkInRange(cx-m_cx,cz-m_cz,m_r)||!wanted(cx,cz,false)){m_sink.release(cx,cz);m_version.erase(it->first);it=m_meshed.erase(it);m_gpuFull=false;} else ++it;
 }
 m_world.pruneCache(m_cx,m_cz,std::clamp(m_cacheR,4,8)); m_needScan=true;
}
