#include "buttons.hpp"
#include "engine.hpp"
#include "scenes.hpp"
#include <memory>

Buttons::Buttons(Scenes &scenes) : Scene(scenes) {
    button = make_unique<Button>(*this, orthographic, screenParams, 24); 
}

void Buttons::updateScreenParams() {
	screenParams.viewport.x = 0.0f;
	screenParams.viewport.y = (float)Engine::swapChainExtent.height / 2;
	screenParams.viewport.width = (float)Engine::swapChainExtent.width / 2;
	screenParams.viewport.height = (float)Engine::swapChainExtent.height / 2;
	screenParams.viewport.minDepth = 0.0f;
	screenParams.viewport.maxDepth = 1.0f;
	screenParams.scissor.offset = {(int32_t)screenParams.viewport.x, (int32_t)screenParams.viewport.y};
	screenParams.scissor.extent = {(uint32_t)screenParams.viewport.width, (uint32_t)screenParams.viewport.height};
}

void Buttons::swapChainUpdate() {
	orthographic.proj = ortho(0.0f, screenParams.viewport.width, 0.0f, -screenParams.viewport.height, -1.0f, 1.0f);

    button->updateUniformBuffers(orthographic);

	Button::StyleParams p;
	p.center = {screenParams.viewport.width * 0.5f, screenParams.viewport.height * 0.5f};
	p.dim = {250.0f, 100.0f};
	p.bgColor = {1.0f, 1.0f, 1.0f, 1.0f};	   // white
	p.outlineColor = {0.3f, 0.3f, 0.3f, 1.0f}; // grey border
	p.outlineWidth = 5.0f;					   // 1 px
	p.borderRadius = 16.0f;					   // rounded
	p.text = "Click me!";
	p.textColor = vec4(0.0f, 0.0f, 0.0f, 1.0f); // black

	button->setParams(p);
}

void Buttons::updateComputeUniformBuffers() {}

void Buttons::computePass() {}

void Buttons::updateUniformBuffers() {}

void Buttons::renderPass() { button->render(); }
