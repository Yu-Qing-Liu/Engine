#include "polygon.hpp"
#include "engine.hpp"
#include <cstring>
#include <vulkan/vulkan_core.h>

using namespace glm;

Polygon::Polygon(const std::vector<Vertex> &vertices, const std::vector<uint16_t> &indices) : Model(Engine::shaderRootPath + "/polygon", vertices, indices) {
	createDescriptorSetLayout();
	createUniformBuffers();
	createDescriptorPool();
	createDescriptorSets();
	createVertexBuffer();
	createIndexBuffer();
	createBindingDescriptions();
	createGraphicsPipeline();
}
