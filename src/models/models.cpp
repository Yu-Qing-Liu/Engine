#include "models.hpp"
#include "triangle.hpp"

Models::Models(VkDevice &device, VkRenderPass &renderPass, VkExtent2D &swapChainExtent) {
    models.emplace(TRIANGLE, std::make_unique<Triangle>(device, modelsPath, renderPass, swapChainExtent));
}
