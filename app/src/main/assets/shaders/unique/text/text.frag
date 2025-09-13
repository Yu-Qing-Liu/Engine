#version 450

layout(location = 0) in vec2 vUV;
layout(location = 1) in float vXNorm;
layout(location = 2) flat in uint vFlags;
layout(location = 3) flat in float vQuadW;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform sampler2D uAtlas;

layout(push_constant) uniform Push {
    vec4 textColor; // rgba
    vec4 selectColor; // rgba (bg for selected glyphs)
    vec4 caretColor; // rgba
    vec4 misc; // x=caretPx, y=unused, z=timeSeconds, w=unused
} pc;

void main() {
    const bool isBgQuad    = (vFlags & 8u) != 0u;  // bit3 = background quad (caret OR selection)
    const bool isSelection = (vFlags & 1u) != 0u;  // bit0 = selection

    if (isBgQuad) {
        if (isSelection) {
            // Selection background: solid fill covering the entire bg quad
            outColor = pc.selectColor;
        } else {
            // Caret background: blink
            float phase = fract(pc.misc.z * 1.0);
            float blinkGate = (phase < 0.5) ? 1.0 : 0.0;
            outColor = vec4(pc.caretColor.rgb, pc.caretColor.a * blinkGate);
        }
        return;
    }

    // Regular glyph quad
    float a = texture(uAtlas, vUV).r;
    vec4 fg = vec4(pc.textColor.rgb, pc.textColor.a * a);
    outColor = fg;
}
