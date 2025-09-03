#include "scene.hpp"
#include "scenes.hpp"

Scene::Scene(Scenes &scenes) : scenes(scenes) {
    screenParams.viewport.x        = 0.0f;
    screenParams.viewport.y        = 0.0f;
    screenParams.viewport.width    = (float) Engine::swapChainExtent.width;
    screenParams.viewport.height   = (float) Engine::swapChainExtent.height;
    screenParams.viewport.minDepth = 0.0f;
    screenParams.viewport.maxDepth = 1.0f;
    screenParams.scissor.offset = {0, 0};
    screenParams.scissor.extent = Engine::swapChainExtent;
    updateScreenParams();
}

void Scene::updateScreenParams() {}

void Scene::updateComputeUniformBuffers() {}
void Scene::computePass() {}

void Scene::updateUniformBuffers() {}
void Scene::renderPass() {}
void Scene::swapChainUpdate() {}
