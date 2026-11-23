#pragma once
// Entity model <-> texture atlas interface (kept separate so TexturePack does not need the whole entity system).
#include <string>
#include <vector>
// A face of a model box that becomes a 16x16 tile of the block atlas (file relative to the asset root).  The rectangle is in the units of the model's nominal texture size
// (texW x texH); when the png is bigger (e.g. a 128x64 ghast) the rectangle is scaled.  hurt = also bake a red tinted copy ("<key>:h"), flash = also a white copy ("<key>:f").
struct TexReq{ std::string key,file; int x,y,w,h; int texW=64,texH=32; bool hurt=true,flash=false; };
void entityTextureRequests(std::vector<TexReq>&out);
class TexturePack;
void entityResolveTiles(const TexturePack&pack);          // after the atlas exists: look the baked tiles up
