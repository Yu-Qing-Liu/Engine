#include "scenes.hpp"
#include "default.hpp"

Scenes::Scenes() {
    scenes.emplace(Engine::DEFAULT, std::make_unique<Default>());
}

void Scenes::renderPass() {
    scenes[Engine::currentScene]->drawFrame();
}
