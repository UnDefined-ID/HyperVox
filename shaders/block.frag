#version 450
layout(location=0) in vec2 uv;
layout(location=1) flat in uint texId;
layout(location=2) flat in float shade;
layout(location=3) in vec3 rel;
layout(push_constant) uniform PC { mat4 vp; vec4 fogColor; vec4 fogParams; } pc;   // fogParams = start, end, on
layout(location=0) out vec4 outColor;
layout(set=0,binding=0) uniform sampler2D atlas;
void main(){
    const float TILES=64.0;
    float id=float(texId);
    vec2 cell=vec2(mod(id,TILES),floor(id/TILES));
    vec2 a=(cell+fract(uv))/TILES;
    // gradients come from the continuous uv (not from fract(uv)) so mip selection has no seams; the atlas has 5 mip levels = one pixel per tile at the end; derivatives are halved = one mip level sharper
    vec4 c=textureGrad(atlas,a,0.5*dFdx(uv)/TILES,0.5*dFdy(uv)/TILES);
    if(c.a<0.1) discard;
    float fog = pc.fogParams.z > 0.5 ? clamp((length(rel) - pc.fogParams.x) / max(pc.fogParams.y - pc.fogParams.x, 0.001), 0.0, 1.0) : 0.0;
    outColor=vec4(mix(c.rgb*shade, pc.fogColor.rgb, fog), c.a);
}
