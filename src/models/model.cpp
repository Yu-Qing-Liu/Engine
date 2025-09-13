#include "model.hpp"
#include "engine.hpp"
#include "scene.hpp"
#include "events.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

Model::Model(Scene *scene, const UBO &ubo, ScreenParams &screenParams, const string &shaderPath) : scene(scene), ubo(ubo), screenParams(screenParams), shaderPath(shaderPath) {
	this->ubo.proj[1][1] *= -1;
    if (scene) {
        scene->models.emplace_back(this);
    }
}
Model::Model(Scene *scene, const UBO &ubo, ScreenParams &screenParams, const string &shaderPath, const vector<uint16_t> &indices) : scene(scene), ubo(ubo), screenParams(screenParams), shaderPath(shaderPath), indices(indices) {
	this->ubo.proj[1][1] *= -1;
    if (scene) {
        scene->models.emplace_back(this);
    }
}

Model::~Model() {
	if (shaderProgram.computeShader != VK_NULL_HANDLE) {
		vkDestroyShaderModule(Engine::device, shaderProgram.computeShader, nullptr);
	}
	if (shaderProgram.fragmentShader != VK_NULL_HANDLE) {
		vkDestroyShaderModule(Engine::device, shaderProgram.fragmentShader, nullptr);
	}
	if (shaderProgram.geometryShader != VK_NULL_HANDLE) {
		vkDestroyShaderModule(Engine::device, shaderProgram.geometryShader, nullptr);
	}
	if (shaderProgram.tessellationControlShader != VK_NULL_HANDLE) {
		vkDestroyShaderModule(Engine::device, shaderProgram.tessellationControlShader, nullptr);
	}
	if (shaderProgram.tessellationEvaluationShader != VK_NULL_HANDLE) {
		vkDestroyShaderModule(Engine::device, shaderProgram.tessellationEvaluationShader, nullptr);
	}
	if (shaderProgram.vertexShader != VK_NULL_HANDLE) {
		vkDestroyShaderModule(Engine::device, shaderProgram.vertexShader, nullptr);
	}

	if (rayTracingProgram.computeShader != VK_NULL_HANDLE) {
		vkDestroyShaderModule(Engine::device, rayTracingProgram.computeShader, nullptr);
		rayTracingProgram.computeShader = VK_NULL_HANDLE;
	}

	for (size_t i = 0; i < Engine::MAX_FRAMES_IN_FLIGHT; i++) {
		if (uniformBuffers[i] != VK_NULL_HANDLE) {
			vkDestroyBuffer(Engine::device, uniformBuffers[i], nullptr);
		}
		if (uniformBuffersMemory[i] != VK_NULL_HANDLE) {
			vkFreeMemory(Engine::device, uniformBuffersMemory[i], nullptr);
		}
	}

	if (vertexBuffer != VK_NULL_HANDLE) {
		vkDestroyBuffer(Engine::device, vertexBuffer, nullptr);
	}
	if (vertexBufferMemory != VK_NULL_HANDLE) {
		vkFreeMemory(Engine::device, vertexBufferMemory, nullptr);
	}
	if (indexBuffer != VK_NULL_HANDLE) {
		vkDestroyBuffer(Engine::device, indexBuffer, nullptr);
	}
	if (indexBufferMemory != VK_NULL_HANDLE) {
		vkFreeMemory(Engine::device, indexBufferMemory, nullptr);
	}
	if (descriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(Engine::device, descriptorPool, nullptr);
	}
	if (descriptorSetLayout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(Engine::device, descriptorSetLayout, nullptr);
	}
	if (graphicsPipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(Engine::device, graphicsPipeline, nullptr);
	}
	if (pipelineLayout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(Engine::device, pipelineLayout, nullptr);
	}

	if (computePipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(Engine::device, computePipeline, nullptr);
	}
	if (computePipelineLayout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(Engine::device, computePipelineLayout, nullptr);
	}
	if (computePool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(Engine::device, computePool, nullptr);
	}
	if (computeDescriptorSetLayout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(Engine::device, computeDescriptorSetLayout, nullptr);
	}

	auto D = [&](VkBuffer &b, VkDeviceMemory &m) {
		if (b) {
			vkDestroyBuffer(Engine::device, b, nullptr);
		}
		if (m) {
			vkFreeMemory(Engine::device, m, nullptr);
		}
		b = VK_NULL_HANDLE;
		m = VK_NULL_HANDLE;
	};

	D(nodesBuf, nodesMem);
	D(trisBuf, trisMem);
	D(posBuf, posMem);
	if (pickUBOMapped) {
		vkUnmapMemory(Engine::device, pickUBOMem);
	}
	D(pickUBO, pickUBOMem);
	if (hitMapped) {
		vkUnmapMemory(Engine::device, hitMem);
	}
	D(hitBuf, hitMem);
}

void Model::copyUBO() { memcpy(uniformBuffersMapped[Engine::currentFrame], &ubo, sizeof(ubo)); }


void Model::setOnMouseClick(std::function<void(int, int, int)> cb) {
    auto callback = [this, cb](int button, int action, int mods) {
        if(mouseIsOver) {
            cb(button, action, mods);
        }
    };
    Events::mouseCallbacks.push_back(callback);
}

void Model::setOnKeyboardKeyPress(std::function<void(int, int, int, int)> cb) {
    auto callback = [this, cb](int key, int scancode, int action, int mods) {
        if(selected) {
            cb(key, scancode, action, mods);
        }
    };
    Events::keyboardCallbacks.push_back(callback);
}

void Model::setMouseIsOver(bool over) {
	std::function<void()> enterCb;
	bool fireEnter = false;

	{
		std::lock_guard lk(m);
		if (over == mouseIsOver) {
			return; // no state change
		}
		// transition detection
		fireEnter = (!mouseIsOver && over);
		mouseIsOver = over;
		if (!over) {
			// wake any waiting exit-watcher
			cv.notify_all();
		}
		if (fireEnter)
			enterCb = onMouseEnter; // copy under lock
	}

	if (fireEnter) {
		if (enterCb)
			enterCb();
		// Arm the watcher ONCE, at the moment we enter.
		onMouseExitEvent();
	}
}

void Model::onMouseExitEvent() {
	if (!onMouseExit) {
		return;
	}

	if (watcher.joinable()) {
		watcher.request_stop();
		cv.notify_all();
	}

	watcher = Platform::jthread([this](Platform::stop_token st) {
		std::unique_lock lk(m);
		cv.wait(lk, [this, st] { return st.stop_requested() || !mouseIsOver; });
		if (st.stop_requested()) {
			return;
		}
		auto cb = onMouseExit;
		lk.unlock();
		if (cb) {
			cb();
		}
	});
}

void Model::updateUniformBuffer(optional<mat4> model, optional<mat4> view, optional<mat4> proj) {
	if (model.has_value()) {
		ubo.model = model.value();
	}
	if (view.has_value()) {
		ubo.view = view.value();
	}
	if (proj.has_value()) {
		ubo.proj = proj.value();
		ubo.proj[1][1] *= -1;
	}
}

void Model::updateUniformBuffer(const UBO &ubo) {
	this->ubo = ubo;
	this->ubo.proj[1][1] *= -1;
}

void Model::updateScreenParams(const ScreenParams &screenParams) { this->screenParams = screenParams; }

/*
 *  Compute setup
 * */

void Model::setRayTraceFromViewportPx(float px, float py, const VkViewport &vp) {
	// Handle possible negative-height viewports (legal in Vulkan).
	const float w = vp.width;
	const float hAbs = std::abs(vp.height);

	// Convert pixel -> viewport-local pixels (y respecting sign of height)
	const float xLocal = (px - vp.x) + 0.5f;
	const float yLocal = (vp.height >= 0.0f) ? (py - vp.y) + 0.5f  // origin at vp.y (top-left style)
											 : (vp.y - py) + 0.5f; // flipped viewport

	// Normalize -> [0,1]
	const float sx = xLocal / w;
	const float sy = yLocal / hAbs;

	// Map to NDC [-1,1] with +Y up (since you already flip proj[1][1]*=-1)
	const float ndcX = sx * 2.0f - 1.0f;
	const float ndcY = sy * 2.0f - 1.0f;

	rayTraceParams.mouseNdc = {ndcX, ndcY};
}

Model::AABB Model::merge(const AABB &a, const AABB &b) { return {glm::min(a.bmin, b.bmin), glm::max(a.bmax, b.bmax)}; }

Model::AABB Model::triAabb(const vec3 &A, const vec3 &B, const vec3 &C) const {
	AABB r;
	r.bmin = glm::min(A, glm::min(B, C));
	r.bmax = glm::max(A, glm::max(B, C));
	return r;
}

int Model::buildNode(std::vector<BuildTri> &tris, int begin, int end, int depth, std::vector<BuildNode> &out) {
	BuildNode node;
	node.b = {vec3(FLT_MAX), vec3(-FLT_MAX)};
	for (int i = begin; i < end; ++i)
		node.b = merge(node.b, tris[i].b);

	const int count = end - begin;
	const int maxLeaf = 8;
	if (count <= maxLeaf || depth > 32) {
		node.firstTri = begin;
		node.triCount = count;
		out.push_back(node);
		return (int)out.size() - 1;
	}

	// Split along largest axis by centroid median
	vec3 ext = node.b.bmax - node.b.bmin;
	int axis = (ext.x > ext.y && ext.x > ext.z) ? 0 : (ext.y > ext.z ? 1 : 2);
	int mid = (begin + end) / 2;
	std::nth_element(tris.begin() + begin, tris.begin() + mid, tris.begin() + end, [axis](const BuildTri &a, const BuildTri &b) { return a.centroid[axis] < b.centroid[axis]; });

	int leftIdx = buildNode(tris, begin, mid, depth + 1, out);
	int rightIdx = buildNode(tris, mid, end, depth + 1, out);

	node.left = leftIdx;
	node.right = rightIdx;
	out.push_back(node);
	return (int)out.size() - 1;
}

void Model::buildBVH() {}

void Model::updateComputeUniformBuffer() {}

void Model::compute() {}

void Model::updateRayTraceUniformBuffer() {
	if (!rayTracingEnabled) {
		return;
	}

    float mousePx = 0.0f, mousePy = 0.0f;
    Platform::GetPointerInFramebufferPixels(mousePx, mousePy);

	setRayTraceFromViewportPx(mousePx, mousePy, screenParams.viewport);

	mat4 invVP = inverse(ubo.proj * ubo.view);
	mat4 invV = inverse(ubo.view);

	vec3 camPos = vec3(invV[3]);

	PickingUBO p{};
	p.invViewProj = invVP;
	p.invModel = inverse(ubo.model);
	p.mouseNdc = rayTraceParams.mouseNdc;
	p.camPos = rayTraceParams.camPos == vec3(0.0f) ? camPos : rayTraceParams.camPos;
	p._pad = rayTraceParams._pad0;

	std::memcpy(pickUBOMapped, &p, sizeof(PickingUBO));

	// if (hitMapped) {
	// 	const uint32_t code = hitMapped->hit;
	// 	if (code != 0) {
	// 		std::cout << "[PickDBG] code=" << code << " primId=" << hitMapped->primId << " t=" << hitMapped->t << std::endl;

	// 		// Important: clear so you only print once per event
	// 		hitMapped->hit = 0;
	// 	}
	// }

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

void Model::createComputeDescriptorSetLayout() {
	// set=0 bindings: 0=nodes SSBO, 1=tris SSBO, 2=positions SSBO, 3=UBO, 4=result SSBO
	VkDescriptorSetLayoutBinding b[5]{};

	b[0].binding = 0;
	b[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	b[0].descriptorCount = 1;
	b[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	b[1].binding = 1;
	b[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	b[1].descriptorCount = 1;
	b[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	b[2].binding = 2;
	b[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	b[2].descriptorCount = 1;
	b[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	b[3].binding = 3;
	b[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	b[3].descriptorCount = 1;
	b[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	b[4].binding = 4;
	b[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	b[4].descriptorCount = 1;
	b[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
	ci.bindingCount = 5;
	ci.pBindings = b;

	if (vkCreateDescriptorSetLayout(Engine::device, &ci, nullptr, &computeDescriptorSetLayout) != VK_SUCCESS)
		throw std::runtime_error("compute DSL failed");

	// Small pool for one set
	std::array<VkDescriptorPoolSize, 2> ps{};
	ps[0] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4};
	ps[1] = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1};

	VkDescriptorPoolCreateInfo pci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
	pci.maxSets = 1;
	pci.poolSizeCount = (uint32_t)ps.size();
	pci.pPoolSizes = ps.data();

	if (vkCreateDescriptorPool(Engine::device, &pci, nullptr, &computePool) != VK_SUCCESS) {
		throw std::runtime_error("compute pool failed");
	}
}

void Model::createShaderStorageBuffers() {
	buildBVH();

	auto createHostVisible = [&](VkDeviceSize sz, VkBufferUsageFlags usage, VkBuffer &buf, VkDeviceMemory &mem, void **mapped) {
		if (sz == 0)
			throw std::runtime_error("createHostVisible: size is 0");
		Engine::createBuffer(sz, usage, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, buf, mem);
		if (mapped) {
			vkMapMemory(Engine::device, mem, 0, sz, 0, mapped);
		}
	};

	auto createDeviceLocalUpload = [&](const void *src, VkDeviceSize sz, VkBufferUsageFlags usage, VkBuffer &buf, VkDeviceMemory &mem) {
		if (sz == 0)
			throw std::runtime_error("createDeviceLocalUpload: size is 0");
		// staging
		VkBuffer stg;
		VkDeviceMemory stgMem;
		Engine::createBuffer(sz, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stg, stgMem);
		void *p;
		vkMapMemory(Engine::device, stgMem, 0, sz, 0, &p);
		std::memcpy(p, src, sz);
		vkUnmapMemory(Engine::device, stgMem);
		// device local
		Engine::createBuffer(sz, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, buf, mem);
		Engine::copyBuffer(stg, buf, sz);
		vkDestroyBuffer(Engine::device, stg, nullptr);
		vkFreeMemory(Engine::device, stgMem, nullptr);
	};

	// Sanity: BVH/triangles must exist
	if (bvhNodes.empty() || triGPU.empty() || posGPU.empty()) {
		throw std::runtime_error("BVH/TRI/POS data missing (check vertices/indices and buildBVH).");
	}

	// Nodes SSBO
	createDeviceLocalUpload(bvhNodes.data(), sizeof(BVHNodeGPU) * bvhNodes.size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, nodesBuf, nodesMem);

	// Tris SSBO
	createDeviceLocalUpload(triGPU.data(), sizeof(TriIndexGPU) * triGPU.size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, trisBuf, trisMem);

	// Positions SSBO (std430-friendly vec4, shader reads .xyz)
	std::vector<glm::vec4> posPacked;
	posPacked.reserve(posGPU.size());
	for (const auto &p : posGPU) {
		posPacked.emplace_back(p, 1.0f);
	}

	createDeviceLocalUpload(posPacked.data(), sizeof(glm::vec4) * posPacked.size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, posBuf, posMem);

	// UBO (host visible)
	createHostVisible(sizeof(PickingUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, pickUBO, pickUBOMem, &pickUBOMapped);

	// Result SSBO (host visible)
	createHostVisible(sizeof(HitOutCPU), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, hitBuf, hitMem, (void **)&hitMapped);

	std::memset(hitMapped, 0, sizeof(HitOutCPU));
}

void Model::createComputeDescriptorSets() {
	VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
	ai.descriptorPool = computePool;
	ai.descriptorSetCount = 1;
	ai.pSetLayouts = &computeDescriptorSetLayout;

	if (vkAllocateDescriptorSets(Engine::device, &ai, &computeDescriptorSet) != VK_SUCCESS)
		throw std::runtime_error("compute DS alloc failed");

	VkDescriptorBufferInfo nb{nodesBuf, 0, VK_WHOLE_SIZE};
	VkDescriptorBufferInfo tb{trisBuf, 0, VK_WHOLE_SIZE};
	VkDescriptorBufferInfo pb{posBuf, 0, VK_WHOLE_SIZE};
	VkDescriptorBufferInfo ub{pickUBO, 0, sizeof(PickingUBO)};
	VkDescriptorBufferInfo rb{hitBuf, 0, sizeof(HitOutCPU)};

	std::array<VkWriteDescriptorSet, 5> w{};
	auto W = [&](int i, uint32_t binding, VkDescriptorType t, const VkDescriptorBufferInfo *bi) {
		w[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w[i].dstSet = computeDescriptorSet;
		w[i].dstBinding = binding;
		w[i].descriptorType = t;
		w[i].descriptorCount = 1;
		w[i].pBufferInfo = bi;
	};
	W(0, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &nb);
	W(1, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &tb);
	W(2, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &pb);
	W(3, 3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &ub);
	W(4, 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &rb);

	vkUpdateDescriptorSets(Engine::device, (uint32_t)w.size(), w.data(), 0, nullptr);
}

void Model::createComputePipeline() {
	rayTracingProgram = Assets::compileShaderProgram(rayTracingShaderPath);
	if (rayTracingProgram.computeShader == VK_NULL_HANDLE) {
		// Fallback: try compiling/loading here if your Engine doesn't do it.
		throw std::runtime_error("compute shader missing (expect raytracing.comp)!");
	}

	// Pipeline layout (compute-only set layout)
	VkPipelineLayoutCreateInfo pli{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	pli.setLayoutCount = 1;
	pli.pSetLayouts = &computeDescriptorSetLayout;
	if (vkCreatePipelineLayout(Engine::device, &pli, nullptr, &computePipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("compute pipeline layout failed");
	}

	VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
	ci.stage = Engine::createShaderStageInfo(rayTracingProgram.computeShader, VK_SHADER_STAGE_COMPUTE_BIT);
	ci.layout = computePipelineLayout;

	if (vkCreateComputePipelines(Engine::device, VK_NULL_HANDLE, 1, &ci, nullptr, &computePipeline) != VK_SUCCESS) {
		throw std::runtime_error("compute pipeline failed");
	}
}

void Model::rayTrace() {
	if (!rayTracingEnabled) {
		return;
	}

	VkCommandBuffer cmd = Engine::currentComputeCommandBuffer();
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 1, &computeDescriptorSet, 0, nullptr);
	vkCmdDispatch(cmd, 1, 1, 1);

	// Make the write to hitBuf visible to the host when the queue completes
	VkMemoryBarrier mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
	mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	mb.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &mb, 0, nullptr, 0, nullptr);
}

/*
 * Graphics setup
 * */

void Model::createDescriptorSetLayout() {
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	uboLayoutBinding.pImmutableSamplers = nullptr;

	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &uboLayoutBinding;

	if (vkCreateDescriptorSetLayout(Engine::device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor set layout!");
	}
}

void Model::createVertexBuffer() {}

void Model::createIndexBuffer() {
	VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

	Engine::createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	void *data;
	vkMapMemory(Engine::device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, indices.data(), (size_t)bufferSize);
	vkUnmapMemory(Engine::device, stagingBufferMemory);

	Engine::createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);

	Engine::copyBuffer(stagingBuffer, indexBuffer, bufferSize);

	vkDestroyBuffer(Engine::device, stagingBuffer, nullptr);
	vkFreeMemory(Engine::device, stagingBufferMemory, nullptr);
}

void Model::createUniformBuffers() {
	VkDeviceSize bufferSize = sizeof(UBO);

	uniformBuffers.resize(Engine::MAX_FRAMES_IN_FLIGHT);
	uniformBuffersMemory.resize(Engine::MAX_FRAMES_IN_FLIGHT);
	uniformBuffersMapped.resize(Engine::MAX_FRAMES_IN_FLIGHT);

	for (size_t i = 0; i < Engine::MAX_FRAMES_IN_FLIGHT; i++) {
		Engine::createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersMemory[i]);
		vkMapMemory(Engine::device, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
	}
}

void Model::createDescriptorPool() {
	poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSize.descriptorCount = static_cast<uint32_t>(Engine::MAX_FRAMES_IN_FLIGHT);

	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;
	poolInfo.maxSets = static_cast<uint32_t>(Engine::MAX_FRAMES_IN_FLIGHT);

	if (vkCreateDescriptorPool(Engine::device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor pool");
	}
}

void Model::createDescriptorSets() {
	vector<VkDescriptorSetLayout> layouts(Engine::MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(Engine::MAX_FRAMES_IN_FLIGHT);
	allocInfo.pSetLayouts = layouts.data();

	descriptorSets.resize(Engine::MAX_FRAMES_IN_FLIGHT);

	if (vkAllocateDescriptorSets(Engine::device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate descritor set");
	}

	for (size_t i = 0; i < Engine::MAX_FRAMES_IN_FLIGHT; i++) {
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = uniformBuffers[i];
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(UBO);

		VkWriteDescriptorSet descriptorWrite{};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = descriptorSets[i];
		descriptorWrite.dstBinding = 0;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pBufferInfo = &bufferInfo;

		vkUpdateDescriptorSets(Engine::device, 1, &descriptorWrite, 0, nullptr);
	}
}

void Model::setupGraphicsPipeline() {}

void Model::createGraphicsPipeline() {
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	shaderProgram = Assets::compileShaderProgram(shaderPath);
	shaderStages = {Engine::createShaderStageInfo(shaderProgram.vertexShader, VK_SHADER_STAGE_VERTEX_BIT), Engine::createShaderStageInfo(shaderProgram.fragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT)};

	// Viewport and Scissor State (using dynamic states)
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	// Rasterization State
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;

	// Multisampling State
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	// Depth Stencil
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;

	// Color Blending Attachment
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	// Color Blending State
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	// Pipeline Layout
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 0;

    setupGraphicsPipeline();

	colorBlending.pAttachments = &colorBlendAttachment;

	if (vkCreatePipelineLayout(Engine::device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create pipeline layout!");
	}

	// Graphics Pipeline Creation
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineInfo.pStages = shaderStages.data();
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = nullptr;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = pipelineLayout;
	pipelineInfo.renderPass = Engine::renderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;
	pipelineInfo.pDepthStencilState = &depthStencil;

	if (vkCreateGraphicsPipelines(Engine::device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create graphics pipeline!");
	}
}

void Model::render() {
	copyUBO();

	vkCmdBindPipeline(Engine::currentCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

	vkCmdSetViewport(Engine::currentCommandBuffer(), 0, 1, &screenParams.viewport);

	vkCmdSetScissor(Engine::currentCommandBuffer(), 0, 1, &screenParams.scissor);

	VkBuffer vertexBuffers[] = {vertexBuffer};
	VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers(Engine::currentCommandBuffer(), 0, 1, vertexBuffers, offsets);
	vkCmdBindIndexBuffer(Engine::currentCommandBuffer(), indexBuffer, 0, VK_INDEX_TYPE_UINT16);

	vkCmdBindDescriptorSets(Engine::currentCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[Engine::currentFrame], 0, nullptr);

	vkCmdDrawIndexed(Engine::currentCommandBuffer(), static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
}
