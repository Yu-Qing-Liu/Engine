#include "polygon.hpp"
#include "engine.hpp"
#include "scene.hpp"
#include "scenes.hpp"
#include <cmath>
#include <glm/gtc/constants.hpp>
#include <vulkan/vulkan_core.h>

Polygon::Polygon(Scene *scene) : Model(scene) {}

template <typename T> void Polygon::expandForOutlines(const std::vector<T> &inVerts, const std::vector<uint32_t> &inIdx, std::vector<Attributes> &outVerts, std::vector<uint32_t> &outIdx) {
	struct Edge {
		uint32_t a{}, b{};
		int tri0{-1}, tri1{-1};
	};

	if (inIdx.size() % 3 != 0)
		throw std::runtime_error("indices not multiple of 3");
	const int triCount = int(inIdx.size() / 3);

	// 1) Face normals
	std::vector<vec3> triN(triCount);
	for (int t = 0; t < triCount; ++t) {
		const uint32_t i0 = inIdx[3 * t + 0], i1 = inIdx[3 * t + 1], i2 = inIdx[3 * t + 2];
		const vec3 A = inVerts[i0].pos;
		const vec3 B = inVerts[i1].pos;
		const vec3 C = inVerts[i2].pos;
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
		const uint32_t i0 = inIdx[3 * t + 0], i1 = inIdx[3 * t + 1], i2 = inIdx[3 * t + 2];
		addEdge(i0, i1, t);
		addEdge(i1, i2, t);
		addEdge(i2, i0, t);
	}

	// 3) Hard-edge test (crease threshold)
	const float creaseCos = std::cos(glm::radians(30.0f));
	auto edgeIsHard = [&](uint32_t u, uint32_t v) {
		EdgeKey k{std::min(u, v), std::max(u, v)};
		auto it = edges.find(k);
		if (it == edges.end())
			return false;
		const Edge &e = it->second;
		if (e.tri1 == -1)
			return true; // boundary
		const float d = glm::dot(triN[e.tri0], triN[e.tri1]);
		return d < creaseCos;
	};

	// 4) Emit expanded vertices
	outVerts.clear();
	outIdx.clear();
	outVerts.reserve(inIdx.size());
	outIdx.reserve(inIdx.size());

	for (int t = 0; t < triCount; ++t) {
		const uint32_t i0 = inIdx[3 * t + 0], i1 = inIdx[3 * t + 1], i2 = inIdx[3 * t + 2];

		// mask.x -> opposite v0 (edge i1-i2), mask.y -> opposite v1, mask.z -> opposite v2
		glm::vec3 mask(0.0f);
		if (edgeIsHard(i1, i2))
			mask.x = 1.0f;
		if (edgeIsHard(i2, i0))
			mask.y = 1.0f;
		if (edgeIsHard(i0, i1))
			mask.z = 1.0f;

		auto emitV = [&](uint32_t idx, glm::vec3 bary) {
			Attributes a{};
			a.pos = inVerts[idx].pos;
			a.color = inVerts[idx].color;
			a.bary = bary;
			a.edgeMask = mask;
			outVerts.push_back(a);
		};

		const uint32_t base = (uint32_t)outVerts.size();
		emitV(i0, {1, 0, 0});
		emitV(i1, {0, 1, 0});
		emitV(i2, {0, 0, 1});

		outIdx.push_back(base + 0);
		outIdx.push_back(base + 1);
		outIdx.push_back(base + 2);
	}
}

void Polygon::init(const std::vector<Vertex> &verts, const std::vector<uint32_t> &idx) {
	engine = scene->getScenes().getEngine();

	expandForOutlines<Vertex>(verts, idx, s_cpuVerts, s_cpuIdx);

	Mesh m{};
	m.vsrc.data = s_cpuVerts.data();
	m.vsrc.bytes = s_cpuVerts.size() * sizeof(Attributes);
	m.vsrc.stride = sizeof(Attributes);

	m.isrc.data = s_cpuIdx.data();
	m.isrc.count = s_cpuIdx.size();

	using F = VkFormat;

	// Binding 0: expanded per-vertex data
	m.vertexAttrs = {
		/* loc, bind,   format,                     offset */
		{0, 0, F::VK_FORMAT_R32G32B32_SFLOAT, uint32_t(offsetof(Attributes, pos))},
		{1, 0, F::VK_FORMAT_R32G32B32A32_SFLOAT, uint32_t(offsetof(Attributes, color))},
		{2, 0, F::VK_FORMAT_R32G32B32_SFLOAT, uint32_t(offsetof(Attributes, bary))},
		{3, 0, F::VK_FORMAT_R32G32B32_SFLOAT, uint32_t(offsetof(Attributes, edgeMask))},
	};

	// Binding 1: per-instance data (mat4 as 4x vec4 + colors + width)
	m.vertexAttrs.push_back({4, 1, F::VK_FORMAT_R32G32B32A32_SFLOAT, uint32_t(offsetof(InstanceData, model) + sizeof(glm::vec4) * 0)});
	m.vertexAttrs.push_back({5, 1, F::VK_FORMAT_R32G32B32A32_SFLOAT, uint32_t(offsetof(InstanceData, model) + sizeof(glm::vec4) * 1)});
	m.vertexAttrs.push_back({6, 1, F::VK_FORMAT_R32G32B32A32_SFLOAT, uint32_t(offsetof(InstanceData, model) + sizeof(glm::vec4) * 2)});
	m.vertexAttrs.push_back({7, 1, F::VK_FORMAT_R32G32B32A32_SFLOAT, uint32_t(offsetof(InstanceData, model) + sizeof(glm::vec4) * 3)});
	m.vertexAttrs.push_back({8, 1, F::VK_FORMAT_R32G32B32A32_SFLOAT, uint32_t(offsetof(InstanceData, color))});
	m.vertexAttrs.push_back({9, 1, F::VK_FORMAT_R32G32B32A32_SFLOAT, uint32_t(offsetof(InstanceData, outlineColor))});
	m.vertexAttrs.push_back({10, 1, F::VK_FORMAT_R32_SFLOAT, uint32_t(offsetof(InstanceData, outlineWidth))});

	initInfo.mesh = m;

	// This tells your Model to create a binding description for binding=1
	// with VK_VERTEX_INPUT_RATE_INSTANCE and the given stride.
	initInfo.instanceStrideBytes = sizeof(InstanceData);

	initInfo.shaders = Assets::compileShaderProgram(Assets::shaderRootPath + "/polygon", engine->getDevice());

	Model::init();

	// Ensure a default instance exists
	InstanceData placeholder{};
	upsertInstance(0, placeholder);
}

void Polygon::initNGon(size_t n) {
	// Need at least a triangle
	if (n < 3)
		n = 3;

	std::vector<Vertex> verts;
	std::vector<uint32_t> idx;

	verts.reserve(n + 1);
	idx.reserve(n * 3);

	const float radius = 0.5f;

	// Center vertex
	Vertex center{};
	center.pos = vec3(0.0f, 0.0f, 0.0f);
	center.normal = vec3(0.0f, 0.0f, 1.0f);
	center.uv = glm::vec2(0.5f, 0.5f);
	center.color = vec4(1.0f); // white
	verts.push_back(center);

	// Ring vertices
	for (size_t i = 0; i < n; ++i) {
		float angle = float(i) / float(n) * glm::two_pi<float>();
		float x = radius * std::cos(angle);
		float y = radius * std::sin(angle);

		Vertex v{};
		v.pos = vec3(x, y, 0.0f);
		v.normal = vec3(0.0f, 0.0f, 1.0f);
		v.uv = glm::vec2(0.5f + 0.5f * std::cos(angle), 0.5f + 0.5f * std::sin(angle));
		v.color = vec4(1.0f);
		verts.push_back(v);
	}

	// Triangle fan from center
	for (uint32_t i = 0; i < uint32_t(n); ++i) {
		uint32_t i0 = 0;						   // center
		uint32_t i1 = 1 + i;					   // current ring vertex
		uint32_t i2 = 1 + ((i + 1) % uint32_t(n)); // next ring vertex

		idx.push_back(i0);
		idx.push_back(i1);
		idx.push_back(i2);
	}

	// Use the generic initializer
	init(verts, idx);
}

void Polygon::initCube() {
	std::vector<Vertex> verts;
	std::vector<uint32_t> idx;

	const float h = 0.5f;

	auto makeV = [&](float x, float y, float z) {
		Vertex v{};
		v.pos = vec3(x, y, z);
		v.color = vec4(1.0f);
		return v;
	};

	// 8 corners of the cube
	verts.push_back(makeV(-h, -h, -h)); // 0
	verts.push_back(makeV(h, -h, -h));	// 1
	verts.push_back(makeV(h, h, -h));	// 2
	verts.push_back(makeV(-h, h, -h));	// 3
	verts.push_back(makeV(-h, -h, h));	// 4
	verts.push_back(makeV(h, -h, h));	// 5
	verts.push_back(makeV(h, h, h));	// 6
	verts.push_back(makeV(-h, h, h));	// 7

	idx = std::vector<uint32_t>{
		4, 5, 6, 6, 7, 4, 1, 0, 3, 3, 2, 1, 0, 4, 7, 7, 3, 0, 5, 1, 2, 2, 6, 5, 3, 7, 6, 6, 2, 3, 0, 1, 5, 5, 4, 0,
	};

	init(verts, idx);
}

void Polygon::syncPickingInstances() { Model::syncPickingInstances<InstanceData>(); }

void Polygon::createGraphicsPipeline() {
	Model::createGraphicsPipeline();
	if (!cull_) {
		pipeline->graphicsPipeline.rasterizationStateCI.cullMode = VK_CULL_MODE_NONE;
		pipeline->graphicsPipeline.depthStencilStateCI.depthTestEnable = VK_FALSE;
		pipeline->graphicsPipeline.depthStencilStateCI.depthWriteEnable = VK_FALSE;
	}
}
