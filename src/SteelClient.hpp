#pragma once
#include <cstdint>
// Optional vanilla-exact terrain from SteelMC through the "steel-provider" helper program (see steel/README.txt).
// The provider is started on first use and talks over a local TCP socket; chunks come back in Minecraft's network section format.
struct Chunk;
namespace steel{
 bool available();                                          // steel/steel-provider + steel/block.json found (cached)
 bool generate(uint64_t seed,int cx,int cz,int dim,Chunk&out);   // dim: 0 overworld, 1 nether, 2 end      // false = could not (provider missing / crashed): caller falls back to the built-in generator
 const char* status();                                       // "ok" or why terrain falls back to the flat world
 void shutdown();                                           // stops the helper process
}
