#include "Climate.hpp"
#include <cmath>
#include <cstring>
#include <string>
namespace{
using u64=uint64_t;
inline u64 rotl(u64 x,int k){return (x<<k)|(x>>(64-k));}
inline u64 mix13(u64 z){z=(z^(z>>30))*0xBF58476D1CE4E5B9ULL;z=(z^(z>>27))*0x94D049BB133111EBULL;return z^(z>>31);}
struct Hash{const char*n;u64 lo,hi;};
const Hash kHash[]={
#include "Hashes.inc"
};
const Hash* find(const std::string&n){for(auto&h:kHash)if(n==h.n)return &h;return nullptr;}
struct Xoro{
 u64 lo,hi;
 Xoro(u64 l,u64 h):lo(l),hi(h){if(!lo&&!hi){lo=0x9E3779B97F4A7C15ULL;hi=0x6A09E667F3BCC909ULL;}}
 static Xoro fromSeed(u64 seed){u64 l=seed^0x6A09E667F3BCC909ULL,m=l+0x9E3779B97F4A7C15ULL;return Xoro(mix13(l),mix13(m));}
 u64 nextLong(){u64 l=lo,m=hi,n=rotl(l+m,17)+l;m^=l;lo=rotl(l,49)^m^(m<<21);hi=rotl(m,28);return n;}
 uint32_t nextInt(uint32_t bound){
  u64 l=nextLong()&0xFFFFFFFFULL,m=l*bound,n=m&0xFFFFFFFFULL;
  if(n<bound){ const uint32_t i=(uint32_t)((~bound+1u)%bound); while(n<i){l=nextLong()&0xFFFFFFFFULL;m=l*bound;n=m&0xFFFFFFFFULL;} }
  return (uint32_t)(m>>32);
 }
 double nextDouble(){return double(nextLong()>>11)*1.1102230246251565e-16;}
};
struct Pos{u64 lo,hi;};
Xoro fromHashOf(const Pos&p,const std::string&name){const Hash*h=find(name); return Xoro(h?h->lo^p.lo:p.lo,h?h->hi^p.hi:p.hi);}
const int G[16][3]={{1,1,0},{-1,1,0},{1,-1,0},{-1,-1,0},{1,0,1},{-1,0,1},{1,0,-1},{-1,0,-1},{0,1,1},{0,-1,1},{0,1,-1},{0,-1,-1},{1,1,0},{0,-1,1},{-1,1,0},{0,-1,-1}};
struct Improved{
 double xo,yo,zo; uint8_t p[512];
 explicit Improved(Xoro r){ xo=r.nextDouble()*256;yo=r.nextDouble()*256;zo=r.nextDouble()*256; uint8_t q[256]; for(int i=0;i<256;i++)q[i]=(uint8_t)i; for(int i=0;i<256;i++){uint32_t j=r.nextInt(256-i);std::swap(q[i],q[i+j]);} for(int i=0;i<512;i++)p[i]=q[i&255]; }
 double noise(double x,double y,double z)const{
  x+=xo;y+=yo;z+=zo; const double fx=std::floor(x),fy=std::floor(y),fz=std::floor(z); const double g=x-fx,h=y-fy,l=z-fz; const int i=(int)(long long)fx&255,j=(int)(long long)fy&255,k=(int)(long long)fz&255;
  const int m=p[i],n=p[i+1],o=p[m+j],q=p[m+j+1],r=p[n+j],t=p[n+j+1];
  auto gd=[&](int hv,double a,double b,double c){const int*v=G[hv&15];return v[0]*a+v[1]*b+v[2]*c;};
  const double v000=gd(p[o+k],g,h,l),v100=gd(p[r+k],g-1,h,l),v010=gd(p[q+k],g,h-1,l),v110=gd(p[t+k],g-1,h-1,l),v001=gd(p[o+k+1],g,h,l-1),v101=gd(p[r+k+1],g-1,h,l-1),v011=gd(p[q+k+1],g,h-1,l-1),v111=gd(p[t+k+1],g-1,h-1,l-1);
  const double sx=g*g*g*(g*(g*6-15)+10),sy=h*h*h*(h*(h*6-15)+10),sz=l*l*l*(l*(l*6-15)+10);
  const double a=v000+(v100-v000)*sx,b=v010+(v110-v010)*sx,c=v001+(v101-v001)*sx,d=v011+(v111-v011)*sx,e=a+(b-a)*sy,f=c+(d-c)*sy; return e+(f-e)*sz;
 }
};
inline double wrap(double v){return v-std::floor(v/3.3554432E7+0.5)*3.3554432E7;}
struct Perlin{
 std::vector<double> amps; std::vector<std::unique_ptr<Improved>> lv; double lowIn,lowVal;
 Perlin(Xoro&r,int first,const std::vector<double>&a):amps(a){
  Pos pos{r.nextLong(),r.nextLong()};
  for(size_t k=0;k<a.size();k++) lv.emplace_back(a[k]!=0?std::make_unique<Improved>(fromHashOf(pos,"octave_"+std::to_string(first+(int)k))):nullptr);
  lowIn=std::pow(2.0,first); const int n=(int)a.size(); lowVal=std::pow(2.0,n-1)/(std::pow(2.0,n)-1);
 }
 double get(double x,double y,double z)const{ double d=0,e=lowIn,f=lowVal; for(size_t i=0;i<amps.size();i++){ if(lv[i])d+=amps[i]*lv[i]->noise(wrap(x*e),wrap(y*e),wrap(z*e))*f; e*=2;f/=2;} return d; }
};
struct Normal{
 Perlin a,b; double vf;
 Normal(Xoro&r,int first,const std::vector<double>&amps):a(r,first,amps),b(r,first,amps){ int lo=1<<30,hi=-1; for(size_t i=0;i<amps.size();i++)if(amps[i]!=0){lo=std::min(lo,(int)i);hi=std::max(hi,(int)i);} vf=0.16666666666666666/(0.1*(1.0+1.0/double(hi-lo+1))); }
 double get(double x,double y,double z)const{ return (a.get(x,y,z)+b.get(x*1.0181268882175227,y*1.0181268882175227,z*1.0181268882175227))*vf; }
};
// ---- terrain offset spline (vanilla TerrainProvider), inputs: continents, erosion, ridges folded (peaks & valleys)
struct Spline{
 struct P{float loc; float val; float d; const Spline*child;};
 int coord=0; std::vector<P> p;
 Spline&add(float loc,float v,float d=0.f){p.push_back({loc,v,d,nullptr});return *this;}
 Spline&add(float loc,const Spline*s,float d=0.f){p.push_back({loc,0.f,d,s});return *this;}
 float ev(const P&q,const float*in)const{return q.child?q.child->eval(in):q.val;}
 float eval(const float*in)const{
  const float x=in[coord]; const size_t n=p.size();
  if(x<p[0].loc)return ev(p[0],in)+p[0].d*(x-p[0].loc);
  if(x>=p[n-1].loc)return ev(p[n-1],in)+p[n-1].d*(x-p[n-1].loc);
  size_t i=0; while(i<n-2&&x>=p[i+1].loc)i++;
  const float x0=p[i].loc,x1=p[i+1].loc,v0=ev(p[i],in),v1=ev(p[i+1],in),t=(x-x0)/(x1-x0),w=x1-x0,h0=p[i].d*w,h1=p[i+1].d*w;
  const float t2=t*t,t3=t2*t; return (2*t3-3*t2+1)*v0+(t3-2*t2+t)*h0+(-2*t3+3*t2)*v1+(t3-t2)*h1;
 }
};
struct Splines{
 std::deque<Spline> pool; const Spline* offset=nullptr;
 Spline* mk(int c){pool.emplace_back();pool.back().coord=c;return &pool.back();}
 static float lerp(float t,float a,float b){return a+t*(b-a);}
 static float mountCont(float f,float g,float h){const float k=1.f-(1.f-g)*0.5f,l=0.5f*(1.f-g),m=(f+1.17f)*0.46082947f,n=m*k-l;return f<h?std::max(n,-0.2222f):std::max(n,0.f);}
 static float zeroPoint(float f){const float i=1.f-(1.f-f)*0.5f,j=0.5f*(1.f-f);return j/(0.46082947f*i)-1.17f;}
 static float slope(float f,float g,float h,float i){return (g-f)/(i-h);}
 const Spline* mountRidge(float f,bool bl){
  Spline*b=mk(2); const float i=mountCont(-1.f,f,-0.7f),l=mountCont(1.f,f,-0.7f),m=zeroPoint(f);
  if(-0.65f<m&&m<1.f){ const float o=mountCont(-0.65f,f,-0.7f),q=mountCont(-0.75f,f,-0.7f),r=slope(i,q,-1.f,-0.75f); b->add(-1.f,i,r).add(-0.75f,q,0.f).add(-0.65f,o,0.f); const float s=mountCont(m,f,-0.7f),u=slope(s,l,m,1.f); b->add(m-0.01f,s,0.f).add(m,s,u).add(1.f,l,u); }
  else{ const float o=slope(i,l,-1.f,1.f); if(bl){b->add(-1.f,std::max(0.2f,i),0.f).add(0.f,lerp(0.5f,i,l),o);} else b->add(-1.f,i,o); b->add(1.f,l,o); }
  return b;
 }
 const Spline* ridge(float f,float g,float h,float i,float j,float k){ const float l=std::max(0.5f*(g-f),k),m=5.f*(h-g); Spline*b=mk(2); b->add(-1.f,f,l).add(-0.4f,g,std::min(l,m)).add(0.f,h,m).add(0.4f,i,2.f*(i-h)).add(1.f,j,0.7f*(j-i)); return b; }
 const Spline* erosionOff(float f,float g,float h,float i,float j,float k,bool bl,bool bl2){
  const Spline*s1=mountRidge(lerp(i,0.6f,1.5f),bl2),*s2=mountRidge(lerp(i,0.6f,1.f),bl2),*s3=mountRidge(i,bl2);
  const Spline*s4=ridge(f-0.15f,0.5f*i,lerp(0.5f,0.5f,0.5f)*i,0.5f*i,0.6f*i,0.5f),*s5=ridge(f,j*i,g*i,0.5f*i,0.6f*i,0.5f),*s6=ridge(f,j,j,g,h,0.5f),*s7=ridge(f,j,j,g,h,0.5f);
  Spline*s8=mk(2); s8->add(-1.f,f,0.f).add(-0.4f,s6,0.f).add(0.f,h+0.07f,0.f);
  const Spline*s9=ridge(-0.02f,k,k,g,h,0.f);
  Spline*b=mk(1); b->add(-0.85f,s1).add(-0.7f,s2).add(-0.4f,s3).add(-0.35f,s4).add(-0.1f,s5).add(0.2f,s6);
  if(bl)b->add(0.4f,s7).add(0.45f,s8).add(0.55f,s8).add(0.58f,s6);
  b->add(0.7f,s9); return b;
 }
 Splines(){
  const Spline*A=erosionOff(-0.15f,0.f,0.f,0.1f,0.f,-0.03f,false,false),*B=erosionOff(-0.1f,0.03f,0.1f,0.1f,0.01f,-0.03f,false,false),*Cc=erosionOff(-0.1f,0.03f,0.1f,0.7f,0.01f,-0.03f,true,true),*D=erosionOff(-0.05f,0.03f,0.1f,1.f,0.01f,0.01f,true,true);
  Spline*o=mk(0); o->add(-1.1f,0.044f).add(-1.02f,-0.2222f).add(-0.51f,-0.2222f).add(-0.44f,-0.12f).add(-0.18f,-0.12f).add(-0.16f,A).add(-0.15f,A).add(-0.1f,B).add(0.25f,Cc).add(1.f,D); offset=o;
 }
};
const Splines& splines(){static const Splines s; return s;}
}
struct Climate::Impl{ std::unique_ptr<Normal> cont,eros,temp,veg,ridge,shift; };
Climate::Climate(uint64_t seed):m(new Impl){
 Xoro base=Xoro::fromSeed(seed); Pos pos{base.nextLong(),base.nextLong()};
 auto mkn=[&](const char*name,int first,std::vector<double> amps){Xoro r=fromHashOf(pos,name);return std::make_unique<Normal>(r,first,amps);};
 m->cont=mkn("minecraft:continentalness",-9,{1,1,2,2,2,1,1,1,1}); m->eros=mkn("minecraft:erosion",-9,{1,1,0,1,1}); m->temp=mkn("minecraft:temperature",-10,{1.5,0,1,0,0,0});
 m->veg=mkn("minecraft:vegetation",-8,{1,1,0,0,0,0}); m->ridge=mkn("minecraft:ridge",-7,{1,2,1,0,0,0}); m->shift=mkn("minecraft:offset",-3,{1,1,1,0});
}
Climate::~Climate()=default;
ClimateSample Climate::at(int x,int z)const{
 const double sx=m->shift->get(x*0.25,0,z*0.25)*4,sz=m->shift->get(z*0.25,x*0.25,0)*4; const double px=x*0.25+sx,pz=z*0.25+sz;
 return ClimateSample{(float)m->cont->get(px,0,pz),(float)m->eros->get(px,0,pz),(float)m->temp->get(px,0,pz),(float)m->veg->get(px,0,pz),(float)m->ridge->get(px,0,pz)};
}
float Climate::offset(float c,float e,float w){ const float pv=-3.f*(std::fabs(std::fabs(w)-2.f/3.f)-1.f/3.f); const float in[3]={c,e,pv}; return splines().offset->eval(in); }
