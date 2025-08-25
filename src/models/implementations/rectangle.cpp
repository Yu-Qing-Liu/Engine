#include "rectangle.hpp"
#include "engine.hpp"
#include <cstring>
#include <vulkan/vulkan_core.h>

using namespace glm;

Rectangle::Rectangle() :
Model(
    Engine::shaderRootPath + "/rectangle", 
    std::vector<Vertex> {
        {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}}
    },
    {
        0, 1, 2, 2, 3, 0
    }
) {
	createDescriptorSetLayout();
	createUniformBuffers();
	createDescriptorPool();
	createDescriptorSets();
	createVertexBuffer();
	createIndexBuffer();
    createBindingDescriptions();
	createGraphicsPipeline();
}
