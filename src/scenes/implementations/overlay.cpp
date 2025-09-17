#include "overlay.hpp"
#include "main.hpp"

Overlay::Overlay(Scenes &scenes) : Scene(scenes) {
	crosshair = Shapes::icon(this, orthographic, screenParams, Assets::textureRootPath + "/crosshair/crosshair.png");
}

void Overlay::updateScreenParams() {
	screenParams.viewport.x = 0.0f;
	screenParams.viewport.y = 0.0f;
	screenParams.viewport.width = (float)Engine::swapChainExtent.width;
	screenParams.viewport.height = (float)Engine::swapChainExtent.height;
	screenParams.viewport.minDepth = 0.0f;
	screenParams.viewport.maxDepth = 1.0f;
	screenParams.scissor.offset = {(int32_t)screenParams.viewport.x, (int32_t)screenParams.viewport.y};
	screenParams.scissor.extent = {(uint32_t)screenParams.viewport.width, (uint32_t)screenParams.viewport.height};
}

void Overlay::swapChainUpdate() {
	auto w = screenParams.viewport.width;
	auto h = screenParams.viewport.height;
	orthographic.proj = ortho(0.0f, w, 0.0f, -h, -1.0f, 1.0f);

	float crosshairSize = 50.0f;
	crosshair->updateUniformBuffer(
        translate(mat4(1.0f), vec3(w * 0.5, h * 0.5, 0.0)) * scale(mat4(1.0f), vec3(crosshairSize)),
        std::nullopt,
        orthographic.proj
    );
}

void Overlay::updateComputeUniformBuffers() {}

void Overlay::computePass() {}

void Overlay::updateUniformBuffers() {}

void Overlay::renderPass() { crosshair->render(); }
