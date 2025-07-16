#include "default.hpp"

Default::Default(VkPhysicalDevice &physicalDevice, VkDevice &device, VkRenderPass &renderPass, VkExtent2D &swapChainExtent) : Scene(physicalDevice, device, renderPass, swapChainExtent) {}

void Default::render(VkCommandBuffer &commandBuffer) {
    models->models[Models::TRIANGLE]->draw(commandBuffer);
}
