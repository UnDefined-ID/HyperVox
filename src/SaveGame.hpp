#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include "Inventory.hpp"
#include "World.hpp"

enum GameMode{MODE_SURVIVAL=0,MODE_CREATIVE=1,MODE_SPECTATOR=2};

struct WorldInfo{
 std::string folder,name; uint64_t seed=0; int mode=MODE_SURVIVAL; int64_t created=0,lastPlayed=0;
 bool debug=false; float health=20.f,sat=5.f,exh=0.f; int food=20; bool hasPlayer=false; float px=0,py=0,pz=0,yaw=0,pitch=0; bool flying=false; Inventory inv;
};

// ./worlds/<folder>/level.dat (gzip NBT) + region/r.X.Z.mca : real Minecraft 1.21.11 (DataVersion 4671) Anvil worlds.
// Chunks are loaded from / written to the region files one by one; chunks that do not exist yet are generated from the seed.
class SaveManager{
public:
 SaveManager();
 std::vector<WorldInfo> list() const;                       // newest first
 bool create(const std::string&name,int mode,uint64_t seed,WorldInfo&out);
 bool remove(const std::string&folder);
 bool saveLevel(const WorldInfo&w) const;
 std::filesystem::path regionDir(const std::string&folder) const{return m_root/folder/"region";}
 const std::filesystem::path& root() const{return m_root;}
 static std::string sanitize(const std::string&name);
 static uint64_t seedFromString(const std::string&s);       // numbers are used as-is, text is hashed, empty = random
private:
 bool readLevel(const std::filesystem::path&dir,WorldInfo&w) const;
 std::filesystem::path m_root;
};
