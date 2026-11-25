#include "Streamer.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>

namespace{
double nowSec(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
int fdiv(int a,int b){return a>=0?a/b:-((-a+b-1)/b);}
int pow3(int n){int r=1;while(n-->0)r*=3;return r;}
int sizeOf(int tier){return tier==2?3:(tier==3?9:1);}
// Mesh key = two ints (the renderers pack them like before).  Plain chunks keep the old (cx,cz) key; sections / 3x3 / 9x9 meshes put a kind number (1..3) in the top 10 bits of x.
uint64_t keyOf(const MeshCell&c){
 if(c.tier==1)return (uint64_t(uint32_t(c.x))<<32)|uint32_t(c.z);
 const int kind=c.tier==0?1:c.tier;
 return (uint64_t(uint32_t((c.x&0x3FFFFF)|(kind<<22)))<<32)|uint32_t((c.z&0x3FFFFF)|((c.tier==0?c.sec:0)<<22));
}
void splitKey(uint64_t k,int&a,int&b){a=(int)(int32_t)(uint32_t)(k>>32);b=(int)(int32_t)(uint32_t)k;}
uint64_t chunkKey(int cx,int cz){return (uint64_t(uint32_t(cx))<<32)|uint32_t(cz);}
bool covers(const MeshCell&c,int cx,int cz){const int s=sizeOf(c.tier),x0=c.x*s,z0=c.z*s;return cx>=x0&&cx<x0+s&&cz>=z0&&cz<z0+s;}
bool overlaps(const MeshCell&a,const MeshCell&b){const int sa=sizeOf(a.tier),sb=sizeOf(b.tier);return a.x*sa<b.x*sb+sb&&b.x*sb<a.x*sa+sa&&a.z*sa<b.z*sb+sb&&b.z*sb<a.z*sa+sa;}

// Copies a chunk mesh into a bigger one: positions are re-quantised from 1/16 block to scale/16 block (scale 2 = 1/8 block, 8 = 1/2 block), uv / tile / face stay.
// Quads that collapse to a line or point are dropped; for the far 9x9 meshes plants and other free quads (bucket 0) are dropped as well.
void mergeInto(std::vector<Vertex>*dst,const ChunkMesh&m,int offX,int offZ,int scale,bool dropFree){
 const int h=scale/2; uint32_t s=0;
 for(int bk=0;bk<kBuckets;bk++){
  const uint32_t e=std::min<uint32_t>(m.bucketEnd[bk],(uint32_t)m.vertices.size());
  if(!(dropFree&&bk==0)){
   for(uint32_t q=s;q+4<=e;q+=4){
    int px[4],py[4],pz[4]; bool ok=true;
    for(int k=0;k<4;k++){ const Vertex&o=m.vertices[q+k]; const int x=o.a&511,z=(o.a>>9)&511,y=((o.a>>18)&4095)|(int)((o.b>>31)<<12);
     px[k]=(x+offX+h)/scale; pz[k]=(z+offZ+h)/scale; py[k]=(y+h)/scale; if(px[k]>511||pz[k]>511||py[k]>8191)ok=false; }
    if(!ok)continue;
    const int zx=(px[0]==px[1]&&px[1]==px[2]&&px[2]==px[3])+(py[0]==py[1]&&py[1]==py[2]&&py[2]==py[3])+(pz[0]==pz[1]&&pz[1]==pz[2]&&pz[2]==pz[3]);
    if(zx>=2)continue;
    for(int k=0;k<4;k++){ const Vertex&o=m.vertices[q+k]; Vertex n;
     n.a=(uint32_t)px[k]|((uint32_t)pz[k]<<9)|(((uint32_t)py[k]&4095u)<<18)|(o.a&0xC0000000u);
     n.b=(o.b&0x7FFFFFFFu)|(((uint32_t)py[k]>>12)<<31); dst[bk].push_back(n); }
   }
  }
  s=e;
 }
}
}

ChunkStreamer::ChunkStreamer(World&w,ChunkSink&s):m_world(w),m_sink(s){
 unsigned hc=std::thread::hardware_concurrency(); unsigned n=hc>2?hc-1:1; n=std::min(n,8u);
#ifdef HV_LOW_MEMORY
 n=1;
#endif
 m_cap=std::max<size_t>(32,size_t(n)*6);
 for(unsigned i=0;i<n;i++)m_threads.emplace_back([this]{worker();});
}
ChunkStreamer::~ChunkStreamer(){
 { std::lock_guard<std::mutex> l(m_mu); m_stop=true; }
 m_cv.notify_all(); for(auto&t:m_threads)if(t.joinable())t.join();
}

ChunkMesh ChunkStreamer::buildCell(const MeshCell&c)const{
 if(c.tier==0)return World::buildMesh(m_world.snapshot(c.x,c.z),c.x,c.z,c.sec*16,c.sec*16+16);
 if(c.tier==1)return World::buildMesh(m_world.snapshot(c.x,c.z),c.x,c.z);
 const int cs=sizeOf(c.tier),scale=c.tier==2?2:8; ChunkMesh out; out.scale=(float)scale; out.size=cs*16; out.ox=c.x*cs*16; out.oz=c.z*cs*16;
 std::vector<Vertex> b[kBuckets];
 for(int dz=0;dz<cs;dz++)for(int dx=0;dx<cs;dx++){
  const int cx=c.x*cs+dx,cz=c.z*cs+dz; const ChunkMesh m=World::buildMesh(m_world.snapshot(cx,cz),cx,cz);
  out.maxY=std::max(out.maxY,m.maxY); mergeInto(b,m,dx*256,dz*256,scale,c.tier>=3);
 }
 size_t tot=0; for(auto&v:b)tot+=v.size(); out.vertices.reserve(tot);
 for(int i=0;i<kBuckets;i++){ out.vertices.insert(out.vertices.end(),b[i].begin(),b[i].end()); out.bucketEnd[i]=(uint32_t)out.vertices.size(); }
 return out;
}

void ChunkStreamer::worker(){
 for(;;){
  Job j; uint64_t epoch;
  { std::unique_lock<std::mutex> l(m_mu);
    m_cv.wait(l,[&]{return m_stop||!m_queue.empty();});
    if(m_stop)return;
    j=m_queue.front(); m_queue.pop_front(); const uint64_t k=keyOf(j.c); m_queued.erase(k); m_inflight.insert(k); epoch=m_epoch; }
  Result r; r.c=j.c; r.ver=j.ver; r.epoch=epoch; r.mesh=buildCell(j.c);
  { std::lock_guard<std::mutex> l(m_mu); m_inflight.erase(keyOf(j.c)); m_results.push_back(std::move(r)); }
  m_idleCv.notify_all();
 }
}

void ChunkStreamer::reset(){
 { std::unique_lock<std::mutex> l(m_mu);
   m_queue.clear(); m_queued.clear(); m_epoch++;
   m_idleCv.wait(l,[&]{return m_inflight.empty();});
   m_results.clear(); }
 m_sink.clear(); m_meshed.clear(); m_version.clear(); m_want.clear(); m_wantSet.clear(); m_split.clear(); m_editMap.clear(); m_editOrder.clear();
 m_haveCenter=false; m_needScan=true; m_gpuFull=false; m_allQueued=false; m_playerSecs=0;
}

bool ChunkStreamer::isMeshed(int cx,int cz)const{
 for(auto&kv:m_meshed)if(kv.second.c.tier>=1&&covers(kv.second.c,cx,cz))return true;
 if(m_haveCenter&&cx==m_cx&&cz==m_cz){ if(m_want.empty())return false; for(const MeshCell&d:m_want)if(d.tier==0&&!m_meshed.count(keyOf(d)))return false; return true; }
 return false;
}
size_t ChunkStreamer::pendingCount(){std::lock_guard<std::mutex> l(m_mu);return m_queue.size()+m_inflight.size();}

void ChunkStreamer::queueRebuild(const MeshCell&c){
 const uint64_t k=keyOf(c); std::lock_guard<std::mutex> l(m_mu);
 if(m_queued.count(k))return;                       // a job that has not started yet reads the newest blocks anyway
 const uint32_t ver=++m_verCounter; m_version[k]=ver;
 m_queue.push_front(Job{c,ver}); m_queued.insert(k); m_cv.notify_one();
}
void ChunkStreamer::rebuildNow(const MeshCell&c){
 const uint64_t k=keyOf(c); const uint32_t ver=++m_verCounter; m_version[k]=ver; int a,b; splitKey(k,a,b);
 ChunkMesh m=buildCell(c);
 if(m_sink.upload(a,b,m)){ Meshed&mm=m_meshed[k]; mm.c=c; mm.ver=ver; } else queueRebuild(c);
}

void ChunkStreamer::markDirty(int cx,int cz,int y){
 uint32_t bit;
 if(y<0)bit=1u<<31;
 else { const int s=std::clamp(y>>4,0,23); bit=1u<<s; if((y&15)==0&&s>0)bit|=1u<<(s-1); if((y&15)==15&&s<23)bit|=1u<<(s+1); if(cx==m_cx&&cz==m_cz&&s>=m_playerSecs)m_needScan=true; }
 const uint64_t k=chunkKey(cx,cz); auto it=m_editMap.find(k);
 if(it==m_editMap.end()){ m_editMap[k]=bit; m_editOrder.push_back(k); } else it->second|=bit;
}

void ChunkStreamer::update(int pcx,int pcz,int radius,const ViewCone&){
 radius=std::max(1,radius);
 if(!m_haveCenter||pcx!=m_cx||pcz!=m_cz||radius!=m_r){m_cx=pcx;m_cz=pcz;m_r=radius;m_haveCenter=true;m_needScan=true;}
 const double t=nowSec();
 if(m_needScan||(!m_allQueued&&!m_gpuFull&&t-m_lastScan>0.2)){ m_lastScan=t; scan(); }
}

// frees every resident mesh that is no longer wanted, but only once everything that now covers its area is on the GPU (no holes while moving)
void ChunkStreamer::releaseCovered(){
 std::vector<uint64_t> rel;
 for(auto&kv:m_meshed){
  if(m_wantSet.count(kv.first))continue; bool ok=true;
  for(const MeshCell&d:m_want){ if(!overlaps(kv.second.c,d))continue; if(!m_meshed.count(keyOf(d))){ok=false;break;} }
  if(ok)rel.push_back(kv.first);
 }
 for(uint64_t k:rel){ int a,b; splitKey(k,a,b); m_sink.release(a,b); m_meshed.erase(k); m_version.erase(k); m_gpuFull=false; }
}

void ChunkStreamer::scan(){
 const int R=m_r,pcx=m_cx,pcz=m_cz; m_needScan=false;
 int secs=0; { const auto pc=m_world.acquire(pcx,pcz,true); secs=pc->maxY<0?0:std::min(Chunk::SECTIONS,(pc->maxY>>4)+1); } m_playerSecs=secs;
 std::vector<MeshCell> want; std::unordered_set<uint64_t> newSplit;
 auto inR=[&](int x,int z,int r){return chunkInRange(x-pcx,z-pcz,r);};
 // nine-way split: a cell is drawn as ONE mesh when it is far enough from the player (hysteresis of one chunk) and lies inside the view radius, else it is split into 3x3 smaller cells
 std::function<void(int,int,int)> visit=[&](int tier,int X,int Z){
  const int s=sizeOf(tier),x0=X*s,z0=Z*s,x1=x0+s-1,z1=z0+s-1,nx=std::clamp(pcx,x0,x1),nz=std::clamp(pcz,z0,z1);
  if(!inR(nx,nz,R))return;
  if(tier==1){ if(X==pcx&&Z==pcz){ for(int sc=0;sc<secs;sc++)want.push_back(MeshCell{0,X,Z,sc}); } else want.push_back(MeshCell{1,X,Z,0}); return; }
  const int slack=(s-1)/4; const bool full=inR(x0,z0,R+slack)&&inR(x1,z0,R+slack)&&inR(x0,z1,R+slack)&&inR(x1,z1,R+slack);
  const int dmin=std::max(std::abs(nx-pcx),std::abs(nz-pcz)); const uint64_t k=keyOf(MeshCell{tier,X,Z,0});
  const int thr=(pow3(tier-1)-1)/2+(m_split.count(k)?1:0);
  if(!full||dmin<=thr){ newSplit.insert(k); for(int dz=0;dz<3;dz++)for(int dx=0;dx<3;dx++)visit(tier-1,X*3+dx,Z*3+dz); }
  else want.push_back(MeshCell{tier,X,Z,0});
 };
 for(int Z=fdiv(pcz-R,9);Z<=fdiv(pcz+R,9);Z++)for(int X=fdiv(pcx-R,9);X<=fdiv(pcx+R,9);X++)visit(3,X,Z);
 m_want=std::move(want); m_wantSet.clear(); for(const MeshCell&c:m_want)m_wantSet.insert(keyOf(c)); m_split.swap(newSplit);
 m_world.pruneCache(m_cx,m_cz,std::clamp(R+4,4,m_cacheR));
 releaseCovered();
 // what is missing, closest first (sections of the player's chunk before everything else)
 struct Miss{MeshCell c;int d;}; std::vector<Miss> miss;
 { std::lock_guard<std::mutex> l(m_mu);
   for(const MeshCell&c:m_want){ const uint64_t k=keyOf(c); if(m_meshed.count(k)||m_inflight.count(k))continue;
    const int s=sizeOf(c.tier); const int nx=std::clamp(pcx,c.x*s,c.x*s+s-1),nz=std::clamp(pcz,c.z*s,c.z*s+s-1);
    miss.push_back(Miss{c,c.tier==0?-1:std::max(std::abs(nx-pcx),std::abs(nz-pcz))}); } }
 std::stable_sort(miss.begin(),miss.end(),[](const Miss&a,const Miss&b){return a.d!=b.d?a.d<b.d:a.c.tier<b.c.tier;});
 { std::lock_guard<std::mutex> l(m_mu);
   const size_t room=m_cap>m_inflight.size()?m_cap-m_inflight.size():0;
   std::deque<Job> keep; for(auto&j:m_queue)if(m_meshed.count(keyOf(j.c)))keep.push_back(j);   // keep the re-meshes of resident cells
   m_queue=std::move(keep); m_queued.clear(); for(auto&j:m_queue)m_queued.insert(keyOf(j.c));
   if(!m_gpuFull&&!m_paused)for(size_t i=0;i<miss.size()&&i<room;i++){ const uint64_t k=keyOf(miss[i].c); const uint32_t ver=++m_verCounter; m_version[k]=ver; m_queue.push_back(Job{miss[i].c,ver}); m_queued.insert(k); }
   m_allQueued=miss.empty()&&!m_gpuFull&&m_queue.empty()&&m_inflight.empty(); }
 m_cv.notify_all();
}

void ChunkStreamer::pump(int maxUploads){
 for(int n=0;n<8&&!m_editOrder.empty();n++){   // block edits: rebuild the affected section / chunk right now (about 0.1 - 1 ms), flag the big meshes above it
  const uint64_t ck=m_editOrder.back(); m_editOrder.pop_back(); const uint32_t mask=m_editMap[ck]; m_editMap.erase(ck); int cx,cz; splitKey(ck,cx,cz);
  std::vector<MeshCell> now,stale;
  for(auto&kv:m_meshed){ const MeshCell&g=kv.second.c; if(!covers(g,cx,cz))continue;
   if(g.tier==0){ if((mask&(1u<<31))||(mask&(1u<<g.sec)))now.push_back(g); } else if(g.tier==1)now.push_back(g); else stale.push_back(g); }
  for(const MeshCell&g:now)rebuildNow(g);
  for(const MeshCell&g:stale)queueRebuild(g);
  for(const MeshCell&d:m_want)if(covers(d,cx,cz)&&!m_meshed.count(keyOf(d)))queueRebuild(d);
 }
 std::vector<Result> batch;
 { std::lock_guard<std::mutex> l(m_mu); if(m_results.empty())return; batch.swap(m_results); }
 int done=0; std::vector<Result> later;
 for(auto&r:batch){
  const uint64_t k=keyOf(r.c); if(r.epoch!=m_epoch)continue;
  auto vit=m_version.find(k); if(vit!=m_version.end()&&r.ver<vit->second)continue;   // a newer job exists
  if(!m_wantSet.count(k)&&!m_meshed.count(k))continue;                              // not needed any more
  if(done>=maxUploads){later.push_back(std::move(r));continue;}
  int a,b; splitKey(k,a,b);
  if(m_sink.upload(a,b,r.mesh)){ Meshed&mm=m_meshed[k]; mm.c=r.c; mm.ver=r.ver; done++; }
  else if(m_sink.uploadBusy()){ later.push_back(std::move(r)); done=maxUploads; }
  else m_gpuFull=true;
 }
 if(!later.empty()){ std::lock_guard<std::mutex> l(m_mu); for(auto&r:later)m_results.push_back(std::move(r)); }
 if(done>0)releaseCovered();
}

void ChunkStreamer::trim(){ m_world.pruneCache(m_cx,m_cz,std::clamp(m_cacheR,4,8)); m_needScan=true; }
