#version 450

layout(location=0) in vec2 vUV;
layout(location=0) out vec4 outColor;
layout(set=0, binding=1) uniform sampler2D uAtlas;

layout(push_constant) uniform Push {
    vec4 color; 
} pc;

void main() {
    float a = texture(uAtlas, vUV).r;
    if (a <= 0.001) {
        discard;
    }  
    outColor = vec4(pc.color.rgb, pc.color.a * a); 
}
