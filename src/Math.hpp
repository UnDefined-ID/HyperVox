#pragma once
#include <cmath>
#include <array>
struct Vec3 { float x=0,y=0,z=0; Vec3 operator+(const Vec3&o)const{return{x+o.x,y+o.y,z+o.z};} Vec3 operator-(const Vec3&o)const{return{x-o.x,y-o.y,z-o.z};} Vec3 operator*(float s)const{return{x*s,y*s,z*s};} };
inline float dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;} inline Vec3 cross(Vec3 a,Vec3 b){return{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline Vec3 normalize(Vec3 a){float n=std::sqrt(dot(a,a));return n>0?a*(1.0f/n):Vec3{};}
struct Mat4 { float m[16]{}; static Mat4 identity(){Mat4 r; r.m[0]=r.m[5]=r.m[10]=r.m[15]=1; return r;} };
inline Mat4 mul(const Mat4&a,const Mat4&b){Mat4 r; for(int c=0;c<4;c++)for(int row=0;row<4;row++)for(int k=0;k<4;k++)r.m[c*4+row]+=a.m[k*4+row]*b.m[c*4+k];return r;}
inline Mat4 perspective(float fov,float aspect,float zn,float zf){Mat4 r{};float f=1/std::tan(fov*.5f);r.m[0]=f/aspect;r.m[5]=-f;r.m[10]=zf/(zn-zf);r.m[11]=-1;r.m[14]=(zf*zn)/(zn-zf);return r;}
inline Mat4 lookAt(Vec3 eye,Vec3 center,Vec3 up){Vec3 f=normalize(center-eye),s=normalize(cross(f,up)),u=cross(s,f);Mat4 r=Mat4::identity();r.m[0]=s.x;r.m[4]=s.y;r.m[8]=s.z;r.m[1]=u.x;r.m[5]=u.y;r.m[9]=u.z;r.m[2]=-f.x;r.m[6]=-f.y;r.m[10]=-f.z;r.m[12]=-dot(s,eye);r.m[13]=-dot(u,eye);r.m[14]=dot(f,eye);return r;}
// Reverse-Z projection (near -> 1, far -> 0) for a [0,1] depth range: float depth keeps full precision at any distance.  flipY = Vulkan clip space.
inline Mat4 perspectiveRev(float fov,float aspect,float zn,float zf,bool flipY){Mat4 r{};float f=1/std::tan(fov*.5f);r.m[0]=f/aspect;r.m[5]=flipY?-f:f;r.m[10]=zn/(zf-zn);r.m[11]=-1;r.m[14]=(zn*zf)/(zf-zn);return r;}
// Classic OpenGL projection ([-1,1] depth, Y up) - only used when the driver has no glClipControl.
inline Mat4 perspectiveGL(float fov,float aspect,float zn,float zf){Mat4 r{};float f=1/std::tan(fov*.5f);r.m[0]=f/aspect;r.m[5]=f;r.m[10]=-(zf+zn)/(zf-zn);r.m[11]=-1;r.m[14]=-2.f*zf*zn/(zf-zn);return r;}
