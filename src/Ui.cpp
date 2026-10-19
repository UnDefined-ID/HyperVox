#include "Ui.hpp"
#include <algorithm>
#include <cmath>

void Ui::begin(float fbW,float fbH,const UiAtlas*atlas,const UiInput&in,int forceScale){
 m_atlas=atlas; m_in=in; m_fbw=fbW; m_fbh=fbH;
 m_scale=forceScale>0?std::max(1,std::min(forceScale,maxScale(fbW,fbH))):autoScale(fbW,fbH);
 m_w=fbW/float(m_scale); m_h=fbH/float(m_scale); m_mx=in.mx/float(m_scale); m_my=in.my/float(m_scale);
 verts.clear(); m_released=m_wasDown&&!in.ldown; m_wasDown=in.ldown; if(m_released||!in.ldown)m_drag=-1;
}
void Ui::emit(float x0,float y0,float x1,float y1,float u0,float v0,float u1,float v1,uint32_t col){
 const float s=float(m_scale); x0=std::round(x0*s);y0=std::round(y0*s);x1=std::round(x1*s);y1=std::round(y1*s);
 verts.push_back({x0,y0,u0,v0,col});verts.push_back({x1,y0,u1,v0,col});verts.push_back({x1,y1,u1,v1,col});verts.push_back({x0,y1,u0,v1,col});
}
void Ui::quad4(float x0,float y0,float u0,float v0,float x1,float y1,float u1,float v1,float x2,float y2,float u2,float v2,float x3,float y3,float u3,float v3,uint32_t col){
 const float s=float(m_scale);
 verts.push_back({std::round(x0*s),std::round(y0*s),u0,v0,col});verts.push_back({std::round(x1*s),std::round(y1*s),u1,v1,col});
 verts.push_back({std::round(x2*s),std::round(y2*s),u2,v2,col});verts.push_back({std::round(x3*s),std::round(y3*s),u3,v3,col});
}
void Ui::rect(float x,float y,float w,float h,uint32_t col){const Sprite&s=m_atlas->white();const float u=(s.u0+s.u1)*.5f,v=(s.v0+s.v1)*.5f;emit(x,y,x+w,y+h,u,v,u,v,col);}
void Ui::sprite(const Sprite&s,float x,float y,float w,float h,uint32_t col){emit(x,y,x+w,y+h,s.u0,s.v0,s.u1,s.v1,col);}
void Ui::region(const Sprite&s,int sx,int sy,int sw,int sh,float x,float y,float w,float h,uint32_t col){
 const float du=(s.u1-s.u0)/float(std::max(1,s.w)),dv=(s.v1-s.v0)/float(std::max(1,s.h));
 emit(x,y,x+w,y+h,s.u0+du*float(sx),s.v0+dv*float(sy),s.u0+du*float(sx+sw),s.v0+dv*float(sy+sh),col);
}
void Ui::nine(const Sprite&s,float refW,float b,float x,float y,float w,float h,uint32_t col){
 if(!s.valid()){rect(x,y,w,h,col);return;}
 const float f=refW/float(s.w); const int sb=std::max(1,(int)std::lround(b/f)); const float sx[4]={0,float(sb),float(s.w-sb),float(s.w)}, sy[4]={0,float(sb),float(s.h-sb),float(s.h)};
 const float dx[4]={x,x+b,x+w-b,x+w}, dy[4]={y,y+b,y+h-b,y+h};
 const float du=(s.u1-s.u0)/float(s.w),dv=(s.v1-s.v0)/float(s.h);
 for(int j=0;j<3;j++)for(int i=0;i<3;i++)emit(dx[i],dy[j],dx[i+1],dy[j+1],s.u0+du*sx[i],s.v0+dv*sy[j],s.u0+du*sx[i+1],s.v0+dv*sy[j+1],col);
}
void Ui::tiled(const Sprite&s,float x,float y,float w,float h,float tile,uint32_t col){
 for(float yy=y;yy<y+h;yy+=tile)for(float xx=x;xx<x+w;xx+=tile){
  const float tw=std::min(tile,x+w-xx),th=std::min(tile,y+h-yy); const float fu=tw/tile,fv=th/tile;
  emit(xx,yy,xx+tw,yy+th,s.u0,s.v0,s.u0+(s.u1-s.u0)*fu,s.v0+(s.v1-s.v0)*fv,col);
 }
}
float Ui::textWidth(const std::string&str)const{
 float w=0; for(char ch:str){const int c=(unsigned char)ch; w+=(c==' ')?4.f:float(m_atlas->glyph(c).w)+1.f;} return std::max(0.f,w-1.f);
}
float Ui::text(const std::string&str,float x,float y,uint32_t col,bool shadow){
 if(shadow){ const uint32_t sc=(col&0xFF000000u)|((col&0x00FCFCFCu)>>2); text(str,x+1,y+1,sc,false); }
 for(char ch:str){
  const int c=(unsigned char)ch;
  if(c==' '){x+=4;continue;}
  const Sprite&g=m_atlas->glyph(c); emit(x,y,x+float(g.w),y+float(g.h),g.u0,g.v0,g.u1,g.v1,col); x+=float(g.w)+1.f;
 }
 return x;
}
void Ui::textCentered(const std::string&s,float cx,float y,uint32_t col,bool shadow){text(s,cx-textWidth(s)*0.5f,y,col,shadow);}
void Ui::bigText(const std::string&str,float cx,float y,float k,uint32_t col){
 float w=0; for(char ch:str){const int c=(unsigned char)ch;w+=(c==' ')?4.f:float(m_atlas->glyph(c).w)+1.f;} w=(w-1.f)*k; float x=cx-w*0.5f;
 const uint32_t sc=(col&0xFF000000u)|((col&0x00FCFCFCu)>>2);
 for(int pass=0;pass<2;pass++){
  float px=x; const float o=pass==0?k:0.f;
  for(char ch:str){const int c=(unsigned char)ch; if(c==' '){px+=4*k;continue;} const Sprite&g=m_atlas->glyph(c);
   emit(px+o,y+o,px+o+float(g.w)*k,y+o+float(g.h)*k,g.u0,g.v0,g.u1,g.v1,pass==0?sc:col); px+=(float(g.w)+1.f)*k; }
 }
}
void Ui::blockIcon(BlockId b,float x,float y,float S){
 const BlockInfo&bi=Registry::get().info(b); const Sprite&top=m_atlas->tile(bi.texture[1]),&left=m_atlas->tile(bi.texture[5]),&right=m_atlas->tile(bi.texture[3]);
 const float cx=x+S*.5f,q=S*.25f;
 const float f=float(bi.height16)/16.f,dy=(1.f-f)*2.f*q;                      // blocks shorter than a cube (enchanting table): top sits lower, sides are cropped
 const float lv=left.v1-(left.v1-left.v0)*f,rv=right.v1-(right.v1-right.v0)*f;
 quad4(cx,y+dy,top.u0,top.v0, x+S,y+q+dy,top.u1,top.v0, cx,y+2*q+dy,top.u1,top.v1, x,y+q+dy,top.u0,top.v1, COL_WHITE);                                   // top
 quad4(x,y+q+dy,left.u0,lv, cx,y+2*q+dy,left.u1,lv, cx,y+S,left.u1,left.v1, x,y+3*q,left.u0,left.v1, rgba(204,204,204));                                 // left (front)
 quad4(cx,y+2*q+dy,right.u0,rv, x+S,y+q+dy,right.u1,rv, x+S,y+3*q,right.u1,right.v1, cx,y+S,right.u0,right.v1, rgba(153,153,153));                     // right
}
void Ui::icon(ItemId id,float x,float y){
 if(!Registry::get().validItem(id))return;
 if(id<kItemBase){ const BlockInfo&bi=Registry::get().info((BlockId)id); if(bi.flatIcon){sprite(m_atlas->tile(bi.texture[2]),x,y,16,16);return;} blockIcon((BlockId)id,x,y,16.f);return;}
 sprite(m_atlas->plainItem(id-kItemBase),x,y,16,16);
}
void Ui::tooltip(const std::string&s){
 const float w=textWidth(s)+8,h=16; float x=m_mx+10,y=m_my-14; if(x+w>m_w-2)x=m_w-2-w; if(y<2)y=2; if(y+h>m_h-2)y=m_h-2-h;
 rect(x,y,w,h,rgba(16,0,16,240)); rect(x,y,w,1,rgba(60,20,120,255)); rect(x,y+h-1,w,1,rgba(60,20,120,255)); rect(x,y,1,h,rgba(60,20,120,255)); rect(x+w-1,y,1,h,rgba(60,20,120,255));
 text(s,x+4,y+4);
}

bool Ui::button(float x,float y,float w,float h,const std::string&label,bool enabled){
 const bool hv=enabled&&hover(x,y,w,h);
 nine(m_atlas->get(!enabled?"gui/sprites/widget/button_disabled":hv?"gui/sprites/widget/button_highlighted":"gui/sprites/widget/button"),200,3,x,y,w,h);
 textCentered(label,x+w*0.5f,y+(h-8)*0.5f,!enabled?COL_GRAY:hv?COL_YELLOW:rgba(224,224,224));
 return hv&&m_in.lclick;
}
bool Ui::slider(int id,float x,float y,float w,float h,const std::string&label,float&v){
 const bool hv=hover(x,y,w,h)||m_drag==id; bool changed=false;
 if(m_in.lclick&&hover(x,y,w,h))m_drag=id;
 if(m_drag==id&&m_in.ldown){const float nv=std::clamp((m_mx-x-4)/(w-8),0.f,1.f); if(nv!=v){v=nv;changed=true;}}
 nine(m_atlas->get(hv?"gui/sprites/widget/slider_highlighted":"gui/sprites/widget/slider"),200,1,x,y,w,h);
 nine(m_atlas->get(hv?"gui/sprites/widget/slider_handle_highlighted":"gui/sprites/widget/slider_handle"),8,1,x+v*(w-8),y,8,h);
 textCentered(label,x+w*0.5f,y+(h-8)*0.5f,hv?COL_YELLOW:rgba(224,224,224));
 return changed;
}
bool Ui::textField(int id,float x,float y,float w,float h,std::string&text,size_t maxLen,int&focus){
 if(m_in.lclick){ if(hover(x,y,w,h))focus=id; else if(focus==id)focus=-1; }
 const bool f=focus==id; bool changed=false;
 if(f){
  for(char c:m_in.typed)if(c>=32&&c<=126&&text.size()<maxLen){text+=c;changed=true;}
  if(m_in.backspace&&!text.empty()){text.pop_back();changed=true;}
 }
 nine(m_atlas->get(f?"gui/sprites/widget/text_field_highlighted":"gui/sprites/widget/text_field"),200,1,x,y,w,h);
 std::string shown=text; if(f&&((long long)(m_in.time*2.0)%2==0))shown+="_";
 while(!shown.empty()&&textWidth(shown)>w-8)shown.erase(shown.begin());
 this->text(shown,x+4,y+(h-8)*0.5f,rgba(224,224,224));
 return changed;
}
