#include "UiAtlas.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;

namespace{
constexpr uint32_t kUiCacheVersion=8, kPanoCacheVersion=2;
const char* kGuiSprites[]={
 "gui/sprites/widget/button","gui/sprites/widget/button_highlighted","gui/sprites/widget/button_disabled",
 "gui/sprites/widget/slider","gui/sprites/widget/slider_highlighted","gui/sprites/widget/slider_handle","gui/sprites/widget/slider_handle_highlighted",
 "gui/sprites/widget/text_field","gui/sprites/widget/text_field_highlighted",
 "gui/sprites/hud/hotbar","gui/sprites/hud/hotbar_selection",
 "gui/sprites/container/slot_highlight_back","gui/sprites/container/slot_highlight_front",
 "gui/sprites/container/creative_inventory/scroller","gui/sprites/container/creative_inventory/scroller_disabled",
 "gui/container/inventory","gui/container/creative_inventory/tab_items",
 "gui/menu_background","gui/menu_list_background","gui/header_separator","gui/footer_separator",
 "gui/sprites/world_list/join","gui/sprites/world_list/join_highlighted",
 "gui/sprites/transferable_list/select","gui/sprites/transferable_list/select_highlighted","gui/sprites/transferable_list/unselect","gui/sprites/transferable_list/unselect_highlighted",
 "misc/unknown_pack","gui/sprites/hud/heart/container","gui/sprites/hud/heart/full","gui/sprites/hud/heart/half","gui/sprites/hud/armor_full","gui/sprites/hud/armor_half","gui/sprites/hud/armor_empty","gui/sprites/hud/food_empty","gui/sprites/hud/food_full","gui/sprites/hud/food_half",
 "environment/celestial/sun","environment/celestial/moon/full_moon","environment/celestial/moon/waning_gibbous","environment/celestial/moon/third_quarter","environment/celestial/moon/waning_crescent","environment/celestial/moon/new_moon","environment/celestial/moon/waxing_crescent","environment/celestial/moon/first_quarter","environment/celestial/moon/waxing_gibbous",
 "gui/title/minecraft","gui/title/edition","gui/container/creative_inventory/tab_inventory",
 "gui/sprites/container/creative_inventory/tab_bottom_selected_1","gui/sprites/container/creative_inventory/tab_bottom_selected_7","gui/sprites/container/creative_inventory/tab_bottom_unselected_1","gui/sprites/container/creative_inventory/tab_bottom_unselected_7",
 "gui/sprites/container/creative_inventory/tab_top_selected_1","gui/sprites/container/creative_inventory/tab_top_selected_2","gui/sprites/container/creative_inventory/tab_top_selected_3","gui/sprites/container/creative_inventory/tab_top_selected_4","gui/sprites/container/creative_inventory/tab_top_selected_5","gui/sprites/container/creative_inventory/tab_top_selected_6","gui/sprites/container/creative_inventory/tab_top_selected_7",
 "gui/sprites/container/creative_inventory/tab_top_unselected_1","gui/sprites/container/creative_inventory/tab_top_unselected_2","gui/sprites/container/creative_inventory/tab_top_unselected_3","gui/sprites/container/creative_inventory/tab_top_unselected_4","gui/sprites/container/creative_inventory/tab_top_unselected_5","gui/sprites/container/creative_inventory/tab_top_unselected_6","gui/sprites/container/creative_inventory/tab_top_unselected_7",
};
struct Entry{ AtlasImage img; Sprite*out=nullptr; float scale=1.f; };

AtlasImage resample16(const AtlasImage&src){        // icons are always stored 16x16
 const uint32_t tile=std::min(src.width,src.height); AtlasImage o; o.width=o.height=16; o.rgba.assign(16*16*4,0);
 for(uint32_t y=0;y<16;y++)for(uint32_t x=0;x<16;x++){
  const uint32_t x0=x*tile/16,x1=std::max(x0+1,(x+1)*tile/16),y0=y*tile/16,y1=std::max(y0+1,(y+1)*tile/16);
  uint32_t acc[4]={0,0,0,0},n=0,aw=0;
  for(uint32_t sy=y0;sy<y1&&sy<src.height;sy++)for(uint32_t sx=x0;sx<x1&&sx<src.width;sx++){const uint8_t*s=&src.rgba[((size_t)sy*src.width+sx)*4];acc[0]+=s[0]*s[3];acc[1]+=s[1]*s[3];acc[2]+=s[2]*s[3];acc[3]+=s[3];aw+=s[3];n++;}
  uint8_t*d=&o.rgba[((size_t)y*16+x)*4]; if(n){ if(aw){d[0]=(uint8_t)(acc[0]/aw);d[1]=(uint8_t)(acc[1]/aw);d[2]=(uint8_t)(acc[2]/aw);} d[3]=(uint8_t)(acc[3]/n); }
 }
 return o;
}
}

bool UiAtlas::readCache(const fs::path&f,uint64_t sig){
 std::ifstream in(f,std::ios::binary); if(!in)return false;
 char magic[4]; in.read(magic,4); if(!in||std::memcmp(magic,"FVKU",4)!=0)return false;
 uint32_t ver=0,w=0,h=0; uint64_t s=0; in.read((char*)&ver,4); in.read((char*)&s,8); in.read((char*)&w,4); in.read((char*)&h,4);
 if(!in||ver!=kUiCacheVersion||s!=sig||w==0||h==0||w>8192||h>8192)return false;
 AtlasImage img; img.width=w; img.height=h; img.rgba.resize((size_t)w*h*4); in.read((char*)img.rgba.data(),(std::streamsize)img.rgba.size()); if(!in)return false;
 Sprite white; in.read((char*)&white,sizeof white);
 uint32_t n=0; in.read((char*)&n,4); if(!in||n>100000)return false;
 std::unordered_map<std::string,Sprite> sp;
 for(uint32_t i=0;i<n;i++){unsigned char l=0;in.read((char*)&l,1);std::string k(l,'\0');in.read(&k[0],l);Sprite s2;in.read((char*)&s2,sizeof s2);if(!in)return false;sp.emplace(k,s2);}
 uint32_t nt=0,ni=0; in.read((char*)&nt,4); if(!in||nt>100000)return false; std::vector<Sprite> tiles(nt); in.read((char*)tiles.data(),(std::streamsize)(nt*sizeof(Sprite)));
 in.read((char*)&ni,4); if(!in||ni>100000)return false; std::vector<Sprite> items(ni); in.read((char*)items.data(),(std::streamsize)(ni*sizeof(Sprite)));
 Sprite glyph[95]; in.read((char*)glyph,sizeof glyph); if(!in)return false;
 m_img=std::move(img); m_white=white; m_sprites=std::move(sp); m_tiles=std::move(tiles); m_items=std::move(items); std::copy(glyph,glyph+95,m_glyph); return true;
}
void UiAtlas::writeCache(const fs::path&f,uint64_t sig)const{
 std::error_code ec; fs::create_directories(f.parent_path(),ec); const auto tmp=fs::path(f).concat(".tmp");
 { std::ofstream out(tmp,std::ios::binary|std::ios::trunc); if(!out)return;
   const uint32_t ver=kUiCacheVersion,w=m_img.width,h=m_img.height; out.write("FVKU",4); out.write((const char*)&ver,4); out.write((const char*)&sig,8); out.write((const char*)&w,4); out.write((const char*)&h,4);
   out.write((const char*)m_img.rgba.data(),(std::streamsize)m_img.rgba.size()); out.write((const char*)&m_white,sizeof m_white);
   const uint32_t n=(uint32_t)m_sprites.size(); out.write((const char*)&n,4);
   for(auto&kv:m_sprites){unsigned char l=(unsigned char)std::min<size_t>(kv.first.size(),255);out.write((const char*)&l,1);out.write(kv.first.data(),l);out.write((const char*)&kv.second,sizeof(Sprite));}
   const uint32_t nt=(uint32_t)m_tiles.size(),ni=(uint32_t)m_items.size();
   out.write((const char*)&nt,4); out.write((const char*)m_tiles.data(),(std::streamsize)(nt*sizeof(Sprite)));
   out.write((const char*)&ni,4); out.write((const char*)m_items.data(),(std::streamsize)(ni*sizeof(Sprite)));
   out.write((const char*)m_glyph,sizeof m_glyph); if(!out)return; }
 fs::rename(tmp,f,ec); if(ec){fs::remove(f,ec);fs::rename(tmp,f,ec);}
}

const Sprite& UiAtlas::get(const std::string&k)const{auto it=m_sprites.find(k);return it==m_sprites.end()?m_white:it->second;}

bool UiAtlas::load(const AssetPaths&assets,const TexturePack&pack,const fs::path&cacheDir){
 const uint64_t sig=assets.signature(kUiCacheVersion)^((uint64_t)Registry::get().blockCount()*0x9E3779B97F4A7C15ull)^((uint64_t)Registry::get().plainItemCount()<<20);
 const fs::path cacheFile=cacheDir/("ui_"+safeFileName(assets.packName)+".cache");
 if(readCache(cacheFile,sig)){m_fromCache=true;return true;}
 const Registry&R=Registry::get();
 std::vector<Entry> entries; entries.reserve(2000);
 m_sprites.clear(); m_items.assign(R.plainItemCount(),Sprite{});
 // solid white pixel
 { Entry e; e.img.width=e.img.height=2; e.img.rgba.assign(16,255); e.out=&m_white; entries.push_back(std::move(e)); }
 for(const char*k:kGuiSprites){
  AtlasImage img; if(!readPng(assets.resolve(std::string(k)+".png"),img)||!img.width)continue;
  if(std::strncmp(k,"environment/celestial",21)==0)for(size_t p=0;p+3<img.rgba.size();p+=4){ const int al=std::max({(int)img.rgba[p],(int)img.rgba[p+1],(int)img.rgba[p+2]}); for(int c=0;c<3;c++)img.rgba[p+c]=(uint8_t)(al?std::min(255,img.rgba[p+c]*255/al):0); img.rgba[p+3]=(uint8_t)al; }   // sun/moon are drawn additively in vanilla: brightness becomes alpha, black = invisible
  Entry e; e.img=std::move(img); e.out=&m_sprites[k]; entries.push_back(std::move(e));
 }
 // font: 8x8 cells in a 16x16 grid, code point = row*16+col; width = last non-empty column + 1
 { AtlasImage f; if(readPng(assets.resolve("font/ascii.png"),f)&&f.width>=128&&f.height>=128){
    const uint32_t cw=f.width/16,ch=f.height/16;
    for(int c=32;c<=126;c++){
     const uint32_t cx=(c%16)*cw,cy=(c/16)*ch; int last=-1;
     for(uint32_t x=0;x<cw;x++)for(uint32_t y=0;y<ch;y++)if(f.rgba[((size_t)(cy+y)*f.width+cx+x)*4+3]>0)last=(int)x;
     Entry e; e.img.width=(uint32_t)std::max(1,last+1); e.img.height=ch; if(c==32)e.img.width=1;
     e.img.rgba.assign((size_t)e.img.width*ch*4,0);
     if(c!=32)for(uint32_t y=0;y<ch;y++)for(uint32_t x=0;x<e.img.width;x++)std::memcpy(&e.img.rgba[((size_t)y*e.img.width+x)*4],&f.rgba[((size_t)(cy+y)*f.width+cx+x)*4],4);
     e.out=&m_glyph[c-32]; e.scale=8.0f/float(cw); entries.push_back(std::move(e));
    }
 } }
 // block icons: only the faces used by the isometric inventory icons (top, left=front, right)
 std::vector<uint16_t> need;
 for(BlockId b=1;b<R.blockCount();b++){const BlockInfo&bi=R.info(b);need.push_back(bi.texture[1]);need.push_back(bi.texture[3]);need.push_back(bi.texture[5]);}
 std::sort(need.begin(),need.end()); need.erase(std::unique(need.begin(),need.end()),need.end());
 uint16_t maxTex=0; for(auto t:need)maxTex=std::max(maxTex,t); m_tiles.assign((size_t)maxTex+1,Sprite{});
 const uint32_t asz=pack.atlasSize();
 for(uint16_t t:need){
  Entry e; e.img.width=e.img.height=16; e.img.rgba.resize(16*16*4); const uint8_t*src=pack.tilePixels(t);
  for(int y=0;y<16;y++)std::memcpy(&e.img.rgba[(size_t)y*16*4],src+(size_t)y*asz*4,16*4);
  e.out=&m_tiles[t]; entries.push_back(std::move(e));
 }
 for(size_t i=0;i<R.plainItemCount();i++){
  AtlasImage img; const std::string&ic=R.plainItem(i).iconName; if(!readPng(assets.resolve("item/"+ic+".png"),img)||!img.width)continue;
  if(ic=="leather_helmet"||ic=="leather_chestplate"||ic=="leather_leggings"||ic=="leather_boots"||ic=="leather_horse_armor"){
   // vanilla draws leather armour as the grey base layer dyed with the default leather colour (#A06540) plus an undyed overlay layer
   for(size_t p=0;p+3<img.rgba.size();p+=4){ img.rgba[p]=(uint8_t)(img.rgba[p]*160/255); img.rgba[p+1]=(uint8_t)(img.rgba[p+1]*101/255); img.rgba[p+2]=(uint8_t)(img.rgba[p+2]*64/255); }
   AtlasImage ov; if(readPng(assets.resolve("item/"+ic+"_overlay.png"),ov)&&ov.width==img.width&&ov.height==img.height)
    for(size_t p=0;p+3<img.rgba.size();p+=4){ const int a=ov.rgba[p+3]; if(!a)continue; const int ia=255-a, oa=img.rgba[p+3]*ia/255, na=a+oa; if(na<=0)continue;
     for(int k=0;k<3;k++)img.rgba[p+k]=(uint8_t)((ov.rgba[p+k]*a+img.rgba[p+k]*oa)/na); img.rgba[p+3]=(uint8_t)std::min(255,na); }
  }
  Entry e; e.img=resample16(img); e.out=&m_items[i]; entries.push_back(std::move(e));
 }
 // shelf packing with a 1px extruded gutter (nearest sampling never bleeds into neighbours)
 std::sort(entries.begin(),entries.end(),[](const Entry&a,const Entry&b){return a.img.height>b.img.height;});
 const uint32_t W=2048; uint32_t x=0,y=0,rowH=0; std::vector<std::array<uint32_t,2>> pos(entries.size());
 for(size_t i=0;i<entries.size();i++){
  const uint32_t w=entries[i].img.width+2,h=entries[i].img.height+2;
  if(x+w>W){y+=rowH;x=0;rowH=0;} pos[i]={x,y}; x+=w; rowH=std::max(rowH,h);
 }
 uint32_t H=256; while(H<y+rowH)H*=2; if(H>8192)return false;
 m_img.width=W; m_img.height=H; m_img.rgba.assign((size_t)W*H*4,0);
 for(size_t i=0;i<entries.size();i++){
  const AtlasImage&s=entries[i].img; const uint32_t ox=pos[i][0]+1,oy=pos[i][1]+1;
  for(int dy=-1;dy<=(int)s.height;dy++)for(int dx=-1;dx<=(int)s.width;dx++){
   const uint32_t sx=(uint32_t)std::clamp(dx,0,(int)s.width-1),sy=(uint32_t)std::clamp(dy,0,(int)s.height-1);
   std::memcpy(&m_img.rgba[((size_t)(oy+dy)*W+(ox+dx))*4],&s.rgba[((size_t)sy*s.width+sx)*4],4);
  }
  Sprite sp; sp.w=std::max(1,(int)std::lround(float(s.width)*entries[i].scale)); sp.h=std::max(1,(int)std::lround(float(s.height)*entries[i].scale)); sp.u0=float(ox)/float(W); sp.v0=float(oy)/float(H); sp.u1=float(ox+s.width)/float(W); sp.v1=float(oy+s.height)/float(H);
  *entries[i].out=sp;
 }
 // glyph advance = width+1 is computed from Sprite::w; a missing font leaves glyphs invalid
 writeCache(cacheFile,sig);
 return true;
}

bool loadPanorama(const AssetPaths&assets,const fs::path&cacheDir,PanoramaImage&out){
 const uint32_t S=512; const uint64_t sig=assets.signature(kPanoCacheVersion);
 const fs::path cacheFile=cacheDir/("panorama_"+safeFileName(assets.packName)+".cache");
 { std::ifstream in(cacheFile,std::ios::binary); char m[4]; uint32_t ver=0,sz=0; uint64_t s=0;
   if(in){in.read(m,4);in.read((char*)&ver,4);in.read((char*)&s,8);in.read((char*)&sz,4);
    if(in&&std::memcmp(m,"FVKP",4)==0&&ver==kPanoCacheVersion&&s==sig&&sz==S){ out.size=S; out.rgba.resize((size_t)S*S*6*4); in.read((char*)out.rgba.data(),(std::streamsize)out.rgba.size()); if(in)return true; }} }
 out=PanoramaImage{}; std::vector<uint8_t> px((size_t)S*S*6*4,255);
 for(int f=0;f<6;f++){
  AtlasImage img; if(!readPng(assets.resolve("gui/title/background/panorama_"+std::to_string(f)+".png"),img)||img.width<2||img.height<2)return false;
  for(uint32_t y=0;y<S;y++)for(uint32_t x=0;x<S;x++){
   const uint32_t x0=x*img.width/S,x1=std::max(x0+1,(x+1)*img.width/S),y0=y*img.height/S,y1=std::max(y0+1,(y+1)*img.height/S);
   uint32_t a[3]={0,0,0},n=0; for(uint32_t sy=y0;sy<y1;sy++)for(uint32_t sx=x0;sx<x1;sx++){const uint8_t*s=&img.rgba[((size_t)sy*img.width+sx)*4];a[0]+=s[0];a[1]+=s[1];a[2]+=s[2];n++;}
   uint8_t*d=&px[(((size_t)f*S+y)*S+x)*4]; d[0]=(uint8_t)(a[0]/n);d[1]=(uint8_t)(a[1]/n);d[2]=(uint8_t)(a[2]/n);d[3]=255;
  }
 }
 out.size=S; out.rgba=std::move(px);
 std::error_code ec; fs::create_directories(cacheDir,ec); const auto tmp=fs::path(cacheFile).concat(".tmp");
 { std::ofstream o(tmp,std::ios::binary|std::ios::trunc); if(o){const uint32_t ver=kPanoCacheVersion; o.write("FVKP",4);o.write((const char*)&ver,4);o.write((const char*)&sig,8);o.write((const char*)&S,4);o.write((const char*)out.rgba.data(),(std::streamsize)out.rgba.size());}}
 fs::rename(tmp,cacheFile,ec); if(ec){fs::remove(cacheFile,ec);fs::rename(tmp,cacheFile,ec);}
 return true;
}
