#version 460
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec2 vUV;
layout(location = 1) flat in uint vFrameIndex;
layout(location = 2) in vec4 vColor;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D uTex[];

void main()
{
    vec4 tex = texture(uTex[nonuniformEXT(vFrameIndex)], vUV);

    // Use texture alpha as mask, vertex color as actual color:
    float alpha = tex.a;
    outColor = vec4(vColor.rgb, vColor.a * alpha);
}
