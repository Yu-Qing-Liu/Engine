#include "triangle.hpp"
#include "engine.hpp"
#include <cstring>
#include <vulkan/vulkan_core.h>

using namespace glm;

Triangle::Triangle() :
Model(
    Engine::shaderRootPath + "/triangle", 
    std::vector<Vertex> {
        {{0.0f, 0.5f}, {1.0f, 0.0f, 0.0f}},
        {{-0.433f, -0.25f}, {0.0f, 1.0f, 0.0f}},
        {{0.433f, -0.25f}, {0.0f, 0.0f, 1.0f}},
    },
    {
        0, 1, 2
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
