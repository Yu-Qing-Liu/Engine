#include "instancedrectangle.hpp"
#include "assets.hpp"
#include "engine.hpp"
#include <cstring>

InstancedRectangle::InstancedRectangle(const UBO &ubo, ScreenParams &screenParams, shared_ptr<unordered_map<int, InstanceData>> instances, uint32_t maxInstances) : instances(instances), Model(nullptr, ubo, screenParams, Assets::shaderRootPath + "/instanced/instancedrectangle") {
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

	// Instance buffers
	createInstanceBuffers();
}

InstancedRectangle::~InstancedRectangle() {
	for (size_t i = 0; i < instanceBuffers.size(); ++i) {
		if (instanceMapped[i])
			vkUnmapMemory(Engine::device, instanceMemories[i]);
		if (instanceBuffers[i])
			vkDestroyBuffer(Engine::device, instanceBuffers[i], nullptr);
		if (instanceMemories[i])
			vkFreeMemory(Engine::device, instanceMemories[i], nullptr);
	}
}

void InstancedRectangle::createInstanceBuffers(size_t maxInstances) {
	const VkDeviceSize sz = static_cast<VkDeviceSize>(maxInstances * sizeof(InstanceData));
	instanceBuffers.resize(Engine::MAX_FRAMES_IN_FLIGHT);
	instanceMemories.resize(Engine::MAX_FRAMES_IN_FLIGHT);
	instanceMapped.resize(Engine::MAX_FRAMES_IN_FLIGHT);

	for (size_t i = 0; i < Engine::MAX_FRAMES_IN_FLIGHT; ++i) {
		Engine::createBuffer(sz, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, instanceBuffers[i], instanceMemories[i]);
		vkMapMemory(Engine::device, instanceMemories[i], 0, sz, 0, &instanceMapped[i]);
	}

    instances->max_load_factor(0.7f);
    const size_t buckets = static_cast<size_t>(std::ceil(maxInstances / instances->max_load_factor()));
    instances->rehash(buckets);

    cpuPtrs.resize(maxInstances, nullptr);
    slotToKey.resize(maxInstances, -1);
    keyToSlot.clear();
    instanceCount = 0;
    frameDirty.fill(true);
}

void InstancedRectangle::createBindingDescriptions() {
	vertexBD = Vertex::getBindingDescription();
	instanceBD = {};
	instanceBD.binding = 1;
	instanceBD.stride = sizeof(InstanceData);
	instanceBD.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

	auto baseAttrs = Vertex::getAttributeDescriptions();
	attributeDescriptions.assign(baseAttrs.begin(), baseAttrs.end());

	VkVertexInputAttributeDescription a{};
	a.binding = 1;
	a.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	a.location = 1;
	a.offset = offsetof(InstanceData, model) + sizeof(vec4) * 0;
	attributeDescriptions.push_back(a);
	a.location = 2;
	a.offset = offsetof(InstanceData, model) + sizeof(vec4) * 1;
	attributeDescriptions.push_back(a);
	a.location = 3;
	a.offset = offsetof(InstanceData, model) + sizeof(vec4) * 2;
	attributeDescriptions.push_back(a);
	a.location = 4;
	a.offset = offsetof(InstanceData, model) + sizeof(vec4) * 3;
	attributeDescriptions.push_back(a);

	VkVertexInputAttributeDescription ad{};
	ad.binding = 1;
	ad.location = 5;
	ad.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	ad.offset = offsetof(InstanceData, color);
	attributeDescriptions.push_back(ad);
	ad.binding = 1;
	ad.location = 6;
	ad.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	ad.offset = offsetof(InstanceData, outlineColor);
	attributeDescriptions.push_back(ad);
	ad.binding = 1;
	ad.location = 7;
	ad.format = VK_FORMAT_R32_SFLOAT;
	ad.offset = offsetof(InstanceData, outlineWidth);
	attributeDescriptions.push_back(ad);
	ad.binding = 1;
	ad.location = 8;
	ad.format = VK_FORMAT_R32_SFLOAT;
	ad.offset = offsetof(InstanceData, borderRadius);
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

void InstancedRectangle::updateInstance(int id, const InstanceData& data) {
    // existing -> in-place update (pointer stable)
    if (auto it = keyToSlot.find(id); it != keyToSlot.end()) {
        *cpuPtrs[it->second] = data;  // write through pointer
        frameDirty.fill(true);
        return;
    }

    // new -> emplace into map (no rehash will occur if you pre-sized correctly)
    auto [itMap, inserted] = instances->emplace(id, data);
    if (!inserted) {
        // race condition with another thread updating the map? just write
        itMap->second = data;
        // also need to put it into a slot if it wasn't already there
    }

    // assign dense slot
    const uint32_t slot = instanceCount++;
    cpuPtrs[slot] = &itMap->second;  // pointer into unordered_map node
    slotToKey[slot] = id;
    keyToSlot[id] = slot;

    frameDirty.fill(true);
}

void InstancedRectangle::deleteInstance(int id) {
    auto it = keyToSlot.find(id);
    if (it == keyToSlot.end()) return;
    const uint32_t slot = it->second;
    const uint32_t last = instanceCount - 1;

    // If not the last slot, move last -> slot
    if (slot != last) {
        cpuPtrs[slot]     = cpuPtrs[last];
        slotToKey[slot]   = slotToKey[last];
        keyToSlot[slotToKey[slot]] = slot;  // fix moved key's slot
    }

    // shrink live range
    cpuPtrs[last]   = nullptr;
    slotToKey[last] = -1;
    instanceCount--;

    // erase from map (invalidates only that node)
    instances->erase(id);
    keyToSlot.erase(it);

    frameDirty.fill(true);
}

void InstancedRectangle::uploadIfDirty() {
    const uint32_t cf = Engine::currentFrame;
    if (!frameDirty[cf] || instanceCount == 0) return;

    auto* dst = static_cast<InstanceData*>(instanceMapped[cf]);
    // contiguous upload of all live slots
    for (uint32_t i = 0; i < instanceCount; ++i) {
        // copy *struct*, not the pointer
        dst[i] = *cpuPtrs[i];
    }
    frameDirty[cf] = false;
}

void InstancedRectangle::render() {
    if (!instances) {
        return;
    }
	// Update UBO once per-frame (view/proj still needed; model can be identity/ignored)
	copyUBO();
    uploadIfDirty();

	// Bind pipeline
	auto cmd = Engine::currentCommandBuffer();
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

	// Dynamic viewport/scissor
	vkCmdSetViewport(cmd, 0, 1, &screenParams.viewport);
	vkCmdSetScissor(cmd, 0, 1, &screenParams.scissor);

	// Bind 2 vertex buffers
	VkBuffer vbs[2] = {vertexBuffer, instanceBuffers[Engine::currentFrame]};
	VkDeviceSize ofs[2] = {0, 0};
	vkCmdBindVertexBuffers(cmd, 0, 2, vbs, ofs);

	// Index buffer (note: your base uses uint16 indices)
	vkCmdBindIndexBuffer(cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT16);

	// Descriptor set (UBO set=0)
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[Engine::currentFrame], 0, nullptr);

	// Draw all instances in one call
	vkCmdDrawIndexed(cmd, static_cast<uint32_t>(indices.size()), instanceCount, 0, 0, 0);
}
