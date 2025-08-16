#include "scenes.hpp"
#include "default.hpp"

Scenes::Scenes() {
    scenes.emplace(DEFAULT, std::make_unique<Default>());
}

void Scenes::render() {
    scenes[DEFAULT]->renderPass();
}

void Scenes::drawFrames() {
    for(const auto &scene : scenes) {
        scene.second->drawFrame();
    }
}
