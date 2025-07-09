#include "scenes.hpp"
#include "default.hpp"

Scenes::Scenes(VkDevice &device, VkRenderPass &renderPass, VkExtent2D &swapChainExtent) {
    scenes.emplace(DEFAULT, std::make_unique<Default>(device, renderPass, swapChainExtent));
}

void Scenes::render(VkCommandBuffer &commandBuffer) {
    /*
     * Main render loop
     * */
    scenes[DEFAULT]->render(commandBuffer);
}
