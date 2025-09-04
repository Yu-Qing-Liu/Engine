#include "polygon.hpp"
#include "engine.hpp"
#include <cstring>
#include <vulkan/vulkan_core.h>

Polygon::Polygon(const vector<Vertex> &vertices, const vector<uint16_t> &indices) : Model(Engine::shaderRootPath + "/polygon", vertices, indices) {
	createDescriptorSetLayout();
	createUniformBuffers();
	createDescriptorPool();
	createDescriptorSets();
	createVertexBuffer();
	createIndexBuffer();
	createBindingDescriptions();
	createGraphicsPipeline();

    createComputeDescriptorSetLayout();
    createShaderStorageBuffers();
    createComputeDescriptorSets();
    createComputePipeline();
}
