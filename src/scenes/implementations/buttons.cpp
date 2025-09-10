#include "buttons.hpp"
#include "colors.hpp"
#include "engine.hpp"
#include "fonts.hpp"
#include "scenes.hpp"

Buttons::Buttons(Scenes &scenes) : Scene(scenes) {
	button = make_unique<Button>(*this, orthographic, screenParams, Text::TextParams{Fonts::ArialBold, 16});
	button->setOnMouseClick([this](int button, int action, int mods) {
		if (action == Events::ACTION_PRESS && button == Events::MOUSE_BUTTON_LEFT) {
			std::cout << "Mouse 1 pressed" << std::endl;
			if (this->button->container->params.color == Colors::WHITE) {
				this->button->container->params.color = Colors::GREEN;
				this->button->container->params.outlineColor = Colors::GREEN;
			} else {
				this->button->container->params.color = Colors::WHITE;
				this->button->container->params.outlineColor = Colors::WHITE;
			}
		}
	});
	button->setOnMouseEnter([this]() {
		std::cout << "Mouse Entered" << std::endl;
		button->container->params.outlineColor = Colors::YELLOW;
	});
	button->setOnMouseExit([this]() {
		std::cout << "Mouse Exited" << std::endl;
		button->container->params.outlineColor = button->container->params.color;
	});

	textInput = make_unique<TextInput>(*this, orthographic, screenParams, Text::TextParams{Fonts::ArialBold, 16});
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
	Button::StyleParams bp;
	bp.center = {screenParams.viewport.width * 0.25f, screenParams.viewport.height * 0.25f};
	bp.textCenter = {screenParams.viewport.width * 0.25, screenParams.viewport.height * 0.25f};
	bp.dim = {100.0f, 40.0f};
	bp.bgColor = {1.0f, 1.0f, 1.0f, 1.0f};		// white
	bp.outlineColor = {0.3f, 0.3f, 0.3f, 1.0f}; // grey border
	bp.outlineWidth = 5.0f;						// 1 px
	bp.borderRadius = 16.0f;					// rounded
	bp.text = "Click me!";
	bp.textColor = vec4(0.0f, 0.0f, 0.0f, 1.0f); // black
	button->setParams(bp);

	textInput->updateUniformBuffers(orthographic);
	TextInput::StyleParams tp;
	tp.center = {screenParams.viewport.width * 0.75f, screenParams.viewport.height * 0.25f};
	tp.textCenter = {screenParams.viewport.width * 0.75, screenParams.viewport.height * 0.25f};
	tp.dim = {200.0f, 40.0f};
	tp.bgColor = {1.0f, 1.0f, 1.0f, 0.05f};		// white
	tp.outlineColor = {0.3f, 0.3f, 0.3f, 1.0f}; // grey border
	tp.outlineWidth = 3.0f;						// 1 px
	tp.borderRadius = 8.0f;					// rounded
	tp.placeholderText = "Enter Text!";
	tp.placeholderTextColor = vec4(0.0f, 0.0f, 0.0f, 1.0f); // black
	tp.textColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);			// black
	textInput->setParams(tp);
}

void Buttons::updateComputeUniformBuffers() {}

void Buttons::computePass() {}

void Buttons::updateUniformBuffers() {}

void Buttons::renderPass() { 
    button->render(); 
    textInput->render();
}
