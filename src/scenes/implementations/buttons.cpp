#include "buttons.hpp"
#include "engine.hpp"
#include "objmodel.hpp"
#include "rectangle.hpp"
#include "scenes.hpp"
#include "colors.hpp"
#include "events.hpp"
#include <memory>
#include <optional>

Buttons::Buttons(Scenes &scenes) : Scene(scenes) {
    button = make_unique<Rectangle>(*this, orthographic, screenParams);
    button->setRayTraceOrtho(true);
    button->setRayTraceEnabled(true);
    button->params.outlineColor = Colors::RED;
    button->onMouseEnter = [this]() {
        button->params.outlineColor = Colors::YELLOW;
    };
    button->onMouseExit = [this]() {
        button->params.outlineColor = Colors::RED;
    };
    button->setOnMouseClick([this](int button, int action, int mods){
        if (action == Events::ACTION_PRESS && button == Events::MOUSE_BUTTON_LEFT) {
            std::cout << "Mouse 1 pressed" << std::endl;
            if (this->button->params.color == Colors::RED) {
                this->button->params.color = Colors::GREEN;
                this->button->params.outlineColor = Colors::GREEN;
            } else {
                this->button->params.color = Colors::RED;
                this->button->params.outlineColor = Colors::RED;
            }
        }
    });
}

void Buttons::updateScreenParams() {
    screenParams.viewport.x        = 0.0f;
    screenParams.viewport.y        = (float) Engine::swapChainExtent.height / 2;
    screenParams.viewport.width    = (float) Engine::swapChainExtent.width / 2;
    screenParams.viewport.height   = (float) Engine::swapChainExtent.height / 2;
    screenParams.viewport.minDepth = 0.0f;
    screenParams.viewport.maxDepth = 1.0f;
    screenParams.scissor.offset = {(int32_t)screenParams.viewport.x, (int32_t)screenParams.viewport.y};
    screenParams.scissor.extent = {(uint32_t)screenParams.viewport.width, (uint32_t)screenParams.viewport.height};
}

void Buttons::swapChainUpdate() {
    orthographic.proj = ortho(0.0f, screenParams.viewport.width, 0.0f, -screenParams.viewport.height, -1.0f, 1.0f);
    button->updateUniformBuffer(
        translate(orthographic.model, vec3(screenParams.viewport.width * 0.5f, screenParams.viewport.height * 0.5f, 0.0f)) *
        scale(orthographic.model, vec3(250.0f, 100.0f, 1.0f)),
        std::nullopt,
        orthographic.proj
    );
}

void Buttons::updateComputeUniformBuffers() {}

void Buttons::computePass() {}

void Buttons::updateUniformBuffers() {
    
}

void Buttons::renderPass() {
    button->render();
}
