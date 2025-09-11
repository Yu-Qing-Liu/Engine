#include "rectangle.hpp"
#include "colors.hpp"
#include "engine.hpp"
#include <cstring>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

Rectangle::Rectangle(Scene &scene, const UBO &ubo, ScreenParams &screenParams) : Model(scene, ubo, screenParams, Assets::shaderRootPath + "/rectangle") {
	// default colors
	params.color = Colors::Red;
	params.outlineColor = Colors::Yellow;
	params.outlineWidth = 2.0f;
    params.borderRadius = 15.0f;

    indices = {
        0, 1, 2, 2, 3, 0
    };

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

Rectangle::~Rectangle() {
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

void Rectangle::buildBVH() {
	// Gather positions and triangles from current mesh
	posGPU.clear();
	triGPU.clear();
	if (!vertices.empty()) {
		posGPU.reserve(vertices.size());
		for (auto &v : vertices) {
			posGPU.push_back(v.pos);
		}
	} else {
		throw std::runtime_error("BVH build: no vertices");
	}

	std::vector<BuildTri> tris;
	tris.reserve(indices.size() / 3);
	for (size_t t = 0; t < indices.size(); t += 3) {
		uint32_t i0 = indices[t + 0], i1 = indices[t + 1], i2 = indices[t + 2];
		const vec3 &A = posGPU[i0];
		const vec3 &B = posGPU[i1];
		const vec3 &C = posGPU[i2];
		BuildTri bt;
		bt.i0 = i0;
		bt.i1 = i1;
		bt.i2 = i2;
		bt.b = triAabb(A, B, C);
		bt.centroid = (A + B + C) * (1.0f / 3.0f);
		tris.push_back(bt);

		triGPU.push_back({i0, i1, i2, 0});
	}

	// Build tree into BuildNode list (temporary)
	std::vector<BuildNode> tmp;
	tmp.reserve(tris.size() * 2);
	int root = buildNode(tris, 0, (int)tris.size(), 0, tmp);

	// Rebuild GPU triangles in the final order used by leaves
	triGPU.clear();
	triGPU.reserve(tris.size());
	for (const auto &t : tris) {
		triGPU.push_back({t.i0, t.i1, t.i2, 0u});
	}

	// Flatten to GPU nodes (depth-first, implicit right=left+1 for internal nodes)
	bvhNodes.clear();
	bvhNodes.resize(tmp.size());
	// Map temp indices to linear DFS order
	std::vector<int> map(tmp.size(), -1);
	std::function<void(int, int &)> dfs = [&](int ni, int &outIdx) {
		int my = outIdx++;
		map[ni] = my;
		if (tmp[ni].triCount == 0) {
			dfs(tmp[ni].left, outIdx);
			dfs(tmp[ni].right, outIdx);
		}
	};
	int counter = 0;
	dfs(root, counter);

	// Fill nodes in DFS order
	std::function<void(int)> emit = [&](int ni) {
		int me = map[ni];
		const BuildNode &n = tmp[ni];
		BVHNodeGPU gn;
		gn.bmin = vec4(n.b.bmin, 0.0f);
		gn.bmax = vec4(n.b.bmax, 0.0f);

		if (n.triCount == 0) {
			gn.leftFirst = map[n.left];
			gn.rightOrCount = (uint32_t(map[n.right]) | 0x80000000u); // INTERNAL
			bvhNodes[me] = gn;
			emit(n.left);
			emit(n.right);
		} else {
			gn.leftFirst = n.firstTri;
			gn.rightOrCount = n.triCount; // leaf => count, no high bit
			bvhNodes[me] = gn;
		}
	};
	emit(root);
}

void Rectangle::createBindingDescriptions() {
	bindingDescription = Vertex::getBindingDescription();
	auto attrs = Vertex::getAttributeDescriptions();
	attributeDescriptions = std::vector<VkVertexInputAttributeDescription>(attrs.begin(), attrs.end());
}

void Rectangle::createDescriptorSetLayout() {
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

void Rectangle::createDescriptorPool() {
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

void Rectangle::createParamsBuffer() {
	VkDeviceSize sz = sizeof(Params);
	paramsBuffers.resize(Engine::MAX_FRAMES_IN_FLIGHT);
	paramsBuffersMemory.resize(Engine::MAX_FRAMES_IN_FLIGHT);
	paramsBuffersMapped.resize(Engine::MAX_FRAMES_IN_FLIGHT);

	for (size_t i = 0; i < Engine::MAX_FRAMES_IN_FLIGHT; ++i) {
		Engine::createBuffer(sz, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, paramsBuffers[i], paramsBuffersMemory[i]);
		vkMapMemory(Engine::device, paramsBuffersMemory[i], 0, sz, 0, &paramsBuffersMapped[i]);
	}
}

void Rectangle::createDescriptorSets() {
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

void Rectangle::setupGraphicsPipeline() {
	rasterizer.cullMode = VK_CULL_MODE_NONE;

	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_FALSE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
}

void Rectangle::render() {
	// Update per-frame data
	std::memcpy(paramsBuffersMapped[Engine::currentFrame], &params, sizeof(params));
    Model::render();
}
