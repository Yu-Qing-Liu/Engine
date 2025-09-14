#pragma once

#include "model.hpp"
#include <memory>
#include <unordered_map>

using std::shared_ptr;
using std::unordered_map;

template <typename T> class InstancedModel : public Model {
  public:
	InstancedModel(InstancedModel &&) = delete;
	InstancedModel(const InstancedModel &) = delete;
	InstancedModel &operator=(InstancedModel &&) = delete;
	InstancedModel &operator=(const InstancedModel &) = delete;

	InstancedModel(const UBO &ubo, ScreenParams &screenParams, const string &shaderPath, shared_ptr<unordered_map<int, T>> instances, uint32_t maxInstances = 65536) : instances(instances), maxInstances(maxInstances), Model(nullptr, ubo, screenParams, shaderPath) { createInstanceBuffers(); }
	~InstancedModel() override {
		for (size_t i = 0; i < instanceBuffers.size(); ++i) {
			if (instanceMapped[i]) {
				vkUnmapMemory(Engine::device, instanceMemories[i]);
			}
			if (instanceBuffers[i]) {
				vkDestroyBuffer(Engine::device, instanceBuffers[i], nullptr);
			}
			if (instanceMemories[i]) {
				vkFreeMemory(Engine::device, instanceMemories[i], nullptr);
			}
		}
	}

	void updateInstance(int id, const T &data) {
		// existing -> in-place update (pointer stable)
		if (auto it = keyToSlot.find(id); it != keyToSlot.end()) {
			*cpuPtrs[it->second] = data; // write through pointer
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
		cpuPtrs[slot] = &itMap->second; // pointer into unordered_map node
		slotToKey[slot] = id;
		keyToSlot[id] = slot;

		frameDirty.fill(true);
	}

	void deleteInstance(int id) {
		auto it = keyToSlot.find(id);
		if (it == keyToSlot.end())
			return;
		const uint32_t slot = it->second;
		const uint32_t last = instanceCount - 1;

		// If not the last slot, move last -> slot
		if (slot != last) {
			cpuPtrs[slot] = cpuPtrs[last];
			slotToKey[slot] = slotToKey[last];
			keyToSlot[slotToKey[slot]] = slot; // fix moved key's slot
		}

		// shrink live range
		cpuPtrs[last] = nullptr;
		slotToKey[last] = -1;
		instanceCount--;

		// erase from map (invalidates only that node)
		instances->erase(id);
		keyToSlot.erase(it);

		frameDirty.fill(true);
	}

	void render() override {
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
        
        // Allow derived to bind more
        bindExtraDescriptorSets(cmd);

		// Draw all instances in one call
		vkCmdDrawIndexed(cmd, static_cast<uint32_t>(indices.size()), instanceCount, 0, 0, 0);
	}

  protected:
	virtual void bindExtraDescriptorSets(VkCommandBuffer cmd) {}

	void createInstanceBuffers() {
		const VkDeviceSize sz = static_cast<VkDeviceSize>(maxInstances * sizeof(T));
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

  private:
	shared_ptr<unordered_map<int, T>> instances;
	uint32_t maxInstances;
	bool modified = true;

	// Per-frame instance buffers
	std::vector<VkBuffer> instanceBuffers;
	std::vector<VkDeviceMemory> instanceMemories;
	std::vector<void *> instanceMapped;

	// Dense packing (slot i -> instance i)
	std::vector<T *> cpuPtrs; // pointer into instances->element.second
	std::vector<int> slotToKey;
	std::unordered_map<int, uint32_t> keyToSlot;

	uint32_t instanceCount = 0; // number of *live* slots (0..instanceCount-1 are valid)

	// Dirty tracking (upload only when modified)
	std::array<bool, Engine::MAX_FRAMES_IN_FLIGHT> frameDirty{}; // mark which frames need refresh

	void uploadIfDirty() {
		const uint32_t cf = Engine::currentFrame;
		if (!frameDirty[cf] || instanceCount == 0)
			return;

		auto *dst = static_cast<T *>(instanceMapped[cf]);
		// contiguous upload of all live slots
		for (uint32_t i = 0; i < instanceCount; ++i) {
			// copy *struct*, not the pointer
			dst[i] = *cpuPtrs[i];
		}
		frameDirty[cf] = false;
	}
};
