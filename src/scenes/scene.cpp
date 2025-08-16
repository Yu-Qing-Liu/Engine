#include "scene.hpp"
#include "models.hpp"
#include <memory>

Scene::Scene() {
    models = std::make_unique<Models>();
}

Scene::~Scene() {}

void Scene::renderPass() {}
void Scene::drawFrame() {}
