#pragma once
// Minimal OpenGL 3.3 loader: every function (even GL 1.1) is fetched through glfwGetProcAddress, so no GL headers or libGL link are needed.
#include <GLFW/glfw3.h>
#include <cstddef>
#include <cstdint>
#ifdef _WIN32
#define HV_GLCALL __stdcall
#else
#define HV_GLCALL
#endif
typedef unsigned int HVglenum,HVgluint,HVglbitfield; typedef int HVglint,HVglsizei; typedef unsigned char HVglboolean,HVglubyte; typedef float HVglfloat; typedef double HVgldouble; typedef char HVglchar;
typedef ptrdiff_t HVglsizeiptr,HVglintptr;
enum:unsigned{
 HVGL_COLOR_BUFFER_BIT=0x4000,HVGL_DEPTH_BUFFER_BIT=0x100,HVGL_DEPTH_TEST=0xB71,HVGL_CULL_FACE=0xB44,HVGL_BLEND=0xBE2,HVGL_FRAMEBUFFER_SRGB=0x8DB9,HVGL_BACK=0x405,HVGL_CCW=0x901,HVGL_CW=0x900,
 HVGL_LESS=0x201,HVGL_GREATER=0x204,HVGL_SRC_ALPHA=0x302,HVGL_ONE_MINUS_SRC_ALPHA=0x303,HVGL_ONE=1,HVGL_ZERO=0,HVGL_TRIANGLES=4,HVGL_LINES=1,
 HVGL_UNSIGNED_INT=0x1405,HVGL_UNSIGNED_SHORT=0x1403,HVGL_UNSIGNED_BYTE=0x1401,HVGL_FLOAT=0x1406,HVGL_ARRAY_BUFFER=0x8892,HVGL_ELEMENT_ARRAY_BUFFER=0x8893,
 HVGL_STATIC_DRAW=0x88E4,HVGL_STREAM_DRAW=0x88E0,HVGL_TEXTURE_2D=0xDE1,HVGL_TEXTURE0=0x84C0,HVGL_TEXTURE_MIN_FILTER=0x2801,HVGL_TEXTURE_MAG_FILTER=0x2800,HVGL_NEAREST=0x2600,HVGL_LINEAR=0x2601,
 HVGL_TEXTURE_MAX_LEVEL=0x813D,HVGL_NEAREST_MIPMAP_NEAREST=0x2700,HVGL_TEXTURE_WRAP_S=0x2802,HVGL_TEXTURE_WRAP_T=0x2803,HVGL_CLAMP_TO_EDGE=0x812F,HVGL_RGBA=0x1908,HVGL_SRGB8_ALPHA8=0x8C43,HVGL_VERTEX_SHADER=0x8B31,HVGL_FRAGMENT_SHADER=0x8B30,
 HVGL_COMPILE_STATUS=0x8B81,HVGL_LINK_STATUS=0x8B82,HVGL_INFO_LOG_LENGTH=0x8B84,HVGL_VERSION=0x1F02,HVGL_RENDERER=0x1F01,HVGL_MAJOR_VERSION=0x821B,HVGL_MINOR_VERSION=0x821C,HVGL_EXTENSIONS=0x1F03,HVGL_NUM_EXTENSIONS=0x821D,
 HVGL_UPPER_LEFT=0x8CA2,HVGL_ZERO_TO_ONE=0x935F,HVGL_UNPACK_ALIGNMENT=0xCF5,HVGL_TRUE=1,HVGL_FALSE=0
};
#define HV_GL_FUNCS(X) \
 X(void,Clear,(HVglbitfield)) X(void,ClearColor,(HVglfloat,HVglfloat,HVglfloat,HVglfloat)) X(void,ClearDepth,(HVgldouble)) \
 X(void,Enable,(HVglenum)) X(void,Disable,(HVglenum)) X(void,Viewport,(HVglint,HVglint,HVglsizei,HVglsizei)) X(void,CullFace,(HVglenum)) X(void,FrontFace,(HVglenum)) \
 X(void,DepthFunc,(HVglenum)) X(void,DepthMask,(HVglboolean)) X(void,BlendFunc,(HVglenum,HVglenum)) X(void,BlendFuncSeparate,(HVglenum,HVglenum,HVglenum,HVglenum)) \
 X(void,GenTextures,(HVglsizei,HVgluint*)) X(void,DeleteTextures,(HVglsizei,const HVgluint*)) X(void,BindTexture,(HVglenum,HVgluint)) \
 X(void,TexImage2D,(HVglenum,HVglint,HVglint,HVglsizei,HVglsizei,HVglint,HVglenum,HVglenum,const void*)) X(void,TexSubImage2D,(HVglenum,HVglint,HVglint,HVglint,HVglsizei,HVglsizei,HVglenum,HVglenum,const void*)) \
 X(void,TexParameteri,(HVglenum,HVglenum,HVglint)) X(void,PixelStorei,(HVglenum,HVglint)) X(const HVglubyte*,GetString,(HVglenum)) X(const HVglubyte*,GetStringi,(HVglenum,HVgluint)) X(void,GetIntegerv,(HVglenum,HVglint*)) \
 X(void,ActiveTexture,(HVglenum)) X(void,GenBuffers,(HVglsizei,HVgluint*)) X(void,DeleteBuffers,(HVglsizei,const HVgluint*)) X(void,BindBuffer,(HVglenum,HVgluint)) \
 X(void,BufferData,(HVglenum,HVglsizeiptr,const void*,HVglenum)) X(void,BufferSubData,(HVglenum,HVglintptr,HVglsizeiptr,const void*)) \
 X(void,GenVertexArrays,(HVglsizei,HVgluint*)) X(void,DeleteVertexArrays,(HVglsizei,const HVgluint*)) X(void,BindVertexArray,(HVgluint)) X(void,EnableVertexAttribArray,(HVgluint)) \
 X(void,VertexAttribPointer,(HVgluint,HVglint,HVglenum,HVglboolean,HVglsizei,const void*)) X(void,VertexAttribIPointer,(HVgluint,HVglint,HVglenum,HVglsizei,const void*)) \
 X(HVgluint,CreateShader,(HVglenum)) X(void,ShaderSource,(HVgluint,HVglsizei,const HVglchar*const*,const HVglint*)) X(void,CompileShader,(HVgluint)) X(void,GetShaderiv,(HVgluint,HVglenum,HVglint*)) \
 X(void,GetShaderInfoLog,(HVgluint,HVglsizei,HVglsizei*,HVglchar*)) X(HVgluint,CreateProgram,(void)) X(void,AttachShader,(HVgluint,HVgluint)) X(void,LinkProgram,(HVgluint)) \
 X(void,GetProgramiv,(HVgluint,HVglenum,HVglint*)) X(void,GetProgramInfoLog,(HVgluint,HVglsizei,HVglsizei*,HVglchar*)) X(void,UseProgram,(HVgluint)) X(void,DeleteShader,(HVgluint)) X(void,DeleteProgram,(HVgluint)) \
 X(HVglint,GetUniformLocation,(HVgluint,const HVglchar*)) X(void,UniformMatrix4fv,(HVglint,HVglsizei,HVglboolean,const HVglfloat*)) X(void,Uniform3f,(HVglint,HVglfloat,HVglfloat,HVglfloat)) X(void,Uniform1i,(HVglint,HVglint)) \
 X(void,DrawElements,(HVglenum,HVglsizei,HVglenum,const void*)) X(void,DrawElementsBaseVertex,(HVglenum,HVglsizei,HVglenum,const void*,HVglint)) X(void,DrawArrays,(HVglenum,HVglint,HVglsizei))
#define HV_DECL(R,N,A) extern R (HV_GLCALL *hvgl##N) A;
HV_GL_FUNCS(HV_DECL)
#undef HV_DECL
extern void (HV_GLCALL *hvglClipControl)(HVglenum,HVglenum);   // GL 4.5 / ARB_clip_control, may be null
bool hvLoadGL();                                           // needs a current context; false when GL < 3.3
