#include "instancing.hpp"
#include "engine.hpp"
#include "objmodel.hpp"
#include "scenes.hpp"
#include <memory>
#include <optional>


Instancing::Instancing(Scenes &scenes) : Scene(scenes) {
    cells = std::make_shared<std::unordered_map<int, InstancedRectangle::InstanceData>>(
        std::initializer_list<std::pair<const int, InstancedRectangle::InstanceData>>{
            {0, {screenParams.viewport.width * 0.25f, screenParams.viewport.height * 0.25f, {100, 100}}},
            {1, {screenParams.viewport.width * 0.75f, screenParams.viewport.height * 0.25f, {100, 100}}},
            {2, {screenParams.viewport.width * 0.25f, screenParams.viewport.height * 0.75f, {100, 100}}},
            {3, {screenParams.viewport.width * 0.75f, screenParams.viewport.height * 0.75f, {100, 100}}},
        }
    );
    grid = make_unique<InstancedRectangle>(orthographic, screenParams, cells, 4);
}

void Instancing::updateScreenParams() {
    screenParams.viewport.x        = (float) Engine::swapChainExtent.width / 2;
    screenParams.viewport.y        = (float) Engine::swapChainExtent.height / 2;
    screenParams.viewport.width    = (float) Engine::swapChainExtent.width / 2;
    screenParams.viewport.height   = (float) Engine::swapChainExtent.height / 2;
    screenParams.viewport.minDepth = 0.0f;
    screenParams.viewport.maxDepth = 1.0f;
    screenParams.scissor.offset = {(int32_t)screenParams.viewport.x, (int32_t)screenParams.viewport.y};
    screenParams.scissor.extent = {(uint32_t)screenParams.viewport.width, (uint32_t)screenParams.viewport.height};
}

void Instancing::swapChainUpdate() {
	orthographic.proj = ortho(0.0f, screenParams.viewport.width, 0.0f, -screenParams.viewport.height, -1.0f, 1.0f);
    grid->updateInstance(0,{screenParams.viewport.width * 0.25f, screenParams.viewport.height * 0.25f, {100, 100}});
    grid->updateInstance(1,{screenParams.viewport.width * 0.25f, screenParams.viewport.height * 0.75f, {100, 100}});
    grid->updateInstance(2,{screenParams.viewport.width * 0.75f, screenParams.viewport.height * 0.25f, {100, 100}});
    grid->updateInstance(3,{screenParams.viewport.width * 0.75f, screenParams.viewport.height * 0.75f, {100, 100}});
    grid->updateUniformBuffer(std::nullopt, std::nullopt, orthographic.proj);
}

void Instancing::updateComputeUniformBuffers() {}

void Instancing::computePass() {}

void Instancing::updateUniformBuffers() {}

void Instancing::renderPass() {
    grid->render();
}
