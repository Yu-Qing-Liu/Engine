#include "scenes.hpp"
#include "main.hpp"
#include "overlay.hpp"

Scenes::Scenes() {
    scenes[Main::getName()] = {
        make_unique<Main>(*this),
        true
    };
    scenes[Overlay::getName()] = {
        make_unique<Overlay>(*this),
        true
    };
}

void Scenes::showScene(const string &sceneName) {
    scenes[sceneName].show = true;
}

void Scenes::hideScene(const string &sceneName) {
    scenes[sceneName].show = false;
}

void Scenes::updateComputeUniformBuffers() {
    for (const auto &sc : scenes) {
        if (sc.second.show) {
            sc.second.scene->updateRayTraceUniformBuffers();
            sc.second.scene->updateComputeUniformBuffers();
        }
    }
}

void Scenes::computePass() {
    for (const auto &sc : scenes) {
        if (sc.second.show) {
            sc.second.scene->rayTraces();
            sc.second.scene->computePass();
        }
    }
}

void Scenes::updateUniformBuffers() {
    for (const auto &sc : scenes) {
        if (sc.second.show) {
            sc.second.scene->updateUniformBuffers();
        }
    }
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
            sc.second.scene->updateScreenParams();
            sc.second.scene->swapChainUpdate();
        }
    }
}
