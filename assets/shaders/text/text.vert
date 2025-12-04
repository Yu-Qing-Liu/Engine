#version 450

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec4 inColor;
layout(location = 3) in float inSdfPx;

layout(location = 0) out vec2 vUV;
layout(location = 1) out vec4 vColor;
layout(location = 2) out float vSdfPx;

layout(std140, set = 0, binding = 0) uniform VP {
    mat4 view;
    mat4 proj;
    uint billboard;
} cam;

// IMPORTANT: same layout in *both* VS and FS
layout(push_constant) uniform PC {
    mat4 uModel;
    float uTime;
    float textOriginX;
    float textOriginY;
    float textExtentX;
    float textExtentY;
} pc;

void main() {
    vUV = inUV;
    vColor = inColor;
    vSdfPx = inSdfPx;

    if (cam.billboard != 0u) {
        // ---- View-space billboard for text ----

        // 1) Anchor: model origin in world space
        vec4 centerWorld = pc.uModel * vec4(0.0, 0.0, 0.0, 1.0);

        // 2) Move anchor to view space
        vec4 centerView = cam.view * centerWorld;

        // 3) Text-local position
        vec2 localCenter = vec2(pc.textOriginX, pc.textOriginY);
        vec2 localOffset = inPos - localCenter; // layout units

        // If characters are vertically flipped, uncomment this:
        localOffset.y = -localOffset.y;

        // 4) Convert text units -> view-space units
        // Tune this or pass as uniform/push constant
        const float textToViewScale = 0.01;

        // Optional: include uniform scaling from the model matrix if you want
        float sx = length(pc.uModel[0].xyz); // magnitude only
        float sy = length(pc.uModel[1].xyz);

        // If you know your model is just 2D scale + translation (no rotation),
        // you can preserve sign instead of using length():
        // float sx = pc.uModel[0][0];
        // float sy = pc.uModel[1][1];

        vec3 right = vec3(1.0, 0.0, 0.0); // view-space +X = right
        vec3 up = vec3(0.0, 1.0, 0.0); // view-space +Y = up

        vec3 offsetView =
            right * (localOffset.x * sx * textToViewScale) +
                up * (localOffset.y * sy * textToViewScale);

        vec4 viewPos = centerView + vec4(offsetView, 0.0);

        // 5) Project normally – depth acts like any other geometry
        gl_Position = cam.proj * viewPos;
    } else {
        vec4 world = pc.uModel * vec4(inPos, 0.0, 1.0);
        gl_Position = cam.proj * cam.view * world;
    }
}
