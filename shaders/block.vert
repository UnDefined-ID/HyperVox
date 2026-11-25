#version 450
// Packed 8-byte vertex (1/16 block units): a = x(9) | z(9)<<9 | y(12)<<18 | face&3<<30, b = u(9) | v(9)<<9 | tile(12)<<18 | face>>2<<30 | y>>12<<31.
// Per draw (instance): xyz = mesh origin (camera-relative), w = vertex scale: 1 = normal chunk / section, 2 = merged 3x3 chunk mesh, 8 = merged 9x9 chunk mesh.
layout(location=0) in uint inA;
layout(location=1) in uint inB;
layout(location=2) in vec4 inOrigin;
layout(location=0) out vec2 uv;
layout(location=1) flat out uint texId;
layout(location=2) flat out float shade;
layout(location=3) out vec3 rel;   // camera-relative position (for the fog)
layout(push_constant) uniform PC { mat4 vp; vec4 fogColor; vec4 fogParams; } pc;
const float SHADE[6] = float[6](0.431, 1.0, 0.686, 0.686, 0.843, 0.843);
void main(){
    vec3 p = vec3(float(inA & 511u), float(((inA >> 18) & 4095u) | ((inB >> 31) << 12)), float((inA >> 9) & 511u)) / 16.0 * inOrigin.w + inOrigin.xyz;
    rel = p;
    gl_Position = pc.vp * vec4(p, 1.0);
    uv = vec2(float(inB & 511u), float((inB >> 9) & 511u)) / 16.0;
    texId = (inB >> 18) & 4095u;
    shade = SHADE[((inA >> 30) & 3u) | (((inB >> 30) & 1u) << 2)];
}
