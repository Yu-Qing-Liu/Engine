#version 450

layout(location = 0) in vec4 vColor;
layout(location = 0) out vec4 outColor;

void main() {
    // Map to [-1,1]^2
    vec2 p = gl_PointCoord * 2.0 - 1.0;
    float r = length(p);

    // Smooth falloff to edge
    float mask = clamp(1.0 - r, 0.0, 1.0);

    outColor = vec4(vColor.rgb, vColor.a * mask);
}
