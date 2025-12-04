#version 450

layout(location = 0) in vec3 inPos;

// instance attributes (binding=1)
layout(location = 1) in vec4 iM0;
layout(location = 2) in vec4 iM1;
layout(location = 3) in vec4 iM2;
layout(location = 4) in vec4 iM3;

layout(set = 0, binding = 0) uniform VP {
    mat4 view;
    mat4 proj;
    uint billboard;
} ubo;

layout(location = 0) out vec2 vLocal;

void main() {
    mat4 model = mat4(iM0, iM1, iM2, iM3);

    vLocal = inPos.xy;

    // Normal path: transform as a regular 3D mesh
    vec4 world = model * vec4(inPos, 1.0);
    vec4 clipPos = ubo.proj * ubo.view * world;
    if (ubo.billboard != 0u) {
        // 1) Use the instance's origin as the billboard center
        vec4 centerWorld = model * vec4(0.0, 0.0, 0.0, 1.0);
        vec4 centerClip = ubo.proj * ubo.view * centerWorld;

        // 2) Perspective divide -> NDC [-1,1]
        centerClip /= centerClip.w;

        // 3) Offset in screen space using quad vertex (assumed in [-0.5,0.5])
        //    This gives you a constant on-screen size.
        const vec2 billboardSizeNDC = vec2(0.2, 0.2); // tweak / make a uniform if needed
        centerClip.xy += inPos.xy * billboardSizeNDC;

        // 4) Final position in NDC
        gl_Position = centerClip;
    } else {
        // Normal 3D mesh
        gl_Position = clipPos;
    }
}
