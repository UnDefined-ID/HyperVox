#include "Png.hpp"
#include <png.h>
#include <cstdio>

bool readPng(const std::filesystem::path&p,AtlasImage& out){
 FILE*f=fopen(p.string().c_str(),"rb"); if(!f)return false;
 png_structp ps=png_create_read_struct(PNG_LIBPNG_VER_STRING,nullptr,nullptr,nullptr);
 png_infop pi=ps?png_create_info_struct(ps):nullptr;
 if(!ps||!pi||setjmp(png_jmpbuf(ps))){ if(ps)png_destroy_read_struct(&ps,&pi,nullptr); fclose(f); return false; }
 png_init_io(ps,f); png_read_info(ps,pi);
 png_uint_32 w,h; int bit,ct;
 png_get_IHDR(ps,pi,&w,&h,&bit,&ct,nullptr,nullptr,nullptr);
 if(bit==16)png_set_strip_16(ps);
 if(ct==PNG_COLOR_TYPE_PALETTE)png_set_palette_to_rgb(ps);
 if(ct==PNG_COLOR_TYPE_GRAY&&bit<8)png_set_expand_gray_1_2_4_to_8(ps);
 if(png_get_valid(ps,pi,PNG_INFO_tRNS))png_set_tRNS_to_alpha(ps);
 png_set_interlace_handling(ps);
 if(ct==PNG_COLOR_TYPE_GRAY||ct==PNG_COLOR_TYPE_GRAY_ALPHA)png_set_gray_to_rgb(ps);
 if(ct==PNG_COLOR_TYPE_RGB||ct==PNG_COLOR_TYPE_GRAY||ct==PNG_COLOR_TYPE_PALETTE)png_set_filler(ps,0xff,PNG_FILLER_AFTER);
 png_read_update_info(ps,pi);
 out.width=w; out.height=h; out.rgba.assign((size_t)w*h*4,0);
 std::vector<png_bytep> rows(h);
 for(size_t y=0;y<h;y++)rows[y]=out.rgba.data()+y*w*4;
 png_read_image(ps,rows.data());
 png_destroy_read_struct(&ps,&pi,nullptr); fclose(f);
 return true;
}
