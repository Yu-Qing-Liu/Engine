#include "models/implementations/triangle.hpp"
#include "engineutils.hpp"
#include <cstring>
#include <vulkan/vulkan_core.h>

using namespace glm;

Triangle::Triangle() :
Model(
    Engine::shaderRootPath + "/triangle", 
    {
        {{0.0f, 0.5f}, {1.0f, 0.0f, 0.0f}},
        {{-0.433f, -0.25f}, {0.0f, 1.0f, 0.0f}},
        {{0.433f, -0.25f}, {0.0f, 0.0f, 1.0f}},
    },
    {
        0, 1, 2
    }
) {}

void Triangle::updateUniformBuffer() {
    setUniformBuffer(
        mat4(1.0f),
        lookAt(vec3(0.0f, 0.1f, 2.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, -2.0f)),
        perspective(radians(45.0f), Engine::swapChainExtent.width / (float) Engine::swapChainExtent.height, 0.1f, 10.0f)
    );
}
