#include "scene.hpp"
#include "scenes.hpp"

Scene::Scene(Scenes &scenes) : scenes(scenes) {}

void Scene::updateComputeUniformBuffers() {}
void Scene::computePass() {}

void Scene::updateUniformBuffers() {}
void Scene::renderPass() {}
void Scene::swapChainUpdate() {}
