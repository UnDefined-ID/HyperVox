#pragma once
#include <array>
#include <mutex>
#include <cstdint>
#include <functional>
#include <memory>
#include <shared_mutex>
#include <unordered_map>
#include <utility>
#include <vector>
#include "Math.hpp"
#include "Registry.hpp"

// The world is 384 blocks tall like Minecraft 1.18+ (y -64 .. 319).  Internally y is stored +64 (our y 0 = Minecraft y -64).
struct Chunk{
 static constexpr int SIZE=16,HEIGHT=384,Y_OFFSET=64,SECTIONS=HEIGHT/16;
 // Two forms.  BUILDING: a dense array, sparse in height (only the layers up to the highest block are stored); generators and loaders write into it with at().
 // PACKED (what the world keeps, see pack()): one palette-compressed 16x16x16 section at a time - empty / uniform sections cost nothing and mixed ones 2-4 KB
 // instead of 8 KB, so a typical chunk shrinks from ~100 KB to ~15 KB.  Packed chunks are immutable except through put() on a private copy.
 std::vector<uint16_t> blocks;
 int maxY=-1;                                   // highest y that may contain a block (upper bound)
 struct Section{ std::vector<uint16_t> palette; std::vector<uint8_t> data; uint16_t single=0; uint8_t bits=0; };   // bits 0: every block == single; 4 / 8: palette indices; 16: raw ids
 std::vector<Section> secs;                     // empty while building
 bool packed()const{return !secs.empty();}
 uint16_t& at(int x,int y,int z){ const size_t need=(static_cast<size_t>(y)+1)*SIZE*SIZE; if(blocks.size()<need)blocks.resize(need,uint16_t(0)); return blocks[(static_cast<size_t>(y)*SIZE+z)*SIZE+x]; }   // building form only
 uint16_t at(int x,int y,int z)const;
 void pack();                                   // building -> packed (frees the dense array)
 void put(int x,int y,int z,uint16_t id);       // set one block (packed or building)
 void unpack(uint16_t*dst,int layers)const;     // dense copy of the lowest `layers` layers rounded up to whole sections: dst[(y*16+z)*16+x]; dst needs ceil(layers/16)*4096 entries
 size_t bytes()const;
};
// Packed 8-byte vertex, positions/uvs in 1/16 block units (chunk-local; the chunk origin is supplied per draw call):
//  a = x(9) | z(9)<<9 | y(low 12)<<18 | face&3 <<30      b = u(9) | v(9)<<9 | tile(12)<<18 | (face>>2)<<30 | (y>>12)<<31
// Every 4 vertices are one quad, in an order that makes the shared quad index pattern (0,1,2, 0,2,3) wind counter-clockwise seen from outside, so the GPU keeps NO index data per chunk.
struct Vertex{ uint32_t a,b; };
// Quads are grouped into draw buckets so the renderers can skip every face that points away from the camera:
//  0 = always (plants, double sided), 1 = -X, 2 = -Z, 3 = -Y, 4 = +Y, 5 = +Z, 6 = +X, 7 = translucent (water, stained / tinted glass; drawn sorted in its own pass).
// bucketEnd[i] is the vertex index where bucket i ends (bucket 0 starts at 0).
constexpr int kBuckets=8;
constexpr int kFaceBucket[6]={3,4,1,6,2,5};   // face id (0=-Y 1=+Y 2=-X 3=+X 4=-Z 5=+Z) -> bucket
struct ChunkMesh{ std::vector<Vertex> vertices; uint32_t bucketEnd[kBuckets]={0,0,0,0,0,0,0,0}; int maxY=0; int ox=0,oz=0; float scale=1.f; int size=16; /* world origin (blocks), vertex scale, width in blocks: merged far meshes use scale 2 / 8 */ };
// Which opaque buckets (bit i = bucket i, bucket 0 always) can face the camera.  ox/oy/oz = the chunk's minimum corner relative to the camera, top = highest y of the chunk.
inline unsigned visibleBuckets(float ox,float oy,float oz,int top,float sz=16.f){
 const float e=0.05f; unsigned m=1u;
 if(ox+sz>-e){m|=1u<<1;} if(ox<e){m|=1u<<6;}        // -X faces need the camera on the -X side of them, +X faces the +X side
 if(oz+sz>-e){m|=1u<<2;} if(oz<e){m|=1u<<5;}
 if(oy+float(top)+1.f>-e){m|=1u<<3;} if(oy<e){m|=1u<<4;}
 return m;
}
// Calls f(firstVertex,vertexCount) for every run of adjacent visible buckets (translucent bucket not included).
template<class F> inline void forEachBucketRun(const uint32_t*end,unsigned mask,F&&f){
 uint32_t runStart=0,runEnd=0; bool open=false;
 for(int b=0;b<7;b++){
  const uint32_t s=b?end[b-1]:0,e=end[b];
  if(e==s||!(mask&(1u<<b))){ if(open){f(runStart,runEnd-runStart);open=false;} continue; }
  if(open&&s==runEnd)runEnd=e; else { if(open)f(runStart,runEnd-runStart); runStart=s;runEnd=e;open=true; }
 }
 if(open)f(runStart,runEnd-runStart);
}
struct ChunkSnapshot{ std::shared_ptr<const Chunk> c,xm,xp,zm,zp; };   // chunk + its 4 edge neighbours (immutable)

// Infinite, seeded world.  Chunks come from (1) the cache, (2) the unsaved-changes map, (3) the save folder (real Minecraft Anvil
// files, loaded one chunk at a time), (4) the terrain generator.  Chunks are immutable once published, so mesh workers need no locks.
class World{
public:
 static constexpr int SEA_LEVEL=126;                          // water fills up to this y (Minecraft sea level 63)
 using Loader=std::function<bool(int,int,Chunk&)>;
 World();
 void reset(uint64_t seed);                                   // empty world with a new seed (drops chunks, changes and the loader)
 void setLoader(Loader l);
 void setDimension(int d){m_dim=d;} int dimension()const{return m_dim;}   // 0 overworld, 1 nether, 2 end
 void setSteel(bool s){m_steel=s;}                         // true: terrain from the SteelMC helper (vanilla exact), falls back to the built-in generator
 uint64_t seed()const{return m_seed64;}
 void setDebug(bool d){m_debug=d;}                        // debug world: every block (all shapes / rotations) in a grid
 bool debug()const{return m_debug;}

 uint16_t get(int x,int y,int z)const;                        // loads / generates + caches the chunk on demand
 int set(int x,int y,int z,uint16_t id,int outChunks[3][2]);  // returns how many chunks need re-meshing
 std::shared_ptr<const Chunk> acquire(int cx,int cz,bool cache)const;
 ChunkSnapshot snapshot(int cx,int cz)const;
 static ChunkMesh buildMesh(const ChunkSnapshot&s,int cx,int cz,int ys=0,int ye=1<<30);   // ys..ye = only the blocks in that y range (one section)

 int terrainHeight(int wx,int wz)const;                       // y of the top terrain block (trees excluded)
 Vec3 findSpawn()const;
 void pruneCache(int cx,int cz,int radius,const std::function<bool(int,int)>&keep={});   // keep(): chunks further than 6 away are only kept when this says they are visible                   // drop cached chunks further than radius (unsaved changes are kept)

 using DirtyList=std::vector<std::pair<uint64_t,std::shared_ptr<const Chunk>>>;
 DirtyList dirtyChunks()const;                                // chunks changed since they were last saved
 void markSaved(const DirtyList&saved);                       // forget the ones that were not modified again meanwhile
 size_t dirtyCount()const; int lastEditY()const{return m_lastY;}
 DirtyList takeGenerated();
 std::vector<std::array<int,3>> takeChanges();                  // every block set() since the last call (world coordinates): drives block updates and fluids
 std::vector<std::array<int,3>> takeNewLootChests();           // chests / barrels the generator placed in freshly generated chunks since the last call (world block coordinates)                                  // chunks the generator made since the last call (to be written to the save, so the terrain never changes)
 size_t generatedCount()const;
private:
 static uint64_t key(int cx,int cz){return (static_cast<uint64_t>(static_cast<uint32_t>(cx))<<32)|static_cast<uint32_t>(cz);}
 void generate(int cx,int cz,Chunk&c)const;
 void generateDebug(int cx,int cz,Chunk&c)const;
 bool m_steel=true; bool m_debug=false; int m_dim=0; mutable int m_lastY=-1;
 uint32_t m_seed=0; uint64_t m_seed64=0; Loader m_loader;

 mutable std::shared_mutex m_mu;
 mutable std::unordered_map<uint64_t,std::shared_ptr<const Chunk>> m_chunks;
 std::unordered_map<uint64_t,std::shared_ptr<const Chunk>> m_dirty;
 mutable std::vector<std::pair<uint64_t,std::shared_ptr<const Chunk>>> m_generated; mutable std::vector<std::array<int,3>> m_newLoot,m_changes; mutable std::mutex m_lootMu;
};
