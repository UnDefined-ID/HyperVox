#include "Anvil.hpp"
#include <zlib.h>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>

namespace{
void w8(std::vector<uint8_t>&o,uint8_t v){o.push_back(v);}
void w16(std::vector<uint8_t>&o,uint16_t v){o.push_back(v>>8);o.push_back(v&255);}
void w32(std::vector<uint8_t>&o,uint32_t v){for(int s=24;s>=0;s-=8)o.push_back((v>>s)&255);}
void w64(std::vector<uint8_t>&o,uint64_t v){for(int s=56;s>=0;s-=8)o.push_back((v>>s)&255);}
void wstr(std::vector<uint8_t>&o,const std::string&s){w16(o,(uint16_t)s.size());o.insert(o.end(),s.begin(),s.end());}
void payload(std::vector<uint8_t>&o,const Nbt&n){
 switch(n.t){
  case Nbt::Byte:w8(o,(uint8_t)n.i);break; case Nbt::Short:w16(o,(uint16_t)n.i);break; case Nbt::Int:w32(o,(uint32_t)n.i);break; case Nbt::Long:w64(o,(uint64_t)n.i);break;
  case Nbt::Float:{float f=(float)n.d;uint32_t u;std::memcpy(&u,&f,4);w32(o,u);break;} case Nbt::Double:{uint64_t u;std::memcpy(&u,&n.d,8);w64(o,u);break;}
  case Nbt::Str:wstr(o,n.s);break;
  case Nbt::List:w8(o,(uint8_t)(n.list.empty()?Nbt::End:n.lt));w32(o,(uint32_t)n.list.size());for(auto&e:n.list)payload(o,e);break;
  case Nbt::Comp:for(auto&c:n.comp){w8(o,(uint8_t)c.second.t);wstr(o,c.first);payload(o,c.second);}w8(o,0);break;
  case Nbt::IntArr:w32(o,(uint32_t)n.ia.size());for(auto v:n.ia)w32(o,(uint32_t)v);break;
  case Nbt::LongArr:w32(o,(uint32_t)n.la.size());for(auto v:n.la)w64(o,(uint64_t)v);break;
  default:break;
 }
}
struct Rd{const uint8_t*p;size_t n,i=0;bool ok=true;
 bool need(size_t k){if(i+k>n){ok=false;return false;}return true;}
 uint64_t u(int bytes){if(!need(bytes))return 0;uint64_t v=0;for(int k=0;k<bytes;k++)v=(v<<8)|p[i++];return v;}
 std::string str(){size_t l=(size_t)u(2);if(!need(l))return {};std::string s((const char*)p+i,l);i+=l;return s;}
};
bool readPayload(Rd&r,int t,Nbt&n,int depth){
 if(depth>64)return false; n=Nbt{}; n.t=t;
 switch(t){
  case Nbt::Byte:n.i=(int8_t)r.u(1);break; case Nbt::Short:n.i=(int16_t)r.u(2);break; case Nbt::Int:n.i=(int32_t)r.u(4);break; case Nbt::Long:n.i=(int64_t)r.u(8);break;
  case Nbt::Float:{uint32_t u=(uint32_t)r.u(4);float f;std::memcpy(&f,&u,4);n.d=f;break;} case Nbt::Double:{uint64_t u=r.u(8);std::memcpy(&n.d,&u,8);break;}
  case Nbt::ByteArr:{size_t c=(size_t)r.u(4);if(!r.need(c))return false;r.i+=c;break;}
  case Nbt::Str:n.s=r.str();break;
  case Nbt::List:{n.lt=(int)r.u(1);size_t c=(size_t)r.u(4);if(c>10000000)return false;for(size_t k=0;k<c&&r.ok;k++){Nbt e;if(!readPayload(r,n.lt,e,depth+1))return false;n.list.push_back(std::move(e));}break;}
  case Nbt::Comp:for(;;){int ct=(int)r.u(1);if(!r.ok)return false;if(ct==0)break;std::string name=r.str();Nbt c;if(!readPayload(r,ct,c,depth+1))return false;n.comp.emplace_back(name,std::move(c));}break;
  case Nbt::IntArr:{size_t c=(size_t)r.u(4);if(c>10000000)return false;for(size_t k=0;k<c&&r.ok;k++)n.ia.push_back((int32_t)r.u(4));break;}
  case Nbt::LongArr:{size_t c=(size_t)r.u(4);if(c>10000000)return false;for(size_t k=0;k<c&&r.ok;k++)n.la.push_back((int64_t)r.u(8));break;}
  default:return false;
 }
 return r.ok;
}
}

void nbtWrite(std::vector<uint8_t>&o,const Nbt&root,const std::string&name){w8(o,(uint8_t)root.t);wstr(o,name);payload(o,root);}
bool nbtRead(const uint8_t*p,size_t n,Nbt&root){Rd r{p,n};int t=(int)r.u(1);if(t!=Nbt::Comp)return false;r.str();return readPayload(r,t,root,0);}

bool gzipCompress(const std::vector<uint8_t>&in,std::vector<uint8_t>&out,bool gz){
 z_stream s{}; if(deflateInit2(&s,6,Z_DEFLATED,gz?31:15,8,Z_DEFAULT_STRATEGY)!=Z_OK)return false;
 out.resize(deflateBound(&s,(uLong)in.size())+32); s.next_in=const_cast<Bytef*>(in.data()); s.avail_in=(uInt)in.size(); s.next_out=out.data(); s.avail_out=(uInt)out.size();
 const int r=deflate(&s,Z_FINISH); out.resize(s.total_out); deflateEnd(&s); return r==Z_STREAM_END;
}
bool inflateAny(const uint8_t*p,size_t n,std::vector<uint8_t>&out){
 z_stream s{}; if(inflateInit2(&s,47)!=Z_OK)return false; s.next_in=const_cast<Bytef*>(p); s.avail_in=(uInt)n; out.clear(); uint8_t buf[65536]; int r;
 do{ s.next_out=buf; s.avail_out=sizeof buf; r=inflate(&s,Z_NO_FLUSH); if(r!=Z_OK&&r!=Z_STREAM_END){inflateEnd(&s);return false;} out.insert(out.end(),buf,buf+(sizeof buf-s.avail_out)); }while(r!=Z_STREAM_END);
 inflateEnd(&s); return true;
}
bool readFile(const std::string&path,std::vector<uint8_t>&out){std::ifstream f(path,std::ios::binary);if(!f)return false;out.assign(std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>());return true;}
bool writeFileAtomic(const std::string&path,const std::vector<uint8_t>&d){
 const std::string tmp=path+".tmp"; { std::ofstream f(tmp,std::ios::binary|std::ios::trunc); if(!f)return false; f.write((const char*)d.data(),(std::streamsize)d.size()); f.flush(); if(!f)return false; }
 std::error_code ec; std::filesystem::rename(tmp,path,ec); if(ec){std::filesystem::remove(path,ec);std::filesystem::rename(tmp,path,ec);} return !ec;
}
bool writeRegion(const std::string&path,const std::vector<RegionChunk>&chunks){
 std::vector<uint8_t> file(8192,0); const uint32_t now=(uint32_t)std::time(nullptr);
 for(auto&c:chunks){
  std::vector<uint8_t> z; if(!gzipCompress(c.nbt,z,false))return false;
  const uint32_t len=(uint32_t)z.size()+1; const uint32_t sectors=(len+4+4095)/4096; const uint32_t start=(uint32_t)(file.size()/4096);
  std::vector<uint8_t> rec; w32(rec,len); w8(rec,2); rec.insert(rec.end(),z.begin(),z.end()); rec.resize((size_t)sectors*4096,0); file.insert(file.end(),rec.begin(),rec.end());
  const int idx=((c.cz&31)<<5)|(c.cx&31); const uint32_t loc=(start<<8)|(sectors&255);
  for(int k=0;k<4;k++){file[idx*4+k]=(loc>>(24-8*k))&255;file[4096+idx*4+k]=(now>>(24-8*k))&255;}
 }
 return writeFileAtomic(path,file);
}
bool readRegion(const std::string&path,std::vector<RegionChunk>&chunks,int rx,int rz){
 std::vector<uint8_t> f; if(!readFile(path,f)||f.size()<8192)return false;
 for(int idx=0;idx<1024;idx++){
  const uint32_t loc=((uint32_t)f[idx*4]<<24)|((uint32_t)f[idx*4+1]<<16)|((uint32_t)f[idx*4+2]<<8)|f[idx*4+3]; if(!loc)continue;
  const size_t off=(size_t)(loc>>8)*4096; if(off+5>f.size())continue;
  const uint32_t len=((uint32_t)f[off]<<24)|((uint32_t)f[off+1]<<16)|((uint32_t)f[off+2]<<8)|f[off+3]; const int comp=f[off+4];
  if(len<1||off+4+len>f.size())continue;
  RegionChunk rc; rc.cx=rx*32+(idx&31); rc.cz=rz*32+(idx>>5);
  if(comp==2||comp==1){ if(!inflateAny(&f[off+5],len-1,rc.nbt))continue; } else if(comp==3)rc.nbt.assign(f.begin()+off+5,f.begin()+off+4+len); else continue;
  chunks.push_back(std::move(rc));
 }
 return true;
}
