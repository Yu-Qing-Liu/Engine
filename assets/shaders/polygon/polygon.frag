#version 450

layout(location = 0) in vec4 vColor;
layout(location = 1) in vec3 vBary;
layout(location = 2) in vec3 vEdgeMask;

layout(location = 3) flat in vec4 vIColor;
layout(location = 4) flat in vec4 vIOutlineColor;
layout(location = 5) flat in float vIOutlineWidth;

layout(location = 0) out vec4 outColor;

void main() {
    // Distance to nearest *marked* edge (opposite vertex where bary = 0)
    float d = 1e6;
    if (vEdgeMask.x > 0.5) d = min(d, vBary.x);
    if (vEdgeMask.y > 0.5) d = min(d, vBary.y);
    if (vEdgeMask.z > 0.5) d = min(d, vBary.z);

    vec4 fill = vColor * vIColor;

    if (d > 1e5) {
        outColor = fill;
        return;
    }

    // AA width scaled by outline width (in "bary space"; tune if you want px-space)
    float w = fwidth(d) * max(vIOutlineWidth, 1.0);
    float t = smoothstep(0.0, w, d); // t ~ 0 at edge, ~1 inside

    vec4 edgeC = vIOutlineColor;

    outColor = mix(edgeC, fill, t);
}
