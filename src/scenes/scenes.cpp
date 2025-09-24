#include "scenes.hpp"
#include "buttons.hpp"
#include "default.hpp"
#include "instancing.hpp"
#include "raytracing.hpp"

Scenes::Scenes() {
    scenesContainer.emplace_back(make_shared<Default>(*this));
    scenesContainer.emplace_back(make_shared<Buttons>(*this));
    scenesContainer.emplace_back(make_shared<RayTracing>(*this));
    scenesContainer.emplace_back(make_shared<Instancing>(*this));
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
