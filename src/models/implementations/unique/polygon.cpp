#include "polygon.hpp"
#include "engine.hpp"
#include <cstring>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

Polygon::Polygon(Scene *scene, const UBO &ubo, ScreenParams &screenParams, const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices) : Model(scene, ubo, screenParams, Assets::shaderRootPath + "/unique/polygon") {
	expandForOutlines<Vertex>(vertices, indices, this->vertices, this->indices);

	createDescriptorSetLayout();
	createUniformBuffers();
	createParamsBuffer();
	createDescriptorPool();
	createDescriptorSets();

	createVertexBuffer<Vertex>(this->vertices);
	createIndexBuffer();

	createBindingDescriptions();
	createGraphicsPipeline();

	createComputeDescriptorSetLayout();
	createShaderStorageBuffers();
	createComputeDescriptorSets();
	createComputePipeline();
}

Polygon::~Polygon() {
	for (size_t i = 0; i < paramsBuffers.size(); ++i) {
		if (paramsBuffersMemory[i]) {
			if (paramsBuffersMapped[i]) {
				vkUnmapMemory(Engine::device, paramsBuffersMemory[i]);
			}
			vkFreeMemory(Engine::device, paramsBuffersMemory[i], nullptr);
		}
		if (paramsBuffers[i]) {
			vkDestroyBuffer(Engine::device, paramsBuffers[i], nullptr);
		}
	}
}

void Polygon::buildBVH() {
    Model::buildBVH<Vertex>(vertices);
}

void Polygon::createBindingDescriptions() {
	bindingDescription = Vertex::getBindingDescription();
	auto attrs = Vertex::getAttributeDescriptions();
	attributeDescriptions = std::vector<VkVertexInputAttributeDescription>(attrs.begin(), attrs.end());
}

void Polygon::createDescriptorSetLayout() {
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	paramsBinding.binding = 1;
	paramsBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	paramsBinding.descriptorCount = 1;
	paramsBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, paramsBinding};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = (uint32_t)bindings.size();
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(Engine::device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor set layout!");
	}
}

void Polygon::createDescriptorPool() {
	poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSize.descriptorCount = static_cast<uint32_t>(2 * Engine::MAX_FRAMES_IN_FLIGHT);

	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;
	poolInfo.maxSets = static_cast<uint32_t>(Engine::MAX_FRAMES_IN_FLIGHT);

	if (vkCreateDescriptorPool(Engine::device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor pool");
	}
}

void Polygon::createParamsBuffer() {
	VkDeviceSize sz = sizeof(Params);
	paramsBuffers.resize(Engine::MAX_FRAMES_IN_FLIGHT);
	paramsBuffersMemory.resize(Engine::MAX_FRAMES_IN_FLIGHT);
	paramsBuffersMapped.resize(Engine::MAX_FRAMES_IN_FLIGHT);

	for (size_t i = 0; i < Engine::MAX_FRAMES_IN_FLIGHT; ++i) {
		Engine::createBuffer(sz, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, paramsBuffers[i], paramsBuffersMemory[i]);
		vkMapMemory(Engine::device, paramsBuffersMemory[i], 0, sz, 0, &paramsBuffersMapped[i]);
	}
}

void Polygon::createDescriptorSets() {
	std::vector<VkDescriptorSetLayout> layouts(Engine::MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(Engine::MAX_FRAMES_IN_FLIGHT);
	allocInfo.pSetLayouts = layouts.data();

	descriptorSets.resize(Engine::MAX_FRAMES_IN_FLIGHT);

	if (vkAllocateDescriptorSets(Engine::device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate descriptor set");
	}

	for (size_t i = 0; i < Engine::MAX_FRAMES_IN_FLIGHT; i++) {
		VkDescriptorBufferInfo uboInfo{uniformBuffers[i], 0, sizeof(UBO)};
		VkDescriptorBufferInfo paramsInfo{paramsBuffers[i], 0, sizeof(Params)};

		std::array<VkWriteDescriptorSet, 2> writes{};

		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet = descriptorSets[i];
		writes[0].dstBinding = 0;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writes[0].descriptorCount = 1;
		writes[0].pBufferInfo = &uboInfo;

		writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[1].dstSet = descriptorSets[i];
		writes[1].dstBinding = 1;
		writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writes[1].descriptorCount = 1;
		writes[1].pBufferInfo = &paramsInfo;

		vkUpdateDescriptorSets(Engine::device, (uint32_t)writes.size(), writes.data(), 0, nullptr);
	}
}

void Polygon::render() {
	// Update per-frame data
	std::memcpy(paramsBuffersMapped[Engine::currentFrame], &params, sizeof(params));
    Model::render();
}
