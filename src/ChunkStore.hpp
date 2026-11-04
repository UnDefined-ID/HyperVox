#pragma once
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include "World.hpp"

uint16_t mapMcBlock(const std::string&name,const std::string&axis,const std::string&facing,const std::string&half,const std::string&type);   // Minecraft block (no namespace) + properties -> block id
// Reads / writes single chunks of a Minecraft Anvil region folder (r.X.Z.mca).  Reading decompresses only the one chunk that is needed;
// writing only touches the sectors of the changed chunks, so big imported Minecraft worlds load instantly and saving stays cheap.
class ChunkStore{
public:
 explicit ChunkStore(std::filesystem::path regionDir):m_dir(std::move(regionDir)){}
 bool load(int cx,int cz,Chunk&out) const;                 // thread safe; false = chunk is not in the save
 bool save(const World::DirtyList&dirty);                  // thread safe
private:
 struct Region{ std::mutex mu; std::vector<uint8_t> header; bool exists=false; std::filesystem::path path; };
 std::shared_ptr<Region> region(int rx,int rz) const;
 std::filesystem::path m_dir; mutable std::mutex m_mu; mutable std::unordered_map<uint64_t,std::shared_ptr<Region>> m_regions;
};
