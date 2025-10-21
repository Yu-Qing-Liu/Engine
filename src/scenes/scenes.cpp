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
	vp.width = static_cast<float>(Engine::swapChainExtent.width);
	vp.height = static_cast<float>(Engine::swapChainExtent.height);
	vp.minDepth = 0.0f;
	vp.maxDepth = 1.0f;
	sc.offset = {(int32_t)vp.x, (int32_t)vp.y};
	sc.extent = {(uint32_t)vp.width, (uint32_t)vp.height};
	blur->updateCopyViewport(vp, sc);

	currentScene = "Menu";

	scenesContainer.emplace_back(make_shared<Background>(*this));
	scenesContainer.emplace_back(make_shared<NavBar>(*this));
	scenesContainer.emplace_back(make_shared<Menu>(*this));
	scenesContainer.emplace_back(make_shared<Inventory>(*this, false));
	scenesContainer.emplace_back(make_shared<Recipes>(*this, false));
	scenesContainer.emplace_back(make_shared<Recipe>(*this, false));
	scenesContainer.emplace_back(make_shared<AddRecipeStep>(*this, false));
	scenesContainer.emplace_back(make_shared<AddIngredient>(*this, false));
	for (const auto &sc : scenesContainer) {
		scenes[sc->getName()] = {sc};
	}
}

void Scenes::showScene(const string &sceneName) { scenes[sceneName]->show = true; }

void Scenes::hideScene(const string &sceneName) { scenes[sceneName]->show = false; }

shared_ptr<Scene> Scenes::getScene(const string &sceneName) { return scenes[sceneName]; }

void Scenes::updateComputeUniformBuffers() {
	for (const auto &sc : scenesContainer) {
		if (sc->show) {
			sc->updateRayTraceUniformBuffers();
			sc->updateComputeUniformBuffers();
		}
	}
}

void Scenes::computePass() {
	Model *globalClosest = nullptr;
	float bestLen = std::numeric_limits<float>::infinity();

	std::vector<Scene *> visibleScenes;
	visibleScenes.reserve(scenes.size());

	for (const auto &sc : scenesContainer) {
		if (!sc->show)
			continue;
		visibleScenes.push_back(sc.get());

		Scene::ClosestHit hit = sc->rayTraces();
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
	for (const auto &sc : scenesContainer) {
		if (sc->show) {
			sc->updateUniformBuffers();
		}
	}
}

void Scenes::renderPass() {
	for (const auto &sc : scenesContainer) {
		if (sc->show) {
			sc->renderPass();
		}
	}
}

void Scenes::renderPass1() {
	blur->copy(Engine::currentCommandBuffer());
	for (const auto &sc : scenesContainer) {
		if (sc->show) {
			sc->renderPass1();
		}
	}
}

void Scenes::swapChainUpdate() {
	vp.width = static_cast<float>(Engine::swapChainExtent.width);
	vp.height = static_cast<float>(Engine::swapChainExtent.height);
	sc.offset = {(int32_t)vp.x, (int32_t)vp.y};
	sc.extent = {(uint32_t)vp.width, (uint32_t)vp.height};
	blur->updateCopyViewport(vp, sc);
	for (const auto &sc : scenesContainer) {
		sc->updateScreenParams();
		sc->swapChainUpdate();
	}
}
