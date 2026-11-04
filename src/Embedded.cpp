#include "Embedded.hpp"
#include <cstring>
// GNU assembler .incbin: the files are pulled in by the linker, no generated source needed (Linux / MinGW).
#if defined(__GNUC__)&&!defined(__APPLE__)&&defined(HV_SHADER_DIR)
#define HV_INCBIN(sym,file) __asm__(".section .rodata\n.balign 16\n.global hv_" #sym "_b\nhv_" #sym "_b:\n.incbin \"" file "\"\n.global hv_" #sym "_e\nhv_" #sym "_e:\n.byte 0\n.previous\n"); extern "C" const unsigned char hv_##sym##_b[],hv_##sym##_e[];
HV_INCBIN(bv,HV_SHADER_DIR "/block.vert.spv") HV_INCBIN(bf,HV_SHADER_DIR "/block.frag.spv")
HV_INCBIN(uv,HV_SHADER_DIR "/ui.vert.spv")    HV_INCBIN(uf,HV_SHADER_DIR "/ui.frag.spv")
HV_INCBIN(lv,HV_SHADER_DIR "/line.vert.spv")  HV_INCBIN(lf,HV_SHADER_DIR "/line.frag.spv")
#ifdef HV_ASSETS_ZIP
HV_INCBIN(assets,HV_ASSETS_ZIP)
#endif
#ifdef HV_SOUNDS_ZIP
HV_INCBIN(sounds,HV_SOUNDS_ZIP)
#endif
#ifdef HV_BLOCKJSON
HV_INCBIN(blockjson,HV_BLOCKJSON)
#endif
#define B(s) EmbeddedBlob{hv_##s##_b,size_t(hv_##s##_e-hv_##s##_b)}
EmbeddedBlob embeddedBlob(const char*n){
 if(!std::strcmp(n,"block.vert.spv"))return B(bv); if(!std::strcmp(n,"block.frag.spv"))return B(bf);
 if(!std::strcmp(n,"ui.vert.spv"))return B(uv);    if(!std::strcmp(n,"ui.frag.spv"))return B(uf);
 if(!std::strcmp(n,"line.vert.spv"))return B(lv);  if(!std::strcmp(n,"line.frag.spv"))return B(lf);
#ifdef HV_ASSETS_ZIP
 if(!std::strcmp(n,"assets"))return B(assets);
#endif
#ifdef HV_SOUNDS_ZIP
 if(!std::strcmp(n,"sounds"))return B(sounds);
#endif
#ifdef HV_BLOCKJSON
 if(!std::strcmp(n,"block.json"))return B(blockjson);
#endif
 return {};
}
#else
EmbeddedBlob embeddedBlob(const char*){ return {}; }
#endif
