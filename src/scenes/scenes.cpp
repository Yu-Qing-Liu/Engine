#include "scenes.hpp"
#include "default.hpp"

Scenes::Scenes() {
    SceneEntry defaultScene = {
        make_unique<Default>(*this),
        true
    };
    scenes.emplace(defaultScene.scene->getName(), std::move(defaultScene));
}

void Scenes::showScene(const string &sceneName) {
    scenes[sceneName].show = true;
}

void Scenes::hideScene(const string &sceneName) {
    scenes[sceneName].show = false;
}

void Scenes::renderPass() {
    for (const auto &sc : scenes) {
        if (sc.second.show) {
            sc.second.scene->renderPass();
        }
    }
}

void Scenes::swapChainUpdate() {
    for (const auto &sc : scenes) {
        if (sc.second.show) {
            sc.second.scene->swapChainUpdate();
        }
    }
}
