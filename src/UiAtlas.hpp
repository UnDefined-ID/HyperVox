#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>
#include "Assets.hpp"
#include "Png.hpp"
#include "Registry.hpp"
#include "TexturePack.hpp"

struct Sprite{ float u0=0,v0=0,u1=0,v1=0; int w=0,h=0; bool valid()const{return w>0;} };

// One texture holding every GUI sprite, the font, block icons and item icons (cached on disk like the block atlas).
class UiAtlas{
public:
 bool load(const AssetPaths&assets,const TexturePack&pack,const std::filesystem::path&cacheDir);
 bool loadedFromCache()const{return m_fromCache;}
 const AtlasImage& image()const{return m_img;}
 const Sprite& get(const std::string&gui)const;           // e.g. "gui/sprites/widget/button"; solid white if missing
 const Sprite& white()const{return m_white;}
 const Sprite& tile(uint16_t texId)const{return texId<m_tiles.size()?m_tiles[texId]:m_white;}      // 16x16 block texture
 const Sprite& plainItem(size_t index)const{return index<m_items.size()?m_items[index]:m_white;}
 const Sprite& glyph(int c)const{return (c>=32&&c<=126)?m_glyph[c-32]:m_glyph['?'-32];}
 bool hasGlyphs()const{return m_glyph[1].valid();}
private:
 bool readCache(const std::filesystem::path&f,uint64_t sig);
 void writeCache(const std::filesystem::path&f,uint64_t sig)const;
 AtlasImage m_img; Sprite m_white; std::unordered_map<std::string,Sprite> m_sprites;
 std::vector<Sprite> m_tiles,m_items; Sprite m_glyph[95]; bool m_fromCache=false;
};

// The six panorama faces stacked vertically (face i occupies rows i*size .. (i+1)*size), downscaled to size x size.
struct PanoramaImage{ uint32_t size=0; std::vector<uint8_t> rgba; bool valid()const{return size>0;} };
bool loadPanorama(const AssetPaths&assets,const std::filesystem::path&cacheDir,PanoramaImage&out);
