#include "header.hpp"
#include "assets.hpp"

Header::Header(Scenes &scenes) : Scene(scenes) {
    fridge = make_unique<Object>(this, fridgeUBO, fridgeVP, Assets::modelRootPath + "/fridge/fridge.obj"); 
}

void Header::updateScreenParams() {
	screenParams.viewport.x = 0.0f;
	screenParams.viewport.y = 0.0f;
	screenParams.viewport.width = (float)Engine::swapChainExtent.width;
	screenParams.viewport.height = (float)Engine::swapChainExtent.height;
	screenParams.viewport.minDepth = 0.0f;
	screenParams.viewport.maxDepth = 1.0f;
	screenParams.scissor.offset = {(int32_t)screenParams.viewport.x, (int32_t)screenParams.viewport.y};
	screenParams.scissor.extent = {(uint32_t)screenParams.viewport.width, (uint32_t)screenParams.viewport.height};
}

void Header::swapChainUpdate() {
    float headerH = Engine::swapChainExtent.height / 10.0f;
    float boxSize = headerH;
    float right   = static_cast<float>(Engine::swapChainExtent.width);
    fridgeVP.viewport = {
        right - boxSize,                 // x: top-right corner
        0.0f,
        boxSize,
        boxSize,
        0.0f,
        1.0f
    };
    fridgeVP.scissor = {
        { static_cast<int32_t>(right - boxSize), 0 },
        { static_cast<uint32_t>(boxSize), static_cast<uint32_t>(boxSize) }
    };
    fridgeUBO.proj = perspective(radians(45.0f), fridgeVP.viewport.width / fridgeVP.viewport.height, 0.1f, 20.0f);
	fridge->updateUniformBuffer(rotate(mat4(1.0f), radians(90.0f), vec3(0.0f, 0.0f, 1.0f)), std::nullopt, fridgeUBO.proj);
}

void Header::updateComputeUniformBuffers() {}

void Header::computePass() {}

void Header::updateUniformBuffers() {}

void Header::renderPass() { fridge->render(); }
