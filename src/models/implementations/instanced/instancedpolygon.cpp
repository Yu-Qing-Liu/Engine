#include "instancedpolygon.hpp"
#include "polygon.hpp"
#include "assets.hpp"
#include <cstring>

InstancedPolygon::InstancedPolygon(const UBO &ubo, ScreenParams &screenParams, const vector<Vertex> &vertices, const vector<uint16_t> &indices, shared_ptr<unordered_map<int, InstancedPolygonData>> instances, uint32_t maxInstances) : InstancedModel(ubo, screenParams, Assets::shaderRootPath + "/instanced/instancedpolygon", instances, maxInstances) {
    Polygon::expandForOutlines<Vertex>(vertices, indices, this->vertices, this->indices);

	// Geometry
	createVertexBuffer<Vertex>(this->vertices);
	createIndexBuffer();

	// Descriptor set (UBO only, reuse base)
	createDescriptorSetLayout();
	createUniformBuffers();
	createDescriptorPool();
	createDescriptorSets();

	// Vertex input (add instance binding + attrs)
	createBindingDescriptions();

	// Graphics pipeline with 2 bindings
	createGraphicsPipeline();
}

void InstancedPolygon::createBindingDescriptions() {
	vertexBD = Vertex::getBindingDescription();

	instanceBD = {};
	instanceBD.binding = 1;
	instanceBD.stride = sizeof(InstancedPolygonData);
	instanceBD.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

	auto baseAttrs = Vertex::getAttributeDescriptions();
	attributeDescriptions.assign(baseAttrs.begin(), baseAttrs.end());
	// base occupies locations 0..3

	// Instance model rows at 4..7
	VkVertexInputAttributeDescription a{};
	a.binding = 1;
	a.format = VK_FORMAT_R32G32B32A32_SFLOAT;

	a.location = 4;
	a.offset = offsetof(InstancedPolygonData, model) + sizeof(vec4) * 0;
	attributeDescriptions.push_back(a);
	a.location = 5;
	a.offset = offsetof(InstancedPolygonData, model) + sizeof(vec4) * 1;
	attributeDescriptions.push_back(a);
	a.location = 6;
	a.offset = offsetof(InstancedPolygonData, model) + sizeof(vec4) * 2;
	attributeDescriptions.push_back(a);
	a.location = 7;
	a.offset = offsetof(InstancedPolygonData, model) + sizeof(vec4) * 3;
	attributeDescriptions.push_back(a);

	// Instance colors / outline at 8..11
	VkVertexInputAttributeDescription ai{};
	ai.binding = 1;

	ai.location = 8;
	ai.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	ai.offset = offsetof(InstancedPolygonData, color);
	attributeDescriptions.push_back(ai);

	ai.location = 9;
	ai.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	ai.offset = offsetof(InstancedPolygonData, outlineColor);
	attributeDescriptions.push_back(ai);

	ai.location = 10;
	ai.format = VK_FORMAT_R32_SFLOAT;
	ai.offset = offsetof(InstancedPolygonData, outlineWidth);
	attributeDescriptions.push_back(ai);

	bindings = {vertexBD, instanceBD};
}

void InstancedPolygon::setupGraphicsPipeline() {
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindings.size());
    vertexInputInfo.pVertexBindingDescriptions = bindings.data();
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
}
