#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

// Where textures come from: the bundled default pack (base) plus an optional resource pack on top (overlay).
// A texture is taken from the overlay when the pack provides it, otherwise from the default pack.
struct AssetPaths{
 std::filesystem::path base,overlay; std::string packName="default";
 std::filesystem::path resolve(const std::string&rel)const;
 bool exists(const std::string&rel)const;
 static std::filesystem::path findBase(const std::filesystem::path&root);              // directory containing block/
 static std::filesystem::path normalizePackRoot(const std::filesystem::path&dir);       // accepts assets/minecraft/textures layouts
 uint64_t signature(uint32_t formatVersion)const;                                       // changes when any texture file changes
};

struct PackInfo{ std::string name,description; std::filesystem::path dir,icon,zip; bool builtin=false; };   // zip packs are unpacked into `dir` by materializePack() before use
// Default pack first, then every folder / .zip found in the given search folders (zips are unpacked under extractRoot on demand).
std::vector<PackInfo> listResourcePacks(const AssetPaths&defaults,const std::vector<std::filesystem::path>&searchDirs,const std::filesystem::path&extractRoot);
// Finds (or unpacks from a zip) the game's own data: kind = "assets" (textures) or "sounds".  Looks in the data folder first
// (~/.local/share/hypervox, %APPDATA%\\hypervox), accepting an extracted folder or minecraft-<kind>*.zip; the old locations next to the executable still work.
std::filesystem::path ensureDataFolder(const char*kind);
bool extractZipAll(const std::filesystem::path&zip,const std::filesystem::path&dest);
bool materializePack(const PackInfo&p);                       // no-op for folders; unpacks the textures of a .zip pack once
bool importPackOrSkin(const std::filesystem::path&file,const std::filesystem::path&destDir);   // copies a dropped file / folder into destDir
void openFolder(const std::filesystem::path&dir);             // opens the folder in the system file manager

std::string safeFileName(const std::string&s);
