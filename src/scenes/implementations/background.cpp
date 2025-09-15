#include "background.hpp"
#include "assets.hpp"

Background::Background(Scenes &scenes) : Scene(scenes) {
    kitchen = make_unique<Object>(this, kitchenUBO, screenParams, Assets::modelRootPath + "/kitchen/kitchen.obj"); 
}

void Background::updateScreenParams() {
	screenParams.viewport.x = 0.0f;
	screenParams.viewport.y = 0.0f;
	screenParams.viewport.width = (float)Engine::swapChainExtent.width;
	screenParams.viewport.height = (float)Engine::swapChainExtent.height;
	screenParams.viewport.minDepth = 0.0f;
	screenParams.viewport.maxDepth = 1.0f;
	screenParams.scissor.offset = {(int32_t)screenParams.viewport.x, (int32_t)screenParams.viewport.y};
	screenParams.scissor.extent = {(uint32_t)screenParams.viewport.width, (uint32_t)screenParams.viewport.height};
}

void Background::swapChainUpdate() {
    kitchenUBO.proj = perspective(radians(45.0f), screenParams.viewport.width / screenParams.viewport.height, 0.1f, 20.0f);
	kitchen->updateUniformBuffer(std::nullopt, std::nullopt, kitchenUBO.proj);
}

void Background::updateComputeUniformBuffers() {}

void Background::computePass() {}

void Background::updateUniformBuffers() {}

void Background::renderPass() { 
    kitchen->render(); 
}
