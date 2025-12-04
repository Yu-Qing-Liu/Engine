#version 450

layout(location = 0) in vec2 vUV;
layout(location = 1) in vec4 vColor;
layout(location = 2) in float vSdfPx;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 2) uniform sampler2D uAtlas;

layout(push_constant) uniform PC {
    mat4 uModel;
    float uTime;
    float textOriginX;
    float textOriginY;
    float textExtentX;
    float textExtentY;
} pc;

void main() {
    // Decorations (selection/caret) are tagged with vSdfPx <= 0 → solid color, no texture.
    if (vSdfPx <= 0.0) {
        if (vColor.a <= 1e-4) discard;

        // Heuristic: caret is fully opaque, selection is semi-transparent.
        float isCaret = step(0.9, vColor.a);

        // Blink: square wave, 1 Hz, 50% duty cycle.
        const float blinkHz = 1.0 / 1000.0;
        const float duty = 0.5; // visible fraction of each period
        float phase = fract(pc.uTime * blinkHz); // 0..1
        float on = step(1.0 - duty, phase); // 0 or 1

        // Only apply blinking to caret; selections stay steady.
        float visibility = mix(1.0, on, isCaret);

        outColor = vec4(vColor.rgb, vColor.a * visibility);
        return;
    }

    // SDF text path
    float s = texture(uAtlas, vUV).r; // 0..1, 0.5 at edge
    float aa = 0.5 * fwidth(s);
    float alpha = smoothstep(0.5 - aa, 0.5 + aa, s);
    if (alpha <= 1e-4) discard;

    outColor = vec4(vColor.rgb, vColor.a * alpha);
}
