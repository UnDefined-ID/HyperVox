#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <shared_mutex>
#include <unordered_map>
#include <utility>
#include <vector>
#include "Math.hpp"
#include "Registry.hpp"
#include "Climate.hpp"

// The world is 384 blocks tall like Minecraft 1.18+ (y -64 .. 319).  Internally y is stored +64 (our y 0 = Minecraft y -64).
struct Chunk{
 static constexpr int SIZE=16,HEIGHT=384,Y_OFFSET=64;
 std::vector<uint16_t> blocks=std::vector<uint16_t>(static_cast<size_t>(SIZE)*HEIGHT*SIZE,uint16_t(0));
 int maxY=-1;                                   // highest y that may contain a block (upper bound)
 uint16_t& at(int x,int y,int z){return blocks[(static_cast<size_t>(y)*SIZE+z)*SIZE+x];}
 const uint16_t& at(int x,int y,int z)const{return blocks[(static_cast<size_t>(y)*SIZE+z)*SIZE+x];}
};
// Packed 8-byte vertex, positions/uvs in 1/16 block units (chunk-local; the chunk origin is supplied per draw call):
//  a = x(9) | z(9)<<9 | y(low 12)<<18 | face&3 <<30      b = u(9) | v(9)<<9 | tile(12)<<18 | (face>>2)<<30 | (y>>12)<<31
struct Vertex{ uint32_t a,b; };
struct ChunkMesh{ std::vector<Vertex> vertices; std::vector<uint32_t> indices,tindices; int maxY=0; };   // indices = opaque/cutout, tindices = translucent (water, stained/tinted glass)
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
 uint64_t seed()const{return m_seed64;}
 void setDebug(bool d){m_debug=d;}                        // debug world: every block (all shapes / rotations) in a grid
 bool debug()const{return m_debug;}

 uint16_t get(int x,int y,int z)const;                        // loads / generates + caches the chunk on demand
 int set(int x,int y,int z,uint16_t id,int outChunks[3][2]);  // returns how many chunks need re-meshing
 std::shared_ptr<const Chunk> acquire(int cx,int cz,bool cache)const;
 ChunkSnapshot snapshot(int cx,int cz)const;
 static ChunkMesh buildMesh(const ChunkSnapshot&s,int cx,int cz);

 int terrainHeight(int wx,int wz)const;                       // y of the top terrain block (trees excluded)
 Vec3 findSpawn()const;
 void pruneCache(int cx,int cz,int radius);                   // drop cached chunks further than radius (unsaved changes are kept)

 using DirtyList=std::vector<std::pair<uint64_t,std::shared_ptr<const Chunk>>>;
 DirtyList dirtyChunks()const;                                // chunks changed since they were last saved
 void markSaved(const DirtyList&saved);                       // forget the ones that were not modified again meanwhile
 size_t dirtyCount()const;
 struct Column{ int top; int biome; float c,e,t,h,w; };                  // terrain column: surface y (our numbering), biome id and the climate values
 Column columnAt(int wx,int wz)const;
private:
 static uint64_t key(int cx,int cz){return (static_cast<uint64_t>(static_cast<uint32_t>(cx))<<32)|static_cast<uint32_t>(cz);}
 void generate(int cx,int cz,Chunk&c)const;
 void generateDebug(int cx,int cz,Chunk&c)const;
 bool m_debug=false;
 uint32_t m_seed=0; uint64_t m_seed64=0; std::shared_ptr<const Climate> m_climate; Loader m_loader;

 mutable std::shared_mutex m_mu;
 mutable std::unordered_map<uint64_t,std::shared_ptr<const Chunk>> m_chunks;
 std::unordered_map<uint64_t,std::shared_ptr<const Chunk>> m_dirty;
};
