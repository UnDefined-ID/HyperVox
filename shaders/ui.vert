#version 450
layout(location=0) in vec2 inPos;
layout(location=1) in vec2 inUV;
layout(location=2) in vec4 inColor;
layout(location=0) out vec2 uv;
layout(location=1) out vec4 col;
layout(push_constant) uniform PC { mat4 vp; } pc;
void main(){
    gl_Position = pc.vp * vec4(inPos, 0.0, 1.0);
    uv = inUV;
    col = vec4(pow(inColor.rgb, vec3(2.2)), inColor.a);   // vertex colours are authored in sRGB
}
