#include "models/implementations/rectangle.hpp"
#include "engineutils.hpp"
#include <chrono>
#include <cstring>
#include <vulkan/vulkan_core.h>

using namespace glm;

Rectangle::Rectangle() :
Model(
    Engine::shaderRootPath + "/rectangle", 
    {
        {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
        {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
        {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
        {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
    },
    {
        0, 1, 2, 2, 3, 0
    }
) {}

void Rectangle::updateUniformBuffer() {
    static auto startTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    setUniformBuffer(
        rotate(mat4(1.0f), time * radians(90.0f), vec3(0.0f, 0.0f, 1.0f)), 
        lookAt(vec3(2.0f, 2.0f, 2.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 1.0f)),
        perspective(radians(45.0f), Engine::swapChainExtent.width / (float) Engine::swapChainExtent.height, 0.1f, 10.0f)
    );
}
