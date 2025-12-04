#version 450
layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec2 vUV;
layout(location = 2) in vec4 vColor;
layout(location = 3) in vec4 vOutlineColor;
layout(location = 4) in float vOutlineWidth;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(vec3(0.4, 0.7, 0.55));
    float ndotl = max(dot(N, L), 0.0);
    vec3 lit = vColor.rgb * (0.2 + 0.8 * ndotl);
    outColor = vec4(lit, vColor.a);
}
