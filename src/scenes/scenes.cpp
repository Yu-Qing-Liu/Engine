#include "scenes.hpp"
#include "background.hpp"
#include "inventory.hpp"
#include "menu.hpp"
#include "navbar.hpp"

Scenes::Scenes() {
	blur = std::make_unique<BlurPipeline>(nullptr);
	blur->createCopyPipeAndSets();
	vp.x = 0.0f;
	vp.y = 0.0f;
	vp.width = static_cast<float>(Engine::swapChainExtent.width);
	vp.height = static_cast<float>(Engine::swapChainExtent.height);
	vp.minDepth = 0.0f;
	vp.maxDepth = 1.0f;
	sc.offset = {(int32_t)vp.x, (int32_t)vp.y};
	sc.extent = {(uint32_t)vp.width, (uint32_t)vp.height};
	blur->updateCopyViewport(vp, sc);

	scenesContainer.emplace_back(make_shared<Background>(*this));
	scenesContainer.emplace_back(make_shared<NavBar>(*this));
	// scenesContainer.emplace_back(make_shared<Menu>(*this));
	scenesContainer.emplace_back(make_shared<Inventory>(*this));
	for (const auto &sc : scenesContainer) {
		scenes[sc->getName()] = {sc, true};
	}
}

void Scenes::showScene(const string &sceneName) { scenes[sceneName].show = true; }

void Scenes::hideScene(const string &sceneName) { scenes[sceneName].show = false; }

shared_ptr<Scene> Scenes::getScene(const string &sceneName) { return scenes[sceneName].scene; }

void Scenes::updateComputeUniformBuffers() {
	for (const auto &sc : scenes) {
		if (sc.second.show) {
			sc.second.scene->updateRayTraceUniformBuffers();
			sc.second.scene->updateComputeUniformBuffers();
		}
	}
}

void Scenes::computePass() {
	Model *globalClosest = nullptr;
	float bestLen = std::numeric_limits<float>::infinity();

	std::vector<Scene *> visibleScenes;
	visibleScenes.reserve(scenes.size());

	for (const auto &sc : scenes) {
		if (!sc.second.show)
			continue;
		visibleScenes.push_back(sc.second.scene.get());

		Scene::ClosestHit hit = sc.second.scene->rayTraces();
		if (hit.model && hit.distance < bestLen) {
			bestLen = hit.distance;
			globalClosest = hit.model;
		}
	}

	for (auto *scene : visibleScenes) {
		scene->applyHover(globalClosest);
		scene->computePass();
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
	for (const auto &scPtr : scenesContainer) {
		auto it = scenes.find(scPtr->getName());
		if (it != scenes.end() && it->second.show) {
			scPtr->renderPass();
		}
	}
}

void Scenes::renderPass1() {
	blur->copy(Engine::currentCommandBuffer());
	for (const auto &scPtr : scenesContainer) {
		auto it = scenes.find(scPtr->getName());
		if (it != scenes.end() && it->second.show) {
			scPtr->renderPass1();
		}
	}
}

void Scenes::swapChainUpdate() {
	vp.width = static_cast<float>(Engine::swapChainExtent.width);
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
