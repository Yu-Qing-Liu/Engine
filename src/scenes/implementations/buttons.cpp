#include "buttons.hpp"
#include "colors.hpp"
#include "engine.hpp"
#include "fonts.hpp"
#include "scenes.hpp"

Buttons::Buttons(Scenes &scenes) : Scene(scenes) {
    background = make_unique<Texture>(
        this,
        orthographic,
        screenParams,
        Assets::textureRootPath + "/example/example.png", 
        std::vector<Texture::Vertex> {
            {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
            {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
            {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
            {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
        },
        std::vector<uint32_t> {
            0, 1, 2, 2, 3, 0,
        }
    );

	button = make_unique<Button>(this, orthographic, screenParams, Text::TextParams{Fonts::ArialBold, 16});
	button->setOnMouseClick([this](int button, int action, int mods) {
		if (action == Events::ACTION_PRESS && button == Events::MOUSE_BUTTON_LEFT) {
			std::cout << "Mouse 1 pressed" << std::endl;
			if (this->button->container->params.color == Colors::White) {
				this->button->container->params.color = Colors::Green;
				this->button->container->params.outlineColor = Colors::Green;
			} else {
				this->button->container->params.color = Colors::White;
				this->button->container->params.outlineColor = Colors::White;
			}
		}
	});
	button->setOnMouseEnter([this]() {
		std::cout << "Mouse Entered" << std::endl;
		button->container->params.outlineColor = Colors::Yellow;
	});
	button->setOnMouseExit([this]() {
		std::cout << "Mouse Exited" << std::endl;
		button->container->params.outlineColor = button->container->params.color;
	});

	textInput = make_unique<TextInput>(this, orthographic, screenParams, Text::TextParams{Fonts::ArialBold, 16});
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

    auto width = screenParams.viewport.width;
    auto height = screenParams.viewport.height;
    float size = 1000.0f;
    background->updateUniformBuffer(translate(mat4(1.0f), vec3((float) width / 2.0f, (float) height / 2.0f, 0.0f)) * scale(mat4(1.0f), vec3(size, size, 1.0f)), std::nullopt, orthographic.proj);

	button->updateUniformBuffers(orthographic);
	Button::StyleParams bp;
	bp.center = {screenParams.viewport.width * 0.25f, screenParams.viewport.height * 0.25f};
	bp.textCenter = {screenParams.viewport.width * 0.25f, screenParams.viewport.height * 0.25f};
	bp.dim = {100.0f, 40.0f};
	bp.outlineWidth = 5.0f;
	bp.borderRadius = 16.0f;
	bp.text = std::string("Click me!");
	button->setParams(bp);

	textInput->updateUniformBuffers(orthographic);
	TextInput::StyleParams tp;
	tp.center = {screenParams.viewport.width * 0.75f, screenParams.viewport.height * 0.25f};
	tp.textCenter = {screenParams.viewport.width * 0.75, screenParams.viewport.height * 0.25f};
	tp.dim = {200.0f, 40.0f};
	tp.outlineWidth = 3.0f;
	tp.borderRadius = 8.0f;
	textInput->setParams(tp);
}

void Buttons::updateComputeUniformBuffers() {}

void Buttons::computePass() {}

void Buttons::updateUniformBuffers() {}

void Buttons::renderPass() { 
    background->render();
    button->render(); 
    textInput->render();
}
