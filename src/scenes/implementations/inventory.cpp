#include "inventory.hpp"

Inventory::Inventory(Scenes &scenes, bool show) : Scene(scenes, show) {
	mvp = {mat4(1.0f), mat4(1.0f), ortho(0.0f, float(Engine::swapChainExtent.width), 0.0f, -float(Engine::swapChainExtent.height), -1.0f, 1.0f)};
	grid = make_unique<Grid>(this, mvp, screenParams);
	grid->grid->enableBlur(Assets::shaderRootPath + "/instanced/blur/irectblur/");
	grid->scrollBar->enableBlur(Assets::shaderRootPath + "/instanced/blur/irectblur/");
}

void Inventory::updateScreenParams() {
	screenParams.viewport.x = 0.0f;
	screenParams.viewport.y = 0.0f;
	screenParams.viewport.width = (float)Engine::swapChainExtent.width;
	screenParams.viewport.height = (float)Engine::swapChainExtent.height;
	screenParams.viewport.minDepth = 0.0f;
	screenParams.viewport.maxDepth = 1.0f;
	screenParams.scissor.offset = {(int32_t)screenParams.viewport.x, (int32_t)screenParams.viewport.y};
	screenParams.scissor.extent = {(uint32_t)screenParams.viewport.width, (uint32_t)screenParams.viewport.height};
}

void Inventory::swapChainUpdate() {
	const auto w = (float)screenParams.viewport.width;
	const auto h = (float)screenParams.viewport.height;
    const float padT = 100;
    const float usableH = h - padT * 2;
	grid->styleParams.gridCenter = vec2(w * 0.5, padT + usableH * 0.5f);
	grid->styleParams.gridDim = vec2(w * 0.95, usableH);
    grid->styleParams.cellSize = vec2(180.f);
	grid->swapChainUpdate();
}

void Inventory::updateComputeUniformBuffers() {}

void Inventory::computePass() {}

void Inventory::updateUniformBuffers() { grid->updateUniformBuffers(); }

void Inventory::renderPass() {}

void Inventory::renderPass1() { grid->render(); }
