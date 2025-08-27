#include "scene.hpp"

void Scene::renderPass() {}

void Scene::drawFrame() {
	for (const auto &cb : frameCallbacks) {
        cb();
	}
}
