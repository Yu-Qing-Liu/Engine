#version 450

layout(location = 0) in vec4 vColor;
layout(location = 1) in vec3 vBary;
layout(location = 2) in vec3 vEdgeMask;

layout(binding = 1) uniform Params {
    vec4 color;
    vec4 outlineColor;
    float outlineWidth;
    float borderRadius;
    float _pad1, _pad2;
} P;

layout(location = 0) out vec4 outColor;

void main() {
    // pick the distance to the nearest *marked* edge (opposite the vertex where bary = 0)
    float d = 1e6;
    if (vEdgeMask.x > 0.5) d = min(d, vBary.x);
    if (vEdgeMask.y > 0.5) d = min(d, vBary.y);
    if (vEdgeMask.z > 0.5) d = min(d, vBary.z);

    // If no marked edge for this pixel, just fill
    if (d > 1e5) {
        outColor = vColor * P.color;
        return;
    }

    // Antialiased line around the marked edge(s)
    // Scale using screen-space derivatives; multiply by a width control.
    float w = fwidth(d) * max(P.outlineWidth, 1.0);
    float t = smoothstep(0.0, w, d); // t ~ 0 at edge, ~1 inside

    vec4 fill = vColor * P.color;
    vec4 edgeC = P.outlineColor;

    // Blend edge over fill (soft falloff into interior)
    outColor = mix(edgeC, fill, t);
}
