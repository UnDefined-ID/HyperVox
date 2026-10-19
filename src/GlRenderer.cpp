#include "GlRenderer.hpp"
#include "Mips.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <string>

#define HV_DEF(R,N,A) R (HV_GLCALL *hvgl##N) A=nullptr;
HV_GL_FUNCS(HV_DEF)
#undef HV_DEF
void (HV_GLCALL *hvglClipControl)(HVglenum,HVglenum)=nullptr;

bool hvLoadGL(){
 bool ok=true;
#define HV_LOAD(R,N,A) hvgl##N=reinterpret_cast<R (HV_GLCALL*)A>(glfwGetProcAddress("gl" #N)); if(!hvgl##N)ok=false;
 HV_GL_FUNCS(HV_LOAD)
#undef HV_LOAD
 hvglClipControl=reinterpret_cast<void (HV_GLCALL*)(HVglenum,HVglenum)>(glfwGetProcAddress("glClipControl"));
 return ok;
}

namespace{
const char* kBlockVS=R"(#version 330 core
layout(location=0) in uint inA;
layout(location=1) in uint inB;
uniform mat4 uVP; uniform vec3 uOrigin;
out vec2 uv; flat out uint texId; flat out float shade;
const float SHADE[6]=float[6](0.431,1.0,0.686,0.686,0.843,0.843);
void main(){
 vec3 p=vec3(float(inA&511u),float(((inA>>18)&4095u)|((inB>>31)<<12)),float((inA>>9)&511u))/16.0+uOrigin;
 gl_Position=uVP*vec4(p,1.0);
 uv=vec2(float(inB&511u),float((inB>>9)&511u))/16.0;
 texId=(inB>>18)&4095u;
 shade=SHADE[((inA>>30)&3u)|(((inB>>30)&1u)<<2)];
})";
const char* kBlockFS=R"(#version 330 core
in vec2 uv; flat in uint texId; flat in float shade;
out vec4 outColor; uniform sampler2D atlas;
// No hardware mip maps here (some drivers mis-sample them): far away, a handful of taps inside the block's own tile are averaged instead.
void main(){
 const float TILES=64.0; float id=float(texId);
 vec2 cell=vec2(mod(id,TILES),floor(id/TILES));
 vec2 dx=dFdx(uv),dy=dFdy(uv);                                  // tiles per pixel
 float fp=max(length(dx),length(dy))*16.0;                      // texels per pixel
 vec4 c;
 if(fp<1.0){ c=textureLod(atlas,(cell+fract(uv))/TILES,0.0); }
 else{
  const vec2 O[8]=vec2[8](vec2(-0.375,-0.125),vec2(-0.125,0.375),vec2(0.125,-0.375),vec2(0.375,0.125),vec2(-0.375,0.375),vec2(0.375,-0.375),vec2(-0.125,-0.125),vec2(0.125,0.125));
  vec3 rgb=vec3(0.0); float a=0.0;
  for(int i=0;i<8;i++){ vec2 u=uv+O[i].x*dx+O[i].y*dy; vec4 s=textureLod(atlas,(cell+fract(u))/TILES,0.0); rgb+=s.rgb*s.a; a+=s.a; }
  c=vec4(rgb/max(a,1e-4),a/8.0);
 }
 if(c.a<0.1) discard;
 outColor=vec4(c.rgb*shade,c.a);
})";
const char* kUiVS=R"(#version 330 core
layout(location=0) in vec2 inPos; layout(location=1) in vec2 inUV; layout(location=2) in vec4 inColor;
uniform mat4 uVP; out vec2 uv; out vec4 col;
void main(){ gl_Position=uVP*vec4(inPos,0.0,1.0); uv=inUV; col=vec4(pow(inColor.rgb,vec3(2.2)),inColor.a); })";
const char* kUiFS=R"(#version 330 core
in vec2 uv; in vec4 col; out vec4 outColor; uniform sampler2D atlas;
void main(){ vec4 c=texture(atlas,uv)*col; if(c.a<0.004) discard; outColor=c; })";
const char* kLineVS=R"(#version 330 core
layout(location=0) in vec3 inPos; uniform mat4 uVP;
void main(){ gl_Position=uVP*vec4(inPos,1.0); })";
const char* kLineFS=R"(#version 330 core
out vec4 outColor; void main(){ outColor=vec4(0.0,0.0,0.0,1.0); })";

HVgluint compile(HVglenum type,const char*src){
 HVgluint s=hvglCreateShader(type); hvglShaderSource(s,1,&src,nullptr); hvglCompileShader(s);
 HVglint ok=0; hvglGetShaderiv(s,HVGL_COMPILE_STATUS,&ok);
 if(!ok){ char log[1024]; HVglsizei n=0; hvglGetShaderInfoLog(s,sizeof log,&n,log); std::cerr<<"GLSL error: "<<log<<"\n"; hvglDeleteShader(s); return 0; }
 return s;
}
HVgluint program(const char*vs,const char*fs){
 HVgluint v=compile(HVGL_VERTEX_SHADER,vs),f=compile(HVGL_FRAGMENT_SHADER,fs); if(!v||!f)return 0;
 HVgluint p=hvglCreateProgram(); hvglAttachShader(p,v); hvglAttachShader(p,f); hvglLinkProgram(p); hvglDeleteShader(v); hvglDeleteShader(f);
 HVglint ok=0; hvglGetProgramiv(p,HVGL_LINK_STATUS,&ok);
 if(!ok){ char log[1024]; HVglsizei n=0; hvglGetProgramInfoLog(p,sizeof log,&n,log); std::cerr<<"GL link error: "<<log<<"\n"; hvglDeleteProgram(p); return 0; }
 return p;
}
inline uint64_t ckey(int cx,int cz){return (uint64_t(uint32_t(cx))<<32)|uint32_t(cz);}
bool visible(const Mat4&m,float x0,float y0,float z0,float x1,float y1,float z1,bool zeroToOne){
 int l=0,r=0,b=0,t=0,n=0,f=0;
 for(int i=0;i<8;i++){
  const float x=(i&1)?x1:x0,y=(i&2)?y1:y0,z=(i&4)?z1:z0;
  const float cx=m.m[0]*x+m.m[4]*y+m.m[8]*z+m.m[12],cy=m.m[1]*x+m.m[5]*y+m.m[9]*z+m.m[13],cz=m.m[2]*x+m.m[6]*y+m.m[10]*z+m.m[14],cw=m.m[3]*x+m.m[7]*y+m.m[11]*z+m.m[15];
  if(cx<-cw)l++; if(cx>cw)r++; if(cy<-cw)b++; if(cy>cw)t++;
  if(zeroToOne){ if(cz<0.f)n++; if(cz>cw)f++; } else { if(cz<-cw)n++; if(cz>cw)f++; }
 }
 return !(l==8||r==8||b==8||t==8||n==8||f==8);
}
}

bool GlRenderer::buildPrograms(){
 progBlock_=program(kBlockVS,kBlockFS); progUi_=program(kUiVS,kUiFS); progLine_=program(kLineVS,kLineFS);
 if(!progBlock_||!progUi_||!progLine_)return false;
 uBlockVP_=hvglGetUniformLocation(progBlock_,"uVP"); uBlockOrigin_=hvglGetUniformLocation(progBlock_,"uOrigin"); uBlockAtlas_=hvglGetUniformLocation(progBlock_,"atlas");
 uUiVP_=hvglGetUniformLocation(progUi_,"uVP"); uUiTex_=hvglGetUniformLocation(progUi_,"atlas"); uLineVP_=hvglGetUniformLocation(progLine_,"uVP");
 return true;
}
static HVgluint makeTex(const uint8_t*rgba,uint32_t w,uint32_t h,bool linear,bool mips=false){
 HVgluint t=0; hvglGenTextures(1,&t); hvglBindTexture(HVGL_TEXTURE_2D,t); hvglPixelStorei(HVGL_UNPACK_ALIGNMENT,1);
 if(mips){ std::vector<size_t> off; const std::vector<uint8_t> all=buildMipChain(rgba,w,h,5,off); for(int l=0;l<5;l++)hvglTexImage2D(HVGL_TEXTURE_2D,l,(HVglint)HVGL_SRGB8_ALPHA8,(HVglsizei)std::max(1u,w>>l),(HVglsizei)std::max(1u,h>>l),0,HVGL_RGBA,HVGL_UNSIGNED_BYTE,all.data()+off[l]); hvglTexParameteri(HVGL_TEXTURE_2D,HVGL_TEXTURE_MAX_LEVEL,4); }
 else hvglTexImage2D(HVGL_TEXTURE_2D,0,(HVglint)HVGL_SRGB8_ALPHA8,(HVglsizei)w,(HVglsizei)h,0,HVGL_RGBA,HVGL_UNSIGNED_BYTE,rgba);
 const HVglint f=linear?(HVglint)HVGL_LINEAR:(HVglint)HVGL_NEAREST; hvglTexParameteri(HVGL_TEXTURE_2D,HVGL_TEXTURE_MIN_FILTER,mips?(HVglint)HVGL_NEAREST_MIPMAP_NEAREST:f); hvglTexParameteri(HVGL_TEXTURE_2D,HVGL_TEXTURE_MAG_FILTER,f);
 hvglTexParameteri(HVGL_TEXTURE_2D,HVGL_TEXTURE_WRAP_S,(HVglint)HVGL_CLAMP_TO_EDGE); hvglTexParameteri(HVGL_TEXTURE_2D,HVGL_TEXTURE_WRAP_T,(HVglint)HVGL_CLAMP_TO_EDGE);
 return t;
}
bool GlRenderer::createTextures(const AtlasImage&b,const AtlasImage&u,const PanoramaImage&p){
 texBlock_=makeTex(b.rgba.data(),b.width,b.height,false,m_mips); texUi_=makeTex(u.rgba.data(),u.width,u.height,false);
 if(p.valid())texPano_=makeTex(p.rgba.data(),p.size,p.size*6,true);
 else{ const uint8_t black[16]={0,0,0,255,0,0,0,255,0,0,0,255,0,0,0,255}; texPano_=makeTex(black,2,2,true); }
 return true;
}
void GlRenderer::destroyTextures(){ HVgluint t[3]={texBlock_,texUi_,texPano_}; hvglDeleteTextures(3,t); texBlock_=texUi_=texPano_=0; }

bool GlRenderer::init(GLFWwindow*w,const AtlasImage&blk,const AtlasImage&ui,const PanoramaImage&pano){
 win_=w; glfwMakeContextCurrent(w);
 if(!hvLoadGL()){std::cerr<<"OpenGL 3.3 functions are not available.\n";return false;}
 HVglint maj=0,min=0; hvglGetIntegerv(HVGL_MAJOR_VERSION,&maj); hvglGetIntegerv(HVGL_MINOR_VERSION,&min);
 if(maj<3||(maj==3&&min<3)){std::cerr<<"OpenGL 3.3 or newer is required (got "<<maj<<"."<<min<<").\n";return false;}
 clipControl_=false;
 if(hvglClipControl){ if(maj>4||(maj==4&&min>=5))clipControl_=true; else{ HVglint n=0; hvglGetIntegerv(HVGL_NUM_EXTENSIONS,&n); for(HVglint i=0;i<n;i++){const char*e=(const char*)hvglGetStringi(HVGL_EXTENSIONS,(HVgluint)i); if(e&&!std::strcmp(e,"GL_ARB_clip_control")){clipControl_=true;break;}} } }
 std::cout<<"OpenGL "<<hvglGetString(HVGL_VERSION)<<" on "<<hvglGetString(HVGL_RENDERER)<<(clipControl_?" (reverse-Z)":" (standard depth)")<<"\n";
 if(clipControl_)hvglClipControl(HVGL_UPPER_LEFT,HVGL_ZERO_TO_ONE);       // behave like Vulkan: Y down, depth 0..1 -> reverse-Z works
 if(!buildPrograms())return false;
 if(!createTextures(blk,ui,pano))return false;
 // chunk arenas: one big vertex buffer + one 16-bit index buffer, sub-allocated with a free list
 vArena_.init(8u*1024*1024); iArena_.init(12u*1024*1024);
 hvglGenVertexArrays(1,&vaoChunk_); hvglBindVertexArray(vaoChunk_);
 hvglGenBuffers(1,&vboChunk_); hvglBindBuffer(HVGL_ARRAY_BUFFER,vboChunk_); hvglBufferData(HVGL_ARRAY_BUFFER,(HVglsizeiptr)vArena_.capacity*sizeof(Vertex),nullptr,HVGL_STATIC_DRAW);
 hvglEnableVertexAttribArray(0); hvglVertexAttribIPointer(0,1,HVGL_UNSIGNED_INT,(HVglsizei)sizeof(Vertex),(const void*)0);
 hvglEnableVertexAttribArray(1); hvglVertexAttribIPointer(1,1,HVGL_UNSIGNED_INT,(HVglsizei)sizeof(Vertex),(const void*)4);
 hvglGenBuffers(1,&iboChunk_); hvglBindBuffer(HVGL_ELEMENT_ARRAY_BUFFER,iboChunk_); hvglBufferData(HVGL_ELEMENT_ARRAY_BUFFER,(HVglsizeiptr)iArena_.capacity*2,nullptr,HVGL_STATIC_DRAW);
 // UI: dynamic vertex buffer + static quad index buffer
 hvglGenVertexArrays(1,&vaoUi_); hvglBindVertexArray(vaoUi_);
 hvglGenBuffers(1,&vboUi_); hvglBindBuffer(HVGL_ARRAY_BUFFER,vboUi_); hvglBufferData(HVGL_ARRAY_BUFFER,(HVglsizeiptr)kMaxUiQuads*4*sizeof(UiVertex),nullptr,HVGL_STREAM_DRAW);
 hvglEnableVertexAttribArray(0); hvglVertexAttribPointer(0,2,HVGL_FLOAT,HVGL_FALSE,(HVglsizei)sizeof(UiVertex),(const void*)offsetof(UiVertex,x));
 hvglEnableVertexAttribArray(1); hvglVertexAttribPointer(1,2,HVGL_FLOAT,HVGL_FALSE,(HVglsizei)sizeof(UiVertex),(const void*)offsetof(UiVertex,u));
 hvglEnableVertexAttribArray(2); hvglVertexAttribPointer(2,4,HVGL_UNSIGNED_BYTE,HVGL_TRUE,(HVglsizei)sizeof(UiVertex),(const void*)offsetof(UiVertex,rgba));
 std::vector<uint32_t> ix; ix.reserve((size_t)kMaxUiQuads*6); for(uint32_t q=0;q<kMaxUiQuads;q++){uint32_t b=q*4;ix.insert(ix.end(),{b,b+1,b+2,b,b+2,b+3});}
 hvglGenBuffers(1,&iboUi_); hvglBindBuffer(HVGL_ELEMENT_ARRAY_BUFFER,iboUi_); hvglBufferData(HVGL_ELEMENT_ARRAY_BUFFER,(HVglsizeiptr)(ix.size()*sizeof(uint32_t)),ix.data(),HVGL_STATIC_DRAW);
 // selection outline
 hvglGenVertexArrays(1,&vaoLine_); hvglBindVertexArray(vaoLine_);
 hvglGenBuffers(1,&vboLine_); hvglBindBuffer(HVGL_ARRAY_BUFFER,vboLine_); hvglBufferData(HVGL_ARRAY_BUFFER,24*3*sizeof(float),nullptr,HVGL_STREAM_DRAW);
 hvglEnableVertexAttribArray(0); hvglVertexAttribPointer(0,3,HVGL_FLOAT,HVGL_FALSE,12,(const void*)0);
 hvglBindVertexArray(0);
 hvglEnable(HVGL_FRAMEBUFFER_SRGB);
 ok_=true; return true;
}
bool GlRenderer::reloadTextures(const AtlasImage&b,const AtlasImage&u,const PanoramaImage&p){ if(!ok_)return false; glfwMakeContextCurrent(win_); destroyTextures(); return createTextures(b,u,p); }
void GlRenderer::updateTiles(const std::vector<TileUpdate>&t){
 if(!ok_||t.empty())return; hvglBindTexture(HVGL_TEXTURE_2D,texBlock_); hvglPixelStorei(HVGL_UNPACK_ALIGNMENT,1);
 for(const TileUpdate&u:t){ const int tx=u.id%64,ty=u.id/64; hvglTexSubImage2D(HVGL_TEXTURE_2D,0,tx*16,ty*16,16,16,HVGL_RGBA,HVGL_UNSIGNED_BYTE,u.rgba);
  uint8_t m[340]; tileMips(u.rgba,m); size_t off=0; for(int l=1;l<5;l++){ const int sz=16>>l; hvglTexSubImage2D(HVGL_TEXTURE_2D,l,tx*sz,ty*sz,sz,sz,HVGL_RGBA,HVGL_UNSIGNED_BYTE,m+off); off+=(size_t)sz*sz*4; } }
}
bool GlRenderer::upload(int cx,int cz,const ChunkMesh&m){
 const uint64_t k=ckey(cx,cz); auto it=index_.find(k);
 if((m.indices.empty()&&m.tindices.empty())||m.vertices.size()>65535){ release(cx,cz); return true; }
 const uint32_t vc=(uint32_t)m.vertices.size(),ic=(uint32_t)m.indices.size(),tc=(uint32_t)m.tindices.size();
 if(it==index_.end()&&(uint64_t(vArena_.freeUnits())*100<uint64_t(vArena_.capacity)*8||uint64_t(iArena_.freeUnits())*100<uint64_t(iArena_.capacity)*8))return false;   // keep 8% for re-meshing edited chunks
 const uint32_t vo=vArena_.alloc(vc); if(vo==UINT32_MAX)return false;
 const uint32_t io=iArena_.alloc(ic+tc); if(io==UINT32_MAX){vArena_.release(vo,vc);return false;}
 std::vector<uint16_t> idx16(ic+tc); for(uint32_t i=0;i<ic;i++)idx16[i]=(uint16_t)m.indices[i]; for(uint32_t i=0;i<tc;i++)idx16[ic+i]=(uint16_t)m.tindices[i];
 hvglBindBuffer(HVGL_ARRAY_BUFFER,vboChunk_); hvglBufferSubData(HVGL_ARRAY_BUFFER,(HVglintptr)vo*sizeof(Vertex),(HVglsizeiptr)vc*sizeof(Vertex),m.vertices.data());
 hvglBindBuffer(HVGL_ELEMENT_ARRAY_BUFFER,iboChunk_); hvglBufferSubData(HVGL_ELEMENT_ARRAY_BUFFER,(HVglintptr)io*2,(HVglsizeiptr)(ic+tc)*2,idx16.data());
 const GpuChunk g{cx,cz,vo,vc,io,ic,tc,m.maxY};
 if(it!=index_.end()){ GpuChunk&o=list_[it->second]; vArena_.release(o.vOff,o.vCnt); iArena_.release(o.iOff,o.iCnt+o.tCnt); o=g; }   // GL syncs buffer writes with queued draws itself
 else{ index_[k]=(uint32_t)list_.size(); list_.push_back(g); }
 return true;
}
void GlRenderer::release(int cx,int cz){
 auto it=index_.find(ckey(cx,cz)); if(it==index_.end())return; const uint32_t i=it->second; const GpuChunk&o=list_[i];
 vArena_.release(o.vOff,o.vCnt); iArena_.release(o.iOff,o.iCnt+o.tCnt); index_.erase(it);
 if(i!=list_.size()-1){ list_[i]=list_.back(); index_[ckey(list_[i].cx,list_[i].cz)]=i; }
 list_.pop_back();
}
void GlRenderer::clear(){ for(auto&o:list_){vArena_.release(o.vOff,o.vCnt);iArena_.release(o.iOff,o.iCnt+o.tCnt);} list_.clear(); index_.clear(); }

void GlRenderer::draw(const FrameParams&fp,const UiVertex*ui,uint32_t skyQuads,uint32_t panoQuads,uint32_t totalQuads){
 if(!ok_)return;
 if(vsync_!=lastVsync_){ glfwSwapInterval(vsync_?1:0); lastVsync_=vsync_; }
 int W=0,H=0; glfwGetFramebufferSize(win_,&W,&H); if(W<=0||H<=0)return;
 hvglViewport(0,0,W,H);
 hvglClearColor(0.45f,0.65f,0.95f,1.f); hvglClearDepth(clipControl_?0.0:1.0); hvglDepthMask(HVGL_TRUE); hvglClear(HVGL_COLOR_BUFFER_BIT|HVGL_DEPTH_BUFFER_BIT);
 lastDrawn_=0;
 const uint32_t uq=std::min(totalQuads,kMaxUiQuads),sq=std::min(skyQuads,uq),pq=std::min(panoQuads,uq-sq);
 Mat4 uvp{}; { const float fw=(float)W,fh=(float)H; uvp.m[0]=2.f/fw; uvp.m[5]=clipControl_?2.f/fh:-2.f/fh; uvp.m[12]=-1.f; uvp.m[13]=clipControl_?-1.f:1.f; uvp.m[15]=1.f; }
 auto drawUi=[&](GLuint tex,uint32_t first,uint32_t count){ if(!count)return; hvglBindTexture(HVGL_TEXTURE_2D,tex); hvglDrawElements(HVGL_TRIANGLES,(HVglsizei)(count*6),HVGL_UNSIGNED_INT,(const void*)((size_t)first*6*4)); };
 auto beginUi=[&]{ hvglDisable(HVGL_DEPTH_TEST); hvglDisable(HVGL_CULL_FACE); hvglEnable(HVGL_BLEND); hvglBlendFuncSeparate(HVGL_SRC_ALPHA,HVGL_ONE_MINUS_SRC_ALPHA,HVGL_ONE,HVGL_ZERO);
  hvglUseProgram(progUi_); hvglUniformMatrix4fv(uUiVP_,1,HVGL_FALSE,uvp.m); hvglUniform1i(uUiTex_,0); hvglBindVertexArray(vaoUi_); hvglBindBuffer(HVGL_ARRAY_BUFFER,vboUi_); hvglBindBuffer(HVGL_ELEMENT_ARRAY_BUFFER,iboUi_); hvglActiveTexture(HVGL_TEXTURE0); };
 if(uq){ hvglBindVertexArray(vaoUi_); hvglBindBuffer(HVGL_ARRAY_BUFFER,vboUi_); hvglBufferData(HVGL_ARRAY_BUFFER,(HVglsizeiptr)kMaxUiQuads*4*sizeof(UiVertex),nullptr,HVGL_STREAM_DRAW); hvglBufferSubData(HVGL_ARRAY_BUFFER,0,(HVglsizeiptr)uq*4*sizeof(UiVertex),ui); }
 if(sq){ beginUi(); drawUi(texUi_,0,sq); }
 if(fp.world&&!list_.empty()){
  hvglEnable(HVGL_DEPTH_TEST); hvglDepthFunc(clipControl_?HVGL_GREATER:HVGL_LESS); hvglEnable(HVGL_CULL_FACE); hvglCullFace(HVGL_BACK); hvglFrontFace(clipControl_?HVGL_CW:HVGL_CCW);   // with the Vulkan-style clip control (upper-left origin) the winding is inverted hvglDisable(HVGL_BLEND);
  hvglUseProgram(progBlock_); hvglUniformMatrix4fv(uBlockVP_,1,HVGL_FALSE,fp.vp.m); hvglUniform1i(uBlockAtlas_,0);
  hvglActiveTexture(HVGL_TEXTURE0); hvglBindTexture(HVGL_TEXTURE_2D,texBlock_);
  hvglBindVertexArray(vaoChunk_); hvglBindBuffer(HVGL_ARRAY_BUFFER,vboChunk_); hvglBindBuffer(HVGL_ELEMENT_ARRAY_BUFFER,iboChunk_);
  struct TV{float d;uint32_t gi;float ox,oy,oz;}; static std::vector<TV> vt; vt.clear(); uint32_t n=0;
  for(uint32_t gi=0;gi<(uint32_t)list_.size();gi++){
   const GpuChunk&g=list_[gi];
   if(!chunkInRange(g.cx-fp.chunkX,g.cz-fp.chunkZ,fp.renderDist))continue;
   const float ox=float(g.cx*16)-fp.camera.x,oy=-fp.camera.y,oz=float(g.cz*16)-fp.camera.z;
   if(!visible(fp.vp,ox,oy,oz,ox+16.f,oy+float(g.maxY),oz+16.f,clipControl_))continue;
   if(g.iCnt){ hvglUniform3f(uBlockOrigin_,ox,oy,oz); hvglDrawElementsBaseVertex(HVGL_TRIANGLES,(HVglsizei)g.iCnt,HVGL_UNSIGNED_SHORT,(const void*)((size_t)g.iOff*2),(HVglint)g.vOff); }
   if(g.tCnt){ const float dx=ox+8.f,dz=oz+8.f; vt.push_back(TV{dx*dx+dz*dz,gi,ox,oy,oz}); }
   n++;
  }
  lastDrawn_=n;
  if(!vt.empty()){   // translucent pass: far to near, alpha blended, no depth writes
   std::sort(vt.begin(),vt.end(),[](const TV&a,const TV&b){return a.d>b.d;});
   hvglEnable(HVGL_BLEND); hvglBlendFuncSeparate(HVGL_SRC_ALPHA,HVGL_ONE_MINUS_SRC_ALPHA,HVGL_ONE,HVGL_ZERO); hvglDepthMask(HVGL_FALSE);
   for(const TV&t:vt){ const GpuChunk&g=list_[t.gi]; hvglUniform3f(uBlockOrigin_,t.ox,t.oy,t.oz); hvglDrawElementsBaseVertex(HVGL_TRIANGLES,(HVglsizei)g.tCnt,HVGL_UNSIGNED_SHORT,(const void*)((size_t)(g.iOff+g.iCnt)*2),(HVglint)g.vOff); }
   hvglDepthMask(HVGL_TRUE); hvglDisable(HVGL_BLEND);
  }
 }
 if(fp.world&&fp.outline){
  const float e=0.003f,x0=float(fp.ox)-fp.camera.x-e,y0=float(fp.oy)-fp.camera.y-e,z0=float(fp.oz)-fp.camera.z-e,x1=x0+1.f+2*e,y1=y0+1.f+2*e,z1=z0+1.f+2*e;
  const float P[8][3]={{x0,y0,z0},{x1,y0,z0},{x1,y0,z1},{x0,y0,z1},{x0,y1,z0},{x1,y1,z0},{x1,y1,z1},{x0,y1,z1}};
  static const int E[12][2]={{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
  float lv[72]; for(int i=0;i<12;i++)for(int k=0;k<2;k++)for(int a=0;a<3;a++)lv[(i*2+k)*3+a]=P[E[i][k]][a];
  hvglEnable(HVGL_DEPTH_TEST); hvglDepthFunc(clipControl_?HVGL_GREATER:HVGL_LESS); hvglDisable(HVGL_CULL_FACE);
  hvglUseProgram(progLine_); hvglUniformMatrix4fv(uLineVP_,1,HVGL_FALSE,fp.vp.m);
  hvglBindVertexArray(vaoLine_); hvglBindBuffer(HVGL_ARRAY_BUFFER,vboLine_); hvglBufferSubData(HVGL_ARRAY_BUFFER,0,sizeof lv,lv); hvglDrawArrays(HVGL_LINES,0,24);
 }
 if(uq>sq){ beginUi(); drawUi(texPano_,sq,pq); drawUi(texUi_,sq+pq,uq-sq-pq); hvglDisable(HVGL_BLEND); }
 glfwSwapBuffers(win_);
}
void GlRenderer::shutdown(){
 if(!ok_)return; glfwMakeContextCurrent(win_);
 destroyTextures(); HVgluint b[5]={vboChunk_,iboChunk_,vboUi_,iboUi_,vboLine_}; hvglDeleteBuffers(5,b); HVgluint a[3]={vaoChunk_,vaoUi_,vaoLine_}; hvglDeleteVertexArrays(3,a);
 hvglDeleteProgram(progBlock_); hvglDeleteProgram(progUi_); hvglDeleteProgram(progLine_); ok_=false;
}
