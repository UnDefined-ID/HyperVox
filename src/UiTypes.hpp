#pragma once
#include <cstdint>
struct UiVertex{ float x,y,u,v; uint32_t rgba; };       // pixel coordinates, packed R | G<<8 | B<<16 | A<<24
constexpr uint32_t rgba(uint32_t r,uint32_t g,uint32_t b,uint32_t a=255){return r|(g<<8)|(b<<16)|(a<<24);}
