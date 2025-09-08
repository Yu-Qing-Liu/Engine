#include "polygon.hpp"
#include "colors.hpp"
#include "engine.hpp"
#include <cstring>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

Polygon::Polygon(Scene &scene, const UBO &ubo, ScreenParams &screenParams, const std::vector<Vertex> &vertices, const std::vector<uint16_t> &indices) : inputVertices(vertices), inputIndices(indices), Model(scene, ubo, screenParams, Assets::shaderRootPath + "/polygon", /*indices ignored here*/ {}) {
	// default colors
	params.color = Colors::RED;
	params.outlineColor = Colors::YELLOW;
	params.outlineWidth = 2.0f;

	std::vector<Vertex> expandedVerts;
	std::vector<uint16_t> expandedIdx;
	expandForOutlines(inputVertices, inputIndices, this->vertices, this->indices);

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

struct EdgeKey {
	uint32_t a, b;
	bool operator==(const EdgeKey &o) const noexcept { return a == o.a && b == o.b; }
};

struct EdgeKeyHash {
	size_t operator()(const EdgeKey &k) const noexcept { return (size_t(k.a) << 32) ^ size_t(k.b); }
};

void Polygon::expandForOutlines(const std::vector<Vertex> &inVerts, const std::vector<uint16_t> &inIdx, std::vector<Vertex> &outVerts, std::vector<uint16_t> &outIdx) {
	struct Edge {
		uint32_t a, b;
		int tri0 = -1;
		int tri1 = -1;
	};
	struct EdgeKey {
		uint32_t a, b;
		bool operator==(const EdgeKey &o) const noexcept { return a == o.a && b == o.b; }
	};
	struct EdgeKeyHash {
		size_t operator()(const EdgeKey &k) const noexcept { return (size_t(k.a) << 32) ^ size_t(k.b); }
	};

	if (inIdx.size() % 3 != 0)
		throw std::runtime_error("indices not multiple of 3");

	const int triCount = (int)inIdx.size() / 3;

	// 1) Face normals
	std::vector<glm::vec3> triN(triCount);
	for (int t = 0; t < triCount; ++t) {
		uint32_t i0 = inIdx[3 * t + 0], i1 = inIdx[3 * t + 1], i2 = inIdx[3 * t + 2];
		glm::vec3 A = inVerts[i0].pos;
		glm::vec3 B = inVerts[i1].pos;
		glm::vec3 C = inVerts[i2].pos;
		triN[t] = glm::normalize(glm::cross(B - A, C - A));
	}

	// 2) Edge adjacency
	std::unordered_map<EdgeKey, Edge, EdgeKeyHash> edges;
	edges.reserve(inIdx.size());
	auto addEdge = [&](uint32_t u, uint32_t v, int tri) {
		EdgeKey k{std::min(u, v), std::max(u, v)};
		auto &e = edges[k];
		e.a = k.a;
		e.b = k.b;
		if (e.tri0 == -1)
			e.tri0 = tri;
		else
			e.tri1 = tri;
	};

	for (int t = 0; t < triCount; ++t) {
		uint32_t i0 = inIdx[3 * t + 0], i1 = inIdx[3 * t + 1], i2 = inIdx[3 * t + 2];
		addEdge(i0, i1, t);
		addEdge(i1, i2, t);
		addEdge(i2, i0, t);
	}

	// crease threshold (degrees -> cos)
	const float deg = 30.0f; // tweak
	const float creaseCos = std::cos(glm::radians(deg));

	// 3) For each triangle, decide which of its three edges are "hard"
	auto edgeIsHard = [&](uint32_t u, uint32_t v) {
		EdgeKey k{std::min(u, v), std::max(u, v)};
		auto it = edges.find(k);
		if (it == edges.end())
			return false; // shouldn't happen
		const Edge &e = it->second;
		if (e.tri1 == -1)
			return true; // boundary
		// two faces: compare normals
		float d = glm::dot(triN[e.tri0], triN[e.tri1]);
		return d < creaseCos;
	};

	outVerts.clear();
	outIdx.clear();
	outVerts.reserve(inIdx.size());
	outIdx.reserve(inIdx.size());

	for (int t = 0; t < triCount; ++t) {
		uint32_t i0 = inIdx[3 * t + 0], i1 = inIdx[3 * t + 1], i2 = inIdx[3 * t + 2];

		// mask.x corresponds to edge opposite v0 (edge i1-i2)
		// mask.y -> opposite v1 (edge i2-i0)
		// mask.z -> opposite v2 (edge i0-i1)
		glm::vec3 mask(0.0f);
		if (edgeIsHard(i1, i2))
			mask.x = 1.0f;
		if (edgeIsHard(i2, i0))
			mask.y = 1.0f;
		if (edgeIsHard(i0, i1))
			mask.z = 1.0f;

		auto makeV = [&](const Vertex &vin, glm::vec3 bary) {
			Vertex v = vin;
			v.bary = bary;
			v.edgeMask = mask;
			return v;
		};

		uint32_t base = (uint32_t)outVerts.size();
		outVerts.push_back(makeV(inVerts[i0], {1, 0, 0}));
		outVerts.push_back(makeV(inVerts[i1], {0, 1, 0}));
		outVerts.push_back(makeV(inVerts[i2], {0, 0, 1}));

		outIdx.push_back(uint16_t(base + 0));
		outIdx.push_back(uint16_t(base + 1));
		outIdx.push_back(uint16_t(base + 2));
	}
}

void Polygon::buildBVH() {
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
