#version 450

layout(location = 0) in vec3  inPos;    // world-space for normal mode OR pixel-space for billboard mode
layout(location = 1) in vec2  inUV;
layout(location = 2) in float inXNorm;
layout(location = 3) in uint  inFlags;
layout(location = 4) in float inQuadW;

layout(location = 0) out vec2  vUV;
layout(location = 1) out float vXNorm;
layout(location = 2) flat out uint  vFlags;
layout(location = 3) flat out float vQuadW;

layout(set = 0, binding = 0) uniform UBO {
    mat4 model;
    mat4 view;
    mat4 proj;
} u;

// Same push-constant block as in fragment
layout(push_constant) uniform Push {
    vec4 textColor;
    vec4 selectColor;
    vec4 caretColor;
    vec4 misc;         // .w = mode (0=world, 1=billboard)
    vec4 bbCenter;     // world xyz
    vec4 bbOffsetPx;   // pixel xy
    vec4 bbScreenSize; // pixel xy
} pc;

void main() {
    vUV    = inUV;
    vXNorm = inXNorm;
    vFlags = inFlags;
    vQuadW = inQuadW;

    // mode: 0 = your original world-space path, 1 = billboard (screen-space offset)
    if (pc.misc.w < 0.5) {
        // --- Normal text (what you had) ---
        gl_Position = u.proj * u.view * u.model * vec4(inPos, 1.0);
        return;
    }

    // --- Billboard text ---
    // 1) Project the world center (baseline origin)
    vec4 clipBase = u.proj * u.view * u.model * vec4(pc.bbCenter.xyz, 1.0);

    // 2) Per-vertex pixel position = (optional per-glyph offset) + this vertex's pixel corner
    // For flexibility:
    //  - If you already baked absolute pixel coords into inPos.xy, set bbOffsetPx.xy={0,0}
    //  - If inPos.xy holds local quad corners (0..w,0..h), put the glyph TL in bbOffsetPx.xy
    vec2 pixelPos = pc.bbOffsetPx.xy + inPos.xy;

    // 3) Convert pixels -> clip offset (preserve perspective via clipBase.w)
    vec2 clipOffset = (pixelPos / pc.bbScreenSize.xy) * 2.0 * clipBase.w;

    // 4) Final position in clip space
    gl_Position = vec4(clipBase.xy + clipOffset, clipBase.z, clipBase.w);
}

