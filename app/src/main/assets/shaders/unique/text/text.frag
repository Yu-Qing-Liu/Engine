#version 450

layout(location = 0) in vec2  vUV;
layout(location = 1) in float vXNorm;
layout(location = 2) flat in uint  vFlags;
layout(location = 3) flat in float vQuadW;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform sampler2D uAtlas;

layout(push_constant) uniform Push {
    vec4 textColor;    // rgba
    vec4 selectColor;  // rgba
    vec4 caretColor;   // rgba
    vec4 misc;         // x=caretPx, y=caretOn(0/1), z=timeSec, w=mode (0=world, 1=billboard)

    // Billboard-only params (ignored when misc.w==0):
    vec4 bbCenter;     // xyz=center in world, w=unused
    vec4 bbOffsetPx;   // xy=per-glyph pixel offset to top-left (can be {0,0})
    vec4 bbScreenSize; // xy=framebuffer size in pixels
} pc;

void main() {
    const bool isBgQuad    = (vFlags & 8u) != 0u;  // bit3 = background quad
    const bool isSelection = (vFlags & 1u) != 0u;  // bit0 = selection

    if (isBgQuad) {
        if (isSelection) {
            outColor = pc.selectColor;
        } else {
            // caret blink
            float phase = fract(pc.misc.z * 1.0);
            float blink = (phase < 0.5) ? 1.0 : 0.0;
            outColor = vec4(pc.caretColor.rgb, pc.caretColor.a * blink);
        }
        return;
    }

    float a = texture(uAtlas, vUV).r;
    outColor = vec4(pc.textColor.rgb, pc.textColor.a * a);
}
