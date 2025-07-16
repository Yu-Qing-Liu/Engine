#include "scene.hpp"
#include "models.hpp"
#include <memory>

Scene::Scene(VkPhysicalDevice &physicalDevice, VkDevice &device, VkRenderPass &renderPass, VkExtent2D &swapChainExtent) {
    models = std::make_unique<Models>(physicalDevice, device, renderPass, swapChainExtent);
}

Scene::~Scene() {}

void Scene::render(VkCommandBuffer &commandBuffer) {}
