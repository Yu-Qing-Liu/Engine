#include "scenes/scene.hpp"
#include "models.hpp"
#include <memory>

Scene::Scene(VkDevice &device, VkRenderPass &renderPass, VkExtent2D &swapChainExtent) {
    models = std::make_unique<Models>(device, renderPass, swapChainExtent);
}

void Scene::render(VkCommandBuffer &commandBuffer) {}
