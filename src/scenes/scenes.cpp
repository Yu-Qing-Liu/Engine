#include "scenes.hpp"
#include "graph.hpp"
#include "overlay.hpp"

Scenes::Scenes() {
    scenesContainer.emplace_back(make_shared<Graph>(*this));
    scenesContainer.emplace_back(make_shared<Overlay>(*this));
    for (const auto &sc : scenesContainer) {
        scenes[sc->getName()] = {sc, true};
    }
}

void Scenes::showScene(const string &sceneName) {
    scenes[sceneName].show = true;
}

void Scenes::hideScene(const string &sceneName) {
    scenes[sceneName].show = false;
}

shared_ptr<Scene> Scenes::getScene(const string &sceneName) {
    return scenes[sceneName].scene;
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
