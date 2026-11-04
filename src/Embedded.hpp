#pragma once
#include <cstddef>
// Files baked into the executable at build time (shaders, and the asset / sound zips when they were in assets/ while building),
// so the game can be shipped as a single file.  name = "block.vert.spv" ... , "assets", "sounds".  p==nullptr when not embedded.
struct EmbeddedBlob{ const unsigned char*p=nullptr; size_t n=0; };
EmbeddedBlob embeddedBlob(const char*name);
