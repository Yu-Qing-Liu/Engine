#include "main.hpp"
#include "engine.hpp"
#include "polygon.hpp"
#include "scenes.hpp"

Main::Main(Scenes &scenes) : Scene(scenes) {
    triangle = make_unique<Polygon>(
        this,
        persp,
        screenParams,
        std::vector<Polygon::Vertex> {
            {{0.0f, 0.5f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{-0.433f, -0.25f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
            {{0.433f, -0.25f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
        },
        std::vector<uint32_t> {
            0, 1, 2
        }
    );
}

void Main::updateScreenParams() {
    screenParams.viewport.x        = 0.0f;
    screenParams.viewport.y        = 0.0f;
    screenParams.viewport.width    = (float) Engine::swapChainExtent.width;
    screenParams.viewport.height   = (float) Engine::swapChainExtent.height;
    screenParams.viewport.minDepth = 0.0f;
    screenParams.viewport.maxDepth = 1.0f;
    screenParams.scissor.offset = {(int32_t)screenParams.viewport.x, (int32_t)screenParams.viewport.y};
    screenParams.scissor.extent = {(uint32_t)screenParams.viewport.width, (uint32_t)screenParams.viewport.height};
}

void Main::swapChainUpdate() {
    triangle->updateUniformBuffer(std::nullopt, std::nullopt, persp.proj);
}

void Main::updateComputeUniformBuffers() {
}

void Main::computePass() {
}

void Main::updateUniformBuffers() {
    triangle->updateUniformBuffer(rotate(mat4(1.0f), Engine::time * radians(90.0f), vec3(0.0f, 0.0f, 1.0f)));
}

void Main::renderPass() {
    triangle->render();
}
