#version 450

// Per-vertex (binding 0)
layout(location = 0) in vec3 inPos;
// unit quad: x in [-0.5, 0.5] (along the segment), z in [-0.5, 0.5] (across)

// Per-instance (binding 1)
layout(location = 1) in vec4 iM0;
layout(location = 2) in vec4 iM1;
layout(location = 3) in vec4 iM2;
layout(location = 4) in vec4 iM3;
layout(location = 5) in vec3 iP1;
layout(location = 6) in vec3 iP2;
layout(location = 7) in vec4 iColor;
layout(location = 8) in float iLineWidth;

// Global camera UBO
layout(set = 0, binding = 0) uniform VP {
    mat4 view;
    mat4 proj;
    uint billboard; // unused here, kept for layout compatibility
} cam;

// Push constants for viewport size (in pixels)
layout(push_constant) uniform LinePC {
    vec2 viewportSize; // (width, height)
} pc;

// To fragment
layout(location = 0) out vec4 vColor;

void main()
{
    // Reconstruct model matrix (these are columns; match your C++ layout)
    mat4 model = mat4(iM0, iM1, iM2, iM3);

    // Endpoints in world → clip space
    vec4 p1World = model * vec4(iP1, 1.0);
    vec4 p2World = model * vec4(iP2, 1.0);

    vec4 p1Clip = cam.proj * cam.view * p1World;
    vec4 p2Clip = cam.proj * cam.view * p2World;

    // Base point on the segment for this vertex (in clip)
    // map inPos.x ∈ [-0.5, 0.5] → t ∈ [0, 1]
    float t = clamp(inPos.x + 0.5, 0.0, 1.0);
    vec4 baseClip = mix(p1Clip, p2Clip, t);

    // Segment direction in NDC
    vec2 p1NDC = p1Clip.xy / p1Clip.w;
    vec2 p2NDC = p2Clip.xy / p2Clip.w;

    vec2 segDir = p2NDC - p1NDC;
    float segLen = length(segDir);
    if (segLen < 1e-6) {
        // Degenerate: just pick some direction
        segDir = vec2(1.0, 0.0);
        segLen = 1.0;
    }
    segDir /= segLen;

    // Screen-space perpendicular (in NDC)
    vec2 sideDir = vec2(-segDir.y, segDir.x);

    // lineWidth is in pixels; convert to NDC units:
    // NDC range is [-1, 1], so 1 pixel = 2 / viewportSize
    float halfWidthPx = 0.5 * iLineWidth;
    vec2 pixelToNDC = 2.0 / pc.viewportSize;
    vec2 offsetNDC = sideDir * halfWidthPx * pixelToNDC;

    // Use inPos.z sign to pick which side of the line
    float sideSign = (inPos.z >= 0.0) ? 1.0 : -1.0;
    offsetNDC *= sideSign;

    // Apply offset in NDC to the base point,
    // then convert back to clip space
    vec2 baseNDC = baseClip.xy / baseClip.w;
    vec2 finalNDC = baseNDC + offsetNDC;

    gl_Position = vec4(finalNDC * baseClip.w, baseClip.z, baseClip.w);

    vColor = iColor;
}
