#include "scenes.hpp"
#include "default.hpp"

Scenes::Scenes(VkPhysicalDevice &physicalDevice, VkDevice &device, VkRenderPass &renderPass, VkExtent2D &swapChainExtent) {
    scenes.emplace(DEFAULT, std::make_unique<Default>(physicalDevice, device, renderPass, swapChainExtent));
}

void Scenes::render(VkCommandBuffer &commandBuffer) {
    /*
     * Main render loop
     * */
    scenes[DEFAULT]->render(commandBuffer);
}
