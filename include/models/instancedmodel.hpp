#pragma once

#include "model.hpp"
#include <memory>
#include <stdexcept>
#include <unordered_map>

using std::shared_ptr;
using std::unordered_map;

template <typename T> class InstancedModel : public Model {
  public:
	InstancedModel(InstancedModel &&) = delete;
	InstancedModel(const InstancedModel &) = delete;
	InstancedModel &operator=(InstancedModel &&) = delete;
	InstancedModel &operator=(const InstancedModel &) = delete;

	InstancedModel(Scene *scene, const UBO &ubo, ScreenParams &screenParams, const string &shaderPath, shared_ptr<unordered_map<int, T>> instances, uint32_t maxInstances = 65536) : instances(instances), maxInstances(maxInstances), Model(scene, ubo, screenParams, shaderPath) { createInstanceBuffers(); }
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
		if (instBuf)
			vkDestroyBuffer(Engine::device, instBuf, nullptr);
		if (idBuf)
			vkDestroyBuffer(Engine::device, idBuf, nullptr);
		if (instMem) {
			vkUnmapMemory(Engine::device, instMem);
			vkFreeMemory(Engine::device, instMem, nullptr);
		}
		if (idMem) {
			vkUnmapMemory(Engine::device, idMem);
			vkFreeMemory(Engine::device, idMem, nullptr);
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

	const T &getInstance(int id) const {
		auto it = instances->find(id);
		if (it == instances->end()) {
			throw std::out_of_range("Instance not found");
		}
		return it->second;
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
		vkCmdBindIndexBuffer(cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

		// Descriptor set (UBO set=0)
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[Engine::currentFrame], 0, nullptr);

		// Allow derived to bind more
		bindExtraDescriptorSets(cmd);

		// Draw all instances in one call
		vkCmdDrawIndexed(cmd, static_cast<uint32_t>(indices.size()), instanceCount, 0, 0, 0);
	}

  protected:
	void *instMapped = nullptr;
	void *idMapped = nullptr;

	virtual void bindExtraDescriptorSets(VkCommandBuffer cmd) {}
	virtual mat4 toModel(const T &M) { return M.model; };

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

		instCPU.resize(maxInstances);
		idsCPU.resize(maxInstances);
	}

	void createShaderStorageBuffers() override {
		Model::createShaderStorageBuffers();

		const VkDeviceSize xSzFull = sizeof(InstanceXformGPU) * maxInstances;
		const VkDeviceSize iSzFull = sizeof(int) * maxInstances;

		auto makeHostVisible = [&](VkDeviceSize sz, VkBufferUsageFlags usage, VkBuffer &buf, VkDeviceMemory &mem, void **mapped) {
			Engine::createBuffer(sz, usage, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, buf, mem);
			if (vkMapMemory(Engine::device, mem, 0, VK_WHOLE_SIZE, 0, mapped) != VK_SUCCESS) {
				throw std::runtime_error("Failed to map instanced raytracing buffers");
			}
		};

		makeHostVisible(xSzFull, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, instBuf, instMem, &instMapped);
		makeHostVisible(iSzFull, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, idBuf, idMem, &idMapped);
	}

	void createComputeDescriptorSetLayout() override {
		// Base had 5 bindings (0..4). Add 5: instances, 6: ids
		std::array<VkDescriptorSetLayoutBinding, 7> b{};

		b[0] = {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}; // nodes
		b[1] = {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}; // tris
		b[2] = {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}; // pos
		b[3] = {3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}; // params
		b[4] = {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}; // out
		b[5] = {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}; // inst xforms
		b[6] = {6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}; // slot->key

		VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
		ci.bindingCount = (uint32_t)b.size();
		ci.pBindings = b.data();
		if (vkCreateDescriptorSetLayout(Engine::device, &ci, nullptr, &computeDescriptorSetLayout) != VK_SUCCESS)
			throw std::runtime_error("compute DSL failed");

		// small pool for 1 set (6 storage + 1 uniform)
		std::array<VkDescriptorPoolSize, 2> ps{VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 6}, VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}};
		VkDescriptorPoolCreateInfo pci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
		pci.maxSets = 1;
		pci.poolSizeCount = (uint32_t)ps.size();
		pci.pPoolSizes = ps.data();
		if (vkCreateDescriptorPool(Engine::device, &pci, nullptr, &computePool) != VK_SUCCESS)
			throw std::runtime_error("compute pool failed");
	}

	void createComputeDescriptorSets() override {
		// allocate one set with our 7-binding layout
		VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
		ai.descriptorPool = computePool;
		ai.descriptorSetCount = 1;
		ai.pSetLayouts = &computeDescriptorSetLayout;

		if (vkAllocateDescriptorSets(Engine::device, &ai, &computeDescriptorSet) != VK_SUCCESS)
			throw std::runtime_error("instanced compute DS alloc failed");

		// buffer infos
		VkDescriptorBufferInfo nb{nodesBuf, 0, VK_WHOLE_SIZE};
		VkDescriptorBufferInfo tb{trisBuf, 0, VK_WHOLE_SIZE};
		VkDescriptorBufferInfo pb{posBuf, 0, VK_WHOLE_SIZE};
		VkDescriptorBufferInfo ub{pickUBO, 0, sizeof(PickingUBO)};
		VkDescriptorBufferInfo rb{hitBuf, 0, sizeof(HitOutCPU)};
		VkDescriptorBufferInfo xb{instBuf, 0, VK_WHOLE_SIZE}; // binding 5
		VkDescriptorBufferInfo ib{idBuf, 0, VK_WHOLE_SIZE};	  // binding 6

		std::array<VkWriteDescriptorSet, 7> w{};
		auto W = [&](uint32_t i, uint32_t binding, VkDescriptorType type, const VkDescriptorBufferInfo *bi) {
			w[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			w[i].dstSet = computeDescriptorSet;
			w[i].dstBinding = binding;
			w[i].descriptorType = type;
			w[i].descriptorCount = 1;
			w[i].pBufferInfo = bi;
		};

		W(0, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &nb); // nodes
		W(1, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &tb); // tris
		W(2, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &pb); // pos
		W(3, 3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &ub); // params
		W(4, 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &rb); // out
		W(5, 5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &xb); // inst xforms
		W(6, 6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &ib); // ids

		vkUpdateDescriptorSets(Engine::device, (uint32_t)w.size(), w.data(), 0, nullptr);
	}

	void createComputePipeline() override {
		rayTracingShaderPath = Assets::shaderRootPath + "/instancedraytracing";
		rayTracingProgram = Assets::compileShaderProgram(rayTracingShaderPath);
		if (rayTracingProgram.computeShader == VK_NULL_HANDLE)
			throw std::runtime_error("instanced compute shader missing!");

		VkPipelineLayoutCreateInfo pli{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
		pli.setLayoutCount = 1;
		pli.pSetLayouts = &computeDescriptorSetLayout;
		if (vkCreatePipelineLayout(Engine::device, &pli, nullptr, &computePipelineLayout) != VK_SUCCESS)
			throw std::runtime_error("compute pipeline layout failed");

		VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
		ci.stage = Engine::createShaderStageInfo(rayTracingProgram.computeShader, VK_SHADER_STAGE_COMPUTE_BIT);
		ci.layout = computePipelineLayout;
		if (vkCreateComputePipelines(Engine::device, VK_NULL_HANDLE, 1, &ci, nullptr, &computePipeline) != VK_SUCCESS)
			throw std::runtime_error("instanced compute pipeline failed");
	}

	void updateComputeUniformBuffer() override {
		if (!rayTracingEnabled)
			return;

		float mousePx = 0, mousePy = 0;
		Platform::GetPointerInFramebufferPixels(mousePx, mousePy);
		setRayTraceFromViewportPx(mousePx, mousePy, screenParams.viewport);

		glm::mat4 invVP = inverse(ubo.proj * ubo.view);
		glm::mat4 invV = inverse(ubo.view);
		glm::vec3 cam = glm::vec3(invV[3]);

		PickingUBO p{};
		p.invViewProj = invVP;
		p.invModel = glm::mat4(1.0f); // not used by instanced path
		p.mouseNdc = rayTraceParams.mouseNdc;
		p.camPos = (rayTraceParams.camPos == glm::vec3(0)) ? cam : rayTraceParams.camPos;
		p._pad = (int)instanceCount; // <--- instance count

		std::memcpy(pickUBOMapped, &p, sizeof(PickingUBO));

		if (hitMapped && hitMapped->hit) {
			hitPos = hitMapped->hitPos;
			rayLength = hitMapped->rayLen;
			hitMapped->hit = 0;
		} else {
			hitPos.reset();
			rayLength.reset();
			setMouseIsOver(false);
		}
	}

  private:
	// --- Compute --- //
	VkBuffer instBuf = VK_NULL_HANDLE;
	VkDeviceMemory instMem = VK_NULL_HANDLE; // binding=5
	VkBuffer idBuf = VK_NULL_HANDLE;
	VkDeviceMemory idMem = VK_NULL_HANDLE; // binding=6

	struct InstanceXformGPU {
		glm::mat4 model;	// object -> world
		glm::mat4 invModel; // world  -> object
	};

	std::vector<InstanceXformGPU> instCPU; // sized to maxInstances
	std::vector<int> idsCPU;			   // slot -> external id

	// --- Graphics --- //
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

		// Fill transform + ids for compute picking
		for (uint32_t i = 0; i < instanceCount; ++i) {
			const T &src = *cpuPtrs[i];
			glm::mat4 M = toModel(src);
			instCPU[i].model = M;
			instCPU[i].invModel = glm::inverse(M);
			idsCPU[i] = slotToKey[i]; // external id you want to read back
		}

		// memcpy into persistently-mapped ranges
		const size_t xSz = sizeof(InstanceXformGPU) * instanceCount;
		const size_t iSz = sizeof(int) * instanceCount;

		std::memcpy(instMapped, instCPU.data(), xSz);
		std::memcpy(idMapped, idsCPU.data(), iSz);

		frameDirty[cf] = false;
	}
};
