#include "scene.hpp"
#include "scenes.hpp"

Scene::Scene(Scenes &scenes) : scenes(scenes) {
	screenParams.viewport.x = 0.0f;
	screenParams.viewport.y = 0.0f;
	screenParams.viewport.width = (float)Engine::swapChainExtent.width;
	screenParams.viewport.height = (float)Engine::swapChainExtent.height;
	screenParams.viewport.minDepth = 0.0f;
	screenParams.viewport.maxDepth = 1.0f;
	screenParams.scissor.offset = {0, 0};
	screenParams.scissor.extent = Engine::swapChainExtent;
}

void Scene::updateRayTraceUniformBuffers() {
	for (const auto &m : models) {
		if (m->rayTracingEnabled) {
			m->updateRayTraceUniformBuffer();
		}
	}
}

void Scene::rayTraces() {
	// 1) Dispatch all
	for (auto *m : models) {
		if (m && m->rayTracingEnabled)
			m->rayTrace();
	}

	// 2) Reset flags & select closest by rayLen
	Model *closest = nullptr;
	float bestLen = std::numeric_limits<float>::infinity();

	for (auto *m : models) {
		if (!m) {
			continue;
        }
		m->mouseIsOver = false;
		if (!m->rayTracingEnabled) {
			continue;
        }
		if (!m->rayLength.has_value()) {
			continue;
        }
		float d = *m->rayLength;
		if (d < bestLen) {
			bestLen = d;
			closest = m;
		}
	}

	// 3) Mark the single winner and (optionally) fire hover
	if (closest) {
		closest->mouseIsOver = true;
		if (closest->onHover) {
			closest->onHover();
		}
	}
}

void Scene::updateScreenParams() {}

void Scene::updateComputeUniformBuffers() {}
void Scene::computePass() {}

void Scene::updateUniformBuffers() {}
void Scene::renderPass() {}
void Scene::swapChainUpdate() {}
