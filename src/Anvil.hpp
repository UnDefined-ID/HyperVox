#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
// Minimal NBT + gzip/zlib + Anvil (.mca) support: enough to write and read Minecraft 1.21.11 worlds (DataVersion 4671).
struct Nbt{
 enum T{End,Byte,Short,Int,Long,Float,Double,ByteArr,Str,List,Comp,IntArr,LongArr};
 int t=End; int64_t i=0; double d=0; std::string s; std::vector<int32_t> ia; std::vector<int64_t> la; int lt=End; std::vector<Nbt> list; std::vector<std::pair<std::string,Nbt>> comp;
 static Nbt Int_(int32_t v){Nbt n;n.t=Int;n.i=v;return n;} static Nbt Byte_(int v){Nbt n;n.t=Byte;n.i=v;return n;} static Nbt Long_(int64_t v){Nbt n;n.t=Long;n.i=v;return n;}
 static Nbt Float_(float v){Nbt n;n.t=Float;n.d=v;return n;} static Nbt Double_(double v){Nbt n;n.t=Double;n.d=v;return n;} static Nbt Str_(const std::string&v){Nbt n;n.t=Str;n.s=v;return n;}
 static Nbt Comp_(){Nbt n;n.t=Comp;return n;} static Nbt List_(int type){Nbt n;n.t=List;n.lt=type;return n;} static Nbt LongArr_(std::vector<int64_t> v){Nbt n;n.t=LongArr;n.la=std::move(v);return n;}
 Nbt& set(const std::string&k,Nbt v){comp.emplace_back(k,std::move(v));return comp.back().second;}
 const Nbt* get(const std::string&k)const{for(auto&c:comp)if(c.first==k)return &c.second;return nullptr;}
};
void nbtWrite(std::vector<uint8_t>&out,const Nbt&root,const std::string&name="");
bool nbtRead(const uint8_t*p,size_t n,Nbt&root);
bool gzipCompress(const std::vector<uint8_t>&in,std::vector<uint8_t>&out,bool gzip);
bool inflateAny(const uint8_t*p,size_t n,std::vector<uint8_t>&out);          // gzip or zlib
bool readFile(const std::string&path,std::vector<uint8_t>&out);
bool writeFileAtomic(const std::string&path,const std::vector<uint8_t>&data);
// region files: chunks keyed by absolute chunk coordinates, payload = uncompressed NBT bytes
struct RegionChunk{int cx,cz;std::vector<uint8_t> nbt;};
bool writeRegion(const std::string&path,const std::vector<RegionChunk>&chunks);
bool readRegion(const std::string&path,std::vector<RegionChunk>&chunks,int rx,int rz);
