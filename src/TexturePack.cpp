#include "TexturePack.hpp"
#include <map>
#include "EntityModel.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <iostream>

namespace {
constexpr uint32_t kCacheVersion=13;
// Colormap tints (the pack ships grayscale grass / foliage / water that vanilla tints per biome).  alphaSet<0 = keep the texture alpha.
struct Tint{const char*name;int r,g,b;int alphaSet,alphaCap;};
const Tint kTints[]={
 {"grass_block_top",145,189,89,-1,255},
 {"oak_leaves",95,165,45,-1,255},{"jungle_leaves",95,165,45,-1,255},{"acacia_leaves",95,165,45,-1,255},
 {"dark_oak_leaves",95,165,45,-1,255},{"mangrove_leaves",95,165,45,-1,255},
 {"birch_leaves",128,167,85,-1,255},{"spruce_leaves",97,153,97,-1,255},
 {"short_grass",145,189,89,-1,255},{"fern",145,189,89,-1,255},{"sugar_cane",145,189,89,-1,255},
 {"tall_grass_bottom",145,189,89,-1,255},{"tall_grass_top",145,189,89,-1,255},{"large_fern_bottom",145,189,89,-1,255},{"large_fern_top",145,189,89,-1,255},{"vine",95,165,45,-1,255},
 {"water_still",63,118,228,-1,255},{"water_flow",63,118,228,-1,255},
 {"melon_stem",110,190,25,-1,255},{"pumpkin_stem",110,190,25,-1,255},{"attached_melon_stem",150,190,40,-1,255},{"attached_pumpkin_stem",150,190,40,-1,255},   // stems are grey in the pack
 {"leaf_litter",125,100,45,-1,255},{"redstone_dust_dot",115,8,8,-1,255},{"redstone_dust_line0",115,8,8,-1,255},{"redstone_dust_line1",115,8,8,-1,255},          // translucent, see-through water like vanilla
 {"tinted_glass",160,160,160,-1,150},         // 160 = no colour change; always see-through
};
const Tint* findTint(const std::string&k){ for(auto&t:kTints)if(k==t.name)return &t; return nullptr; }

// Box-filters frame `frame` of a (possibly animated strip) image down to a tinted 16x16 tile.
void makeTile(const AtlasImage&img,uint32_t frame,const Tint*tint,uint8_t*out){
 const uint32_t tile=std::min(img.width,img.height),oy0=frame*tile;
 for(uint32_t y=0;y<16;y++)for(uint32_t x=0;x<16;x++){
  const uint32_t x0=x*tile/16,x1=std::max(x0+1,(x+1)*tile/16),y0=oy0+y*tile/16,y1=std::max(y0+1,oy0+(y+1)*tile/16);
  uint32_t acc[4]={0,0,0,0},n=0,aw=0;
  for(uint32_t sy=y0;sy<y1&&sy<img.height;sy++)for(uint32_t sx=x0;sx<x1&&sx<img.width;sx++){const uint8_t*s=&img.rgba[((size_t)sy*img.width+sx)*4];acc[0]+=s[0]*s[3];acc[1]+=s[1]*s[3];acc[2]+=s[2]*s[3];acc[3]+=s[3];aw+=s[3];n++;}
  uint8_t px[4]={0,0,0,0}; if(n){ if(aw){px[0]=(uint8_t)(acc[0]/aw);px[1]=(uint8_t)(acc[1]/aw);px[2]=(uint8_t)(acc[2]/aw);} px[3]=(uint8_t)(acc[3]/n); }
  uint8_t*dst=out+((size_t)y*16+x)*4; std::copy(px,px+4,dst);
  if(tint){
   dst[0]=(uint8_t)std::min(255,dst[0]*tint->r/160); dst[1]=(uint8_t)std::min(255,dst[1]*tint->g/160); dst[2]=(uint8_t)std::min(255,dst[2]*tint->b/160);
   if(tint->alphaSet>=0)dst[3]=(uint8_t)tint->alphaSet; dst[3]=(uint8_t)std::min<int>(dst[3],tint->alphaCap);
  }
 }
}
// minimal reader for the "animation" block of a .png.mcmeta file
bool readMcmeta(const std::filesystem::path&p,int&frametime,bool&interp,std::vector<std::pair<int,int>>&frames){
 std::ifstream f(p); if(!f)return false; std::string s((std::istreambuf_iterator<char>(f)),std::istreambuf_iterator<char>());
 if(s.find("\"animation\"")==std::string::npos)return false;
 auto numAfter=[&](size_t from,const char*key,int def)->int{ size_t k=s.find(key,from); if(k==std::string::npos)return def; k=s.find(':',k); if(k==std::string::npos)return def; k++; while(k<s.size()&&s[k]==' ')k++; return std::atoi(s.c_str()+k); };
 frametime=std::max(1,numAfter(0,"\"frametime\"",1));
 { size_t k=s.find("\"interpolate\""); interp=false; if(k!=std::string::npos){ k=s.find(':',k); if(k!=std::string::npos){ k++; while(k<s.size()&&s[k]==' ')k++; interp=s.compare(k,4,"true")==0; } } }
 frames.clear(); size_t k=s.find("\"frames\"");
 if(k!=std::string::npos){ k=s.find('[',k); if(k!=std::string::npos){
   for(k++;k<s.size()&&s[k]!=']';){
    if(s[k]=='{'){ const size_t e=s.find('}',k); if(e==std::string::npos)break; const std::string o=s.substr(k,e-k); size_t i=o.find("\"index\""),t=o.find("\"time\"");
     int idx=0,tm=-1; if(i!=std::string::npos){i=o.find(':',i);idx=std::atoi(o.c_str()+i+1);} if(t!=std::string::npos){t=o.find(':',t);tm=std::atoi(o.c_str()+t+1);} frames.push_back({idx,tm}); k=e+1; }
    else if(s[k]>='0'&&s[k]<='9'){ frames.push_back({std::atoi(s.c_str()+k),-1}); while(k<s.size()&&s[k]>='0'&&s[k]<='9')k++; }
    else k++;
   } } }
 return true;
}
}

bool TexturePack::readCache(const std::filesystem::path&f,uint64_t sig){
 std::ifstream in(f,std::ios::binary); if(!in)return false;
 char magic[4]; in.read(magic,4); if(!in||std::memcmp(magic,"FVKB",4)!=0)return false;
 uint32_t ver=0,cols=0,count=0; uint64_t s=0;
 in.read((char*)&ver,4); in.read((char*)&s,8); in.read((char*)&cols,4); in.read((char*)&count,4);
 if(!in||ver!=kCacheVersion||s!=sig||cols!=kCols||count>100000)return false;
 std::unordered_map<std::string,uint16_t> ids;
 for(uint32_t i=0;i<count;i++){unsigned char l=0;in.read((char*)&l,1);std::string n(l,'\0');in.read(&n[0],l);uint16_t id=0;in.read((char*)&id,2);if(!in)return false;ids.emplace(n,id);}
 AtlasImage img; img.width=img.height=atlasSize(); img.rgba.resize((size_t)img.width*img.height*4);
 in.read((char*)img.rgba.data(),(std::streamsize)img.rgba.size()); if(!in)return false;
 m_ids=std::move(ids); m_atlas=std::move(img); return true;
}
void TexturePack::writeCache(const std::filesystem::path&f,uint64_t sig)const{
 std::error_code ec; std::filesystem::create_directories(f.parent_path(),ec);
 const auto tmp=std::filesystem::path(f).concat(".tmp");
 { std::ofstream out(tmp,std::ios::binary|std::ios::trunc); if(!out)return;
   const uint32_t ver=kCacheVersion,cols=kCols,count=(uint32_t)m_ids.size();
   out.write("FVKB",4); out.write((const char*)&ver,4); out.write((const char*)&sig,8); out.write((const char*)&cols,4); out.write((const char*)&count,4);
   for(auto&kv:m_ids){unsigned char l=(unsigned char)std::min<size_t>(kv.first.size(),255);out.write((const char*)&l,1);out.write(kv.first.data(),l);out.write((const char*)&kv.second,2);}
   out.write((const char*)m_atlas.rgba.data(),(std::streamsize)m_atlas.rgba.size()); if(!out)return; }
 std::filesystem::rename(tmp,f,ec); if(ec){std::filesystem::remove(f,ec);std::filesystem::rename(tmp,f,ec);}
}


// Chest tiles cut from entity/chest/normal.png (64x64 model sheet, any 64-multiple): side = lid + body, front = side + latch, top = lid top.
// Laid out so the 14x14x14 chest box (1..15, 0..14, 1..15) maps straight onto them.
static void chestTiles(const AtlasImage&im,uint8_t*side,uint8_t*front,uint8_t*top,uint8_t*bottom){
 const float s=float(im.width)/64.f; std::fill(side,side+1024,0); std::fill(front,front+1024,0); std::fill(top,top+1024,0); std::fill(bottom,bottom+1024,0);
 auto px=[&](float u,float v,uint8_t*o){ const uint32_t X=std::min(im.width-1,(uint32_t)(u*s)),Y=std::min(im.height-1,(uint32_t)(v*s)); const uint8_t*p=&im.rgba[((size_t)Y*im.width+X)*4]; o[0]=p[0];o[1]=p[1];o[2]=p[2];o[3]=255; };
 for(int ty=0;ty<16;ty++)for(int tx=0;tx<16;tx++){ uint8_t*o=(size_t)(ty*16+tx)*4+side,*of=(size_t)(ty*16+tx)*4+front,*ot=(size_t)(ty*16+tx)*4+top,*ob=(size_t)(ty*16+tx)*4+bottom;
  if(tx>=1&&tx<=14&&ty>=1&&ty<=14){ px(14.5f+float(tx-1),0.5f+float(ty-1),ot); px(14.5f+float(tx-1),0.5f+float(ty-1),ob); for(int k=0;k<3;k++)ob[k]=(uint8_t)(ob[k]*55/100); }   // lid top (14,0) and the body underside (28,19) of the 64x64 sheet
  if(tx>=1&&tx<=14&&ty>=2&&ty<=15){ const int k=ty-2; const float v=k<5?14.5f+float(k):34.5f+float(k-5); px(14.5f+float(tx-1),v,o); std::copy(o,o+4,of);
   if(tx>=7&&tx<=8&&k>=3&&k<=6)px(1.5f+float(tx-7),1.5f+float(k-3),of); } }
}

// Mob heads (skeleton skull, zombie head ...): the 8x8 faces cut from the entity skin and placed so the 8x8x8 head box (4..12) maps straight onto 16x16 tiles.
static void headTiles(const AtlasImage&im,uint8_t out[4][1024]){   // front, side, back, top
 const float s=float(im.width)/64.f; std::fill(&out[0][0],&out[0][0]+4096,0);
 auto cut=[&](int q,int u0,int v0,int tx,int ty){ for(int y=0;y<8;y++)for(int x=0;x<8;x++){ const uint32_t X=std::min(im.width-1,(uint32_t)((u0+x+0.5f)*s)),Y=std::min(im.height-1,(uint32_t)((v0+y+0.5f)*s)); const uint8_t*p=&im.rgba[((size_t)Y*im.width+X)*4]; uint8_t*o=&out[q][((size_t)(ty+y)*16+(tx+x))*4]; o[0]=p[0];o[1]=p[1];o[2]=p[2];o[3]=p[3]; } };
 cut(0,8,8,4,8); cut(1,0,8,4,8); cut(2,24,8,4,8); cut(3,8,0,4,4);
}

// Textures that are "model unwraps" (lantern, candle, rods, chains: the sprite is not laid out like the block faces): rebuild a tile in which the pieces sit at the
// coordinates the box models use, so every face samples the right pixels.   name -> name + "_u"
static void unwrapTile(const AtlasImage&im,int kind,uint8_t*out){
 std::fill(out,out+1024,0); const float s=float(im.width)/16.f;
 auto copy=[&](int sx,int sy,int w,int h,int dx,int dy){ for(int y=0;y<h;y++)for(int x=0;x<w;x++){ const uint32_t X=std::min(im.width-1,(uint32_t)((sx+x+0.5f)*s)),Y=std::min(im.height-1,(uint32_t)((sy+y+0.5f)*s)); const uint8_t*p=&im.rgba[((size_t)Y*im.width+X)*4]; uint8_t*o=out+((size_t)(dy+y)*16+(dx+x))*4; o[0]=p[0];o[1]=p[1];o[2]=p[2];o[3]=p[3]; } };
 switch(kind){
  case 0: copy(0,10,2,6,7,10); copy(0,6,2,2,7,7); break;                                 // candle: body sides (rows 10..15) + top
  case 1: copy(0,2,6,7,5,9); copy(0,9,6,6,5,5); copy(1,0,4,2,6,7); break;               // lantern: body sides, body top / bottom, cap sides
  case 2: copy(0,0,2,16,7,0); break;                                                      // rod (end rod / lightning rod): 2 wide
  case 3: copy(0,0,3,16,6,0); break; }                                                    // chain: 3 wide
}
bool TexturePack::load(const AssetPaths&assets,const std::filesystem::path&cacheDir){
 namespace fs=std::filesystem;
 m_assets=assets; m_ids.clear(); m_atlas={}; m_fromCache=false;
 if(m_assets.base.empty()||!fs::exists(m_assets.base/"block"))return false;
 const uint64_t sig=m_assets.signature(kCacheVersion);
 const fs::path cacheFile=cacheDir/("blocks_"+safeFileName(assets.packName)+".cache");
 if(readCache(cacheFile,sig)){m_fromCache=true;loadAnimations();entityResolveTiles(*this);return true;}

 std::vector<fs::path> files;
 for(auto&e:fs::recursive_directory_iterator(m_assets.base/"block"))if(e.is_regular_file()&&e.path().extension()==".png")files.push_back(e.path());
 std::sort(files.begin(),files.end());
 const uint32_t size=atlasSize(), cols=kCols, maxTiles=cols*cols;
 if(files.size()>maxTiles)return false;
 m_atlas.width=m_atlas.height=size; m_atlas.rgba.assign((size_t)size*size*4,0);
 uint32_t i=0;
 for(auto&p:files){
  const std::string k=p.stem().string();
  AtlasImage img; if(!readPng(m_assets.resolve("block/"+k+".png"),img)||!img.width||!img.height){ if(!readPng(p,img)||!img.width||!img.height)continue; }
  const uint16_t id=(uint16_t)i++;
  const Tint*tint=findTint(k);
  if(m_ids.find(k)==m_ids.end())m_ids.emplace(k,id);
  const uint32_t ox=(id%cols)*16, oy=(id/cols)*16; uint8_t tilePx[16*16*4]; makeTile(img,0,tint,tilePx);   // animated strips: first frame here, the rest in loadAnimations()
  for(uint32_t y=0;y<16;y++)std::copy(tilePx+(size_t)y*64,tilePx+(size_t)y*64+64,&m_atlas.rgba[((size_t)(oy+y)*size+ox)*4]);
 }
 if(m_ids.empty())return false;
 static const char*const kChestSrc[][2]={{"normal","chest"},{"trapped","trapped_chest"},{"ender","ender_chest"},{"copper","copper_chest"},{"copper_exposed","exposed_copper_chest"},{"copper_weathered","weathered_copper_chest"},{"copper_oxidized","oxidized_copper_chest"}};
 for(const auto&cs:kChestSrc){   // chests from the entity sheets: side / front / top tiles
   AtlasImage ch; if(!(readPng(m_assets.resolve(std::string("entity/chest/")+cs[0]+".png"),ch)&&ch.width>=64&&ch.height>=ch.width*3/4&&ch.rgba.size()>=(size_t)ch.width*ch.height*4))continue;
   uint8_t t3[4][1024]; chestTiles(ch,t3[0],t3[1],t3[2],t3[3]); const std::string base=cs[1]; const std::string nm[4]={base+"_side",base+"_front",base+"_top",base+"_bottom"};
   for(int q=0;q<4&&i<maxTiles;q++){ const uint32_t id=i++,ox=(id%cols)*16,oy=(id/cols)*16; m_ids.emplace(nm[q],(uint16_t)id);
    for(uint32_t y=0;y<16;y++)std::copy(t3[q]+(size_t)y*64,t3[q]+(size_t)y*64+64,&m_atlas.rgba[((size_t)(oy+y)*size+ox)*4]); } }


 { std::vector<std::pair<std::string,int>> unw; for(const char*c:{"","white_","orange_","magenta_","light_blue_","yellow_","lime_","pink_","gray_","light_gray_","cyan_","purple_","blue_","brown_","green_","red_","black_"})unw.push_back({std::string(c)+"candle",0});
   for(const char*c:{"","soul_","copper_","exposed_copper_","weathered_copper_","oxidized_copper_"})unw.push_back({std::string(c)+"lantern",1});
   for(const char*c:{"end_rod","lightning_rod","exposed_lightning_rod","weathered_lightning_rod","oxidized_lightning_rod"})unw.push_back({c,2});
   for(const char*c:{"iron_chain","copper_chain","exposed_copper_chain","weathered_copper_chain","oxidized_copper_chain"})unw.push_back({c,3});
   for(const auto&u:unw){ AtlasImage im; if(!(readPng(m_assets.resolve("block/"+u.first+".png"),im)&&im.width>=16&&im.rgba.size()>=(size_t)im.width*im.height*4))continue; if(i>=maxTiles)break;
    uint8_t t16[1024]; unwrapTile(im,u.second,t16); const uint32_t id=i++,ox=(id%cols)*16,oy=(id/cols)*16; m_ids.emplace(u.first+"_u",(uint16_t)id);
    for(uint32_t y=0;y<16;y++)std::copy(t16+(size_t)y*64,t16+(size_t)y*64+64,&m_atlas.rgba[((size_t)(oy+y)*size+ox)*4]); } }
 static const char*const kHeadSrc[][2]={{"entity/skeleton/skeleton.png","skeleton_skull"},{"entity/skeleton/wither_skeleton.png","wither_skeleton_skull"},{"entity/zombie/zombie.png","zombie_head"},{"entity/creeper/creeper.png","creeper_head"},{"entity/piglin/piglin.png","piglin_head"},{"entity/player/wide/steve.png","player_head"}};
 for(const auto&hs:kHeadSrc){ AtlasImage hi; if(!(readPng(m_assets.resolve(hs[0]),hi)&&hi.width>=64&&hi.rgba.size()>=(size_t)hi.width*hi.height*4&&hi.height>=hi.width/2))continue;
   uint8_t t4[4][1024]; headTiles(hi,t4); const std::string base=hs[1]; const std::string nm[4]={base+"_front",base+"_side",base+"_back",base+"_top"};
   for(int q=0;q<4&&i<maxTiles;q++){ const uint32_t id=i++,ox=(id%cols)*16,oy=(id/cols)*16; m_ids.emplace(nm[q],(uint16_t)id);
    for(uint32_t y=0;y<16;y++)std::copy(t4[q]+(size_t)y*64,t4[q]+(size_t)y*64+64,&m_atlas.rgba[((size_t)(oy+y)*size+ox)*4]); } }
 if(m_ids.find("missing")==m_ids.end()&&i<maxTiles){   // built-in MISSING tile: magenta / black checkerboard
  const uint32_t id=i++,ox=(id%cols)*16,oy=(id/cols)*16; m_ids.emplace("missing",(uint16_t)id);
  for(uint32_t y=0;y<16;y++)for(uint32_t x=0;x<16;x++){ const bool m=((x/8)+(y/8))%2==0; uint8_t*d=&m_atlas.rgba[((size_t)(oy+y)*size+ox+x)*4]; d[0]=m?248:0;d[1]=0;d[2]=m?248:0;d[3]=255; }
 }

 // generated tiles: the end portal surface (dark teal with stars) and the decorated pot clay
 if(m_ids.find("end_portal_surface")==m_ids.end()&&i<maxTiles){
  const uint32_t id=i++,ox=(id%cols)*16,oy=(id/cols)*16; m_ids.emplace("end_portal_surface",(uint16_t)id);
  for(uint32_t y=0;y<16;y++)for(uint32_t x=0;x<16;x++){ uint32_t h=(x*73856093u)^(y*19349663u); h^=h>>13; h*=0x5bd1e995u; h^=h>>15; uint8_t*d=&m_atlas.rgba[((size_t)(oy+y)*size+ox+x)*4];
   const bool star=(h%23u)==0, glow=(h%11u)==0; d[0]=star?190:(glow?10:3); d[1]=star?240:(glow?60:14); d[2]=star?235:(glow?72:22); d[3]=255; } }
 { AtlasImage pi; if(m_ids.find("decorated_pot_side")==m_ids.end()&&i<maxTiles&&readPng(m_assets.resolve("entity/decorated_pot/decorated_pot_side.png"),pi)&&pi.width>=16&&pi.height>=16&&pi.rgba.size()>=(size_t)pi.width*pi.height*4){
   const uint32_t id=i++,ox=(id%cols)*16,oy=(id/cols)*16; m_ids.emplace("decorated_pot_side",(uint16_t)id);
   for(uint32_t y=0;y<16;y++)for(uint32_t x=0;x<16;x++)std::copy(&pi.rgba[((size_t)y*pi.width+x)*4],&pi.rgba[((size_t)y*pi.width+x)*4]+4,&m_atlas.rgba[((size_t)(oy+y)*size+ox+x)*4]); } }

 // entity models: every face of every box becomes a 16x16 tile (area averaged / nearest scaled from its rectangle of the entity texture); tile names "e:<model>:<box>:<face>"
 { std::vector<TexReq> reqs; entityTextureRequests(reqs); std::map<std::string,AtlasImage> imgs;
   auto writeTile=[&](uint32_t id,const uint8_t*px){ const uint32_t ox=(id%cols)*16,oy=(id/cols)*16; for(int ty=0;ty<16;ty++)for(int tx=0;tx<16;tx++)std::copy(px+(ty*16+tx)*4,px+(ty*16+tx)*4+4,&m_atlas.rgba[((size_t)(oy+ty)*size+ox+tx)*4]); };
   for(const TexReq&r:reqs){ if(i+2>=maxTiles)break; if(m_ids.count(r.key))continue;
    auto it=imgs.find(r.file); if(it==imgs.end()){ AtlasImage im; if(r.file.empty()||!readPng(m_assets.resolve(r.file),im)||im.rgba.size()<(size_t)im.width*im.height*4)im=AtlasImage{}; it=imgs.emplace(r.file,std::move(im)).first; }
    const AtlasImage&im=it->second; if(im.width==0||r.w<=0||r.h<=0)continue;
    const double kx=(double)im.width/std::max(1,r.texW),ky=(double)im.height/std::max(1,r.texH); const double rx=r.x*kx,ry=r.y*ky,rw=r.w*kx,rh=r.h*ky;
    uint8_t px[1024];
    for(int ty=0;ty<16;ty++)for(int tx=0;tx<16;tx++){
     const double sx0=rx+tx*rw/16.0,sx1=rx+(tx+1)*rw/16.0,sy0=ry+ty*rh/16.0,sy1=ry+(ty+1)*rh/16.0;
     int x0=(int)std::floor(sx0),x1=std::max(x0+1,(int)std::ceil(sx1-1e-6)),y0=(int)std::floor(sy0),y1=std::max(y0+1,(int)std::ceil(sy1-1e-6));
     double a=0,rr=0,gg=0,bb=0; int n=0;
     for(int y=y0;y<y1;y++)for(int x=x0;x<x1;x++){ n++; if(x<0||y<0||x>=(int)im.width||y>=(int)im.height)continue; const uint8_t*p=&im.rgba[((size_t)y*im.width+x)*4]; a+=p[3]; rr+=p[0]*(double)p[3]; gg+=p[1]*(double)p[3]; bb+=p[2]*(double)p[3]; }
     uint8_t*d=&px[(ty*16+tx)*4]; if(a>0&&n>0){ d[0]=(uint8_t)(rr/a); d[1]=(uint8_t)(gg/a); d[2]=(uint8_t)(bb/a); d[3]=(uint8_t)(a/n); } else d[0]=d[1]=d[2]=d[3]=0; }
    uint32_t id=i++; m_ids.emplace(r.key,(uint16_t)id); writeTile(id,px);
    if(r.hurt&&i<maxTiles){ uint8_t hp[1024]; for(int k=0;k<256;k++){ hp[k*4]=(uint8_t)std::min(255,px[k*4]*6/10+110); hp[k*4+1]=(uint8_t)(px[k*4+1]*5/10); hp[k*4+2]=(uint8_t)(px[k*4+2]*5/10); hp[k*4+3]=px[k*4+3]; } id=i++; m_ids.emplace(r.key+":h",(uint16_t)id); writeTile(id,hp); }
    if(r.flash&&i<maxTiles){ uint8_t fp[1024]; for(int k=0;k<256;k++){ fp[k*4]=(uint8_t)std::min(255,px[k*4]/3+170); fp[k*4+1]=(uint8_t)std::min(255,px[k*4+1]/3+170); fp[k*4+2]=(uint8_t)std::min(255,px[k*4+2]/3+170); fp[k*4+3]=px[k*4+3]; } id=i++; m_ids.emplace(r.key+":f",(uint16_t)id); writeTile(id,fp); } } }
 // fire balls and 64 spare tiles that the game fills at run time with item sprites (dropped items)
 auto solidTile=[&](const char*name,uint8_t r,uint8_t g,uint8_t b){ if(m_ids.count(name)||i>=maxTiles)return; const uint32_t id=i++,ox=(id%cols)*16,oy=(id/cols)*16; m_ids.emplace(name,(uint16_t)id); for(uint32_t y=0;y<16;y++)for(uint32_t x=0;x<16;x++){ uint8_t*d=&m_atlas.rgba[((size_t)(oy+y)*size+ox+x)*4]; const bool core=(x>=4&&x<12&&y>=4&&y<12); d[0]=core?255:r; d[1]=core?(uint8_t)std::min(255,g+80):g; d[2]=core?(uint8_t)std::min(255,b+60):b; d[3]=255; } };
 solidTile("fx_fireball",255,140,0); solidTile("fx_dragonfire",170,40,230); solidTile("fx_arrow",150,120,80);
 for(int q=0;q<64&&i<maxTiles;q++){ const std::string nm="dyn_item_"+std::to_string(q); if(m_ids.count(nm))continue; const uint32_t id=i++; m_ids.emplace(nm,(uint16_t)id); }
 writeCache(cacheFile,sig);
 loadAnimations();
 entityResolveTiles(*this); return true;
}
const uint8_t* TexturePack::tilePixels(uint16_t id)const{
 const uint32_t size=atlasSize(); return &m_atlas.rgba[((size_t)(id/kCols)*16*size+(size_t)(id%kCols)*16)*4];
}
bool TexturePack::has(const std::string&texture)const{return m_ids.find(texture)!=m_ids.end();}
uint16_t TexturePack::id(const std::string&texture)const{auto it=m_ids.find(texture);return it==m_ids.end()?0:it->second;}

void TexturePack::loadAnimations(){
 namespace fs=std::filesystem; m_anims.clear();
 for(auto&kv:m_ids){
  const std::string&k=kv.first; const fs::path meta=m_assets.resolve("block/"+k+".png.mcmeta"); std::error_code ec; if(!fs::exists(meta,ec))continue;
  int ft=1; bool interp=false; std::vector<std::pair<int,int>> fr; if(!readMcmeta(meta,ft,interp,fr))continue;
  AtlasImage img; if(!readPng(m_assets.resolve("block/"+k+".png"),img)||!img.width||img.height<img.width*2)continue;
  const uint32_t n=img.height/img.width; const Tint*tint=findTint(k);
  TileAnim a; a.tile=kv.second; a.interp=interp; a.frames.resize(n);
  for(uint32_t i=0;i<n;i++)makeTile(img,i,tint,a.frames[i].data());
  if(fr.empty())for(uint32_t i=0;i<n;i++)a.seq.push_back({(int)i,ft});
  else for(auto&f:fr)a.seq.push_back({(int)((uint32_t)std::max(0,f.first)%n),f.second>0?f.second:ft});
  for(auto&s:a.seq)a.total+=s.second; if(a.total<=0||a.seq.size()<2)continue;
  m_anims.push_back(std::move(a));
 }
}
void TexturePack::collectAnimationUpdates(double ticks,std::vector<TileUpdate>&out){
 for(TileAnim&a:m_anims){
  const double t=std::fmod(ticks,(double)a.total); int acc=0; size_t si=0; for(;si<a.seq.size();si++){ if(t<acc+a.seq[si].second)break; acc+=a.seq[si].second; } if(si>=a.seq.size())si=a.seq.size()-1;
  const int cur=a.seq[si].first;
  if(!a.interp){ if(a.lastKey==cur)continue; a.lastKey=cur; TileUpdate u; u.id=a.tile; std::copy(a.frames[cur].begin(),a.frames[cur].end(),u.rgba); out.push_back(u); }
  else{
   const int key=(int)std::floor(t); if(a.lastKey==key)continue; a.lastKey=key;      // blend towards the next frame, updated every tick
   const int nxt=a.seq[(si+1)%a.seq.size()].first; const float f=float(t-acc)/float(a.seq[si].second);
   TileUpdate u; u.id=a.tile; for(int i=0;i<1024;i++)u.rgba[i]=(uint8_t)std::lround(a.frames[cur][i]*(1.f-f)+a.frames[nxt][i]*f); out.push_back(u);
  }
 }
}
