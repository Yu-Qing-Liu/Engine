#include "default.hpp"

Default::Default(VkDevice &device, VkRenderPass &renderPass, VkExtent2D &swapChainExtent) : Scene(device, renderPass, swapChainExtent) {}

void Default::render(VkCommandBuffer &commandBuffer) {
    models->models[Models::TRIANGLE]->draw(commandBuffer);
}
