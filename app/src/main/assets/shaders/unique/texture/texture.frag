#version 450

layout(location=0) in  vec2 vUV;
layout(location=1) in  vec4 vColor;
layout(location=0) out vec4 outColor;

layout(binding=1) uniform sampler2D uTex;

void main() {
    outColor = texture(uTex, vUV) * vColor;  // <-- no UV scaling
}

