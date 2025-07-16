#include "models.hpp"
#include "triangle.hpp"

Models::Models(VkPhysicalDevice &physicalDevice, VkDevice &device, VkRenderPass &renderPass, VkExtent2D &swapChainExtent) {
    models.emplace(TRIANGLE, std::make_unique<Triangle>(physicalDevice, device, modelsPath, renderPass, swapChainExtent));
}
