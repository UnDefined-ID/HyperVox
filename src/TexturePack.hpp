#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>
#include "Assets.hpp"
#include "Png.hpp"

// One animated 16x16 block texture frame to write into the atlas.
struct TileUpdate{ uint16_t id; uint8_t rgba[16*16*4]; };
// An animated block texture (water, lava, fire, magma, sea lantern ...): frames from the *.png.mcmeta timing, 1 tick = 1/20 s.
struct TileAnim{ uint16_t tile=0; std::vector<std::array<uint8_t,1024>> frames; std::vector<std::pair<int,int>> seq; int total=0; bool interp=false; int lastKey=-1; };

// Packs every block/*.png into ONE 16x16-tile atlas (64 tiles per row) that the whole world is drawn from.
// The packed atlas is saved to <cacheDir> and reused on the next start while the texture files are unchanged.
class TexturePack {
public:
 static constexpr uint32_t kCols=64;                       // keep in sync with TILES in shaders/block.frag
 bool load(const AssetPaths& assets,const std::filesystem::path& cacheDir);
 bool loadedFromCache()const{return m_fromCache;}
 bool has(const std::string& texture) const;
 uint16_t id(const std::string& texture) const;           // 0 if missing
 const AtlasImage& atlas() const{return m_atlas;}
 uint32_t atlasSize()const{return kCols*16;}
 size_t textureCount()const{return m_ids.size();}
 const AssetPaths& assets()const{return m_assets;}
 const std::filesystem::path& assetRoot()const{return m_assets.base;}
 const std::vector<TileAnim>& animations()const{return m_anims;}
 void collectAnimationUpdates(double ticks,std::vector<TileUpdate>&out);   // only tiles whose frame changed
 const uint8_t* tilePixels(uint16_t id)const;             // 16x16 RGBA, row stride = atlasSize*4
private:
 void loadAnimations(); std::vector<TileAnim> m_anims;
 bool readCache(const std::filesystem::path&f,uint64_t sig);
 void writeCache(const std::filesystem::path&f,uint64_t sig)const;
 std::unordered_map<std::string,uint16_t> m_ids; AssetPaths m_assets; AtlasImage m_atlas; bool m_fromCache=false;
};
