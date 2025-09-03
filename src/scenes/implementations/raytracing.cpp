#include "raytracing.hpp"
#include "engine.hpp"
#include "objmodel.hpp"
#include "polygon.hpp"
#include "scenes.hpp"
#include <memory>
#include <optional>

RayTracing::RayTracing(Scenes &scenes) : Scene(scenes) {
    this->cube = make_unique<Polygon>(
        std::vector<Model::Vertex> {
            {{0.0f, 0.5f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{-0.433f, -0.25f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
            {{0.433f, -0.25f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
        },
        std::vector<uint16_t> {
            0, 1, 2
        }
    );
}

void RayTracing::updateScreenParams() {
    screenParams.viewport.x        = 0.0f;
    screenParams.viewport.y        = 0.0f;
    screenParams.viewport.width    = (float) Engine::swapChainExtent.width;
    screenParams.viewport.height   = (float) Engine::swapChainExtent.height;
    screenParams.viewport.minDepth = 0.0f;
    screenParams.viewport.maxDepth = 1.0f;
    screenParams.scissor.offset = {0, 0};
    screenParams.scissor.extent = Engine::swapChainExtent;
}

void RayTracing::updateComputeUniformBuffers() {

}

void RayTracing::computePass() {

}

void RayTracing::updateUniformBuffers() {
    cube->updateUniformBuffer();
}

void RayTracing::swapChainUpdate() {
    updateScreenParams();

    cube->updateUniformBuffer(std::nullopt, std::nullopt, perspective(radians(45.0f), screenParams.viewport.width / screenParams.viewport.height, 0.1f, 10.0f));
}

void RayTracing::renderPass() {
    cube->render(persp, screenParams);
}
