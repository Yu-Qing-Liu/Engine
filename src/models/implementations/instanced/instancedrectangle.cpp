#include "instancedrectangle.hpp"
#include "assets.hpp"
#include <cstring>

InstancedRectangle::InstancedRectangle(Scene* scene, const UBO &ubo, ScreenParams &screenParams, shared_ptr<unordered_map<int, InstancedRectangleData>> instances, uint32_t maxInstances) : InstancedModel(scene, ubo, screenParams, Assets::shaderRootPath + "/instanced/instancedrectangle", instances, maxInstances) {
	indices = {0, 1, 2, 2, 3, 0};

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

	createComputeDescriptorSetLayout();
	createShaderStorageBuffers();
	createComputeDescriptorSets();
	createComputePipeline();
}

void InstancedRectangle::buildBVH() {
    Model::buildBVH<Vertex>(vertices);
}

void InstancedRectangle::createBindingDescriptions() {
	vertexBD = Vertex::getBindingDescription();
	instanceBD = {};
	instanceBD.binding = 1;
	instanceBD.stride = sizeof(InstancedRectangleData);
	instanceBD.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

	auto baseAttrs = Vertex::getAttributeDescriptions();
	attributeDescriptions.assign(baseAttrs.begin(), baseAttrs.end());

	VkVertexInputAttributeDescription a{};
	a.binding = 1;
	a.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	a.location = 1;
	a.offset = offsetof(InstancedRectangleData, model) + sizeof(vec4) * 0;
	attributeDescriptions.push_back(a);
	a.location = 2;
	a.offset = offsetof(InstancedRectangleData, model) + sizeof(vec4) * 1;
	attributeDescriptions.push_back(a);
	a.location = 3;
	a.offset = offsetof(InstancedRectangleData, model) + sizeof(vec4) * 2;
	attributeDescriptions.push_back(a);
	a.location = 4;
	a.offset = offsetof(InstancedRectangleData, model) + sizeof(vec4) * 3;
	attributeDescriptions.push_back(a);

	VkVertexInputAttributeDescription ad{};
	ad.binding = 1;
	ad.location = 5;
	ad.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	ad.offset = offsetof(InstancedRectangleData, color);
	attributeDescriptions.push_back(ad);
	ad.binding = 1;
	ad.location = 6;
	ad.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	ad.offset = offsetof(InstancedRectangleData, outlineColor);
	attributeDescriptions.push_back(ad);
	ad.binding = 1;
	ad.location = 7;
	ad.format = VK_FORMAT_R32_SFLOAT;
	ad.offset = offsetof(InstancedRectangleData, outlineWidth);
	attributeDescriptions.push_back(ad);
	ad.binding = 1;
	ad.location = 8;
	ad.format = VK_FORMAT_R32_SFLOAT;
	ad.offset = offsetof(InstancedRectangleData, borderRadius);
	attributeDescriptions.push_back(ad);

	bindings = {vertexBD, instanceBD};
}

void InstancedRectangle::setupGraphicsPipeline() {
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindings.size());
	vertexInputInfo.pVertexBindingDescriptions = bindings.data();
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	rasterizer.cullMode = VK_CULL_MODE_NONE;

	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_FALSE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
}
