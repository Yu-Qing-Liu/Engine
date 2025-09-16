#include "scenes.hpp"
#include "default.hpp"
#include "instancing.hpp"
#include "raytracing.hpp"
#include "buttons.hpp"

Scenes::Scenes() {
    scenes[Default::getName()] = {
        make_unique<Default>(*this),
        true
    };
    scenes[RayTracing::getName()] = {
        make_unique<RayTracing>(*this),
        true
    };
    scenes[Buttons::getName()] = {
        make_unique<Buttons>(*this),
        true
    };
    scenes[Instancing::getName()] = {
        make_unique<Instancing>(*this),
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
