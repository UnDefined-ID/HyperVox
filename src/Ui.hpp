#pragma once
#include <algorithm>
#include <string>
#include <vector>
#include "Registry.hpp"
#include "UiAtlas.hpp"
#include "UiTypes.hpp"

struct UiInput{
 float mx=0,my=0;                     // mouse in framebuffer pixels
 bool lclick=false,rclick=false,mclick=false,ldown=false,shift=false;
 float wheel=0; std::string typed; bool backspace=false; double time=0;
};

constexpr uint32_t COL_WHITE=0xFFFFFFFFu, COL_GRAY=0xFFA0A0A0u, COL_DARK=0xFF404040u, COL_YELLOW=0xFFFFFFA0u;

// Immediate-mode GUI.  Layout coordinates are "gui pixels" (an integer number of screen pixels each, like vanilla).
class Ui{
public:
 void begin(float fbW,float fbH,const UiAtlas*atlas,const UiInput&in,int forceScale=0);   // forceScale 0 = automatic menu scale
 static int autoScale(float fbW,float fbH){return std::max(1,std::min((int)(fbW/320.f),(int)(fbH/240.f)));}
 static int maxScale(float fbW,float fbH){return std::max(1,std::min((int)(fbW/256.f),(int)(fbH/192.f)));}
 float W()const{return m_w;} float H()const{return m_h;} int scale()const{return m_scale;}
 float mx()const{return m_mx;} float my()const{return m_my;}
 const UiInput& input()const{return m_in;}
 std::vector<UiVertex> verts; size_t quads()const{return verts.size()/4;}

 // drawing
 void rect(float x,float y,float w,float h,uint32_t col);
 void sprite(const Sprite&s,float x,float y,float w,float h,uint32_t col=COL_WHITE);
 void region(const Sprite&s,int sx,int sy,int sw,int sh,float x,float y,float w,float h,uint32_t col=COL_WHITE);   // sub-rectangle in sprite pixels
 void nine(const Sprite&s,float refW,float border,float x,float y,float w,float h,uint32_t col=COL_WHITE);
 void tiled(const Sprite&s,float x,float y,float w,float h,float tile,uint32_t col=COL_WHITE);
 float text(const std::string&s,float x,float y,uint32_t col=COL_WHITE,bool shadow=true);
 float textWidth(const std::string&s)const;
 void textCentered(const std::string&s,float cx,float y,uint32_t col=COL_WHITE,bool shadow=true);
 void bigText(const std::string&s,float cx,float y,float k,uint32_t col);                  // text scaled by k, centered
 void icon(ItemId id,float x,float y);                                                    // 16x16 slot content
 void blockIcon(BlockId b,float x,float y,float size);
 void quad4(float x0,float y0,float u0,float v0,float x1,float y1,float u1,float v1,float x2,float y2,float u2,float v2,float x3,float y3,float u3,float v3,uint32_t col);
 void tooltip(const std::string&s);                                                       // near the mouse, drawn immediately

 // widgets
 bool hover(float x,float y,float w,float h)const{return m_mx>=x&&m_mx<x+w&&m_my>=y&&m_my<y+h;}
 bool button(float x,float y,float w,float h,const std::string&label,bool enabled=true);
 bool slider(int id,float x,float y,float w,float h,const std::string&label,float&value01);   // true when value changed
 bool textField(int id,float x,float y,float w,float h,std::string&text,size_t maxLen,int&focus);
 bool sliderReleased()const{return m_released;}
 bool dragging()const{return m_drag>=0;}
private:
 void emit(float x0,float y0,float x1,float y1,float u0,float v0,float u1,float v1,uint32_t col);
 const UiAtlas* m_atlas=nullptr; UiInput m_in; float m_fbw=1,m_fbh=1,m_w=1,m_h=1,m_mx=0,m_my=0; int m_scale=1; int m_drag=-1; bool m_released=false,m_wasDown=false;
};
