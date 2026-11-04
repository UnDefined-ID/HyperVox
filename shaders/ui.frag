#version 450
layout(location=0) in vec2 uv;
layout(location=1) in vec4 col;
layout(location=0) out vec4 outColor;
layout(set=0,binding=0) uniform sampler2D atlas;
void main(){
    vec4 c = texture(atlas, uv) * col;
    if (c.a < 0.004) discard;
    outColor = c;
}
