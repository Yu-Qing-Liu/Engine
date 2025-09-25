#include "scenes.hpp"
#include "buttons.hpp"
#include "default.hpp"
#include "instancing.hpp"
#include "raytracing.hpp"

Scenes::Scenes() {
    blur = std::make_unique<BlurPipeline>(nullptr);
    blur->createCopyPipeAndSets();
    vp.x = 0.0f;
    vp.y = 0.0f;
    vp.width  = static_cast<float>(Engine::swapChainExtent.width);
    vp.height = static_cast<float>(Engine::swapChainExtent.height);
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;
	sc.offset = {(int32_t)vp.x, (int32_t)vp.y};
	sc.extent = {(uint32_t)vp.width, (uint32_t)vp.height};
    blur->updateCopyViewport(vp, sc);

    scenesContainer.emplace_back(make_shared<Default>(*this));
    scenesContainer.emplace_back(make_shared<Buttons>(*this));
    scenesContainer.emplace_back(make_shared<Instancing>(*this));
    scenesContainer.emplace_back(make_shared<RayTracing>(*this));
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

void Scenes::renderPass1() {
    blur->copy(Engine::currentCommandBuffer());
    for (const auto &sc : scenes) {
        if (sc.second.show) {
            sc.second.scene->renderPass1();
        }
    }
}

void Scenes::swapChainUpdate() {
    vp.width  = static_cast<float>(Engine::swapChainExtent.width);
    vp.height = static_cast<float>(Engine::swapChainExtent.height);
	sc.offset = {(int32_t)vp.x, (int32_t)vp.y};
	sc.extent = {(uint32_t)vp.width, (uint32_t)vp.height};
    blur->updateCopyViewport(vp, sc);
    for (const auto &sc : scenes) {
        if (sc.second.show) {
            sc.second.scene->updateScreenParams();
            sc.second.scene->swapChainUpdate();
        }
    }
}
