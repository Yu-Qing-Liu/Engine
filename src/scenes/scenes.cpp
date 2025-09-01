#include "scenes.hpp"
#include "default.hpp"
#include "engine.hpp"

Scenes::Scenes() {
    scenes.emplace(Engine::DEFAULT, std::make_unique<Default>());
}

void Scenes::renderPass() {
    scenes[Engine::currentScene]->renderPass();
}

void Scenes::swapChainUpdate() {
    scenes[Engine::currentScene]->swapChainUpdate();
}
