#include "scene.hpp"

void Scene::renderPass() {}

void Scene::drawFrame() {
    renderPass();
	for (const auto &cb : frameCallbacks) {
        cb();
	}
}
