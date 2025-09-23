#pragma once

#include "colors.hpp"
#include "model.hpp"
#include <vulkan/vulkan_core.h>

class Polygon : public Model {
  public:
	Polygon(Polygon &&) = delete;
	Polygon(const Polygon &) = delete;
	Polygon &operator=(Polygon &&) = delete;
	Polygon &operator=(const Polygon &) = delete;
	~Polygon();

	struct Params {
		vec4 color = Colors::Green;
		vec4 outlineColor = Colors::Green;
		float outlineWidth = 0.0f; // pixels
		float _pad0 = 0.0f, _pad1 = 0.0f, _pad2 = 0.0f;
	};

	struct Vertex {
		vec3 pos;
		vec4 color;
		vec3 bary;	   // per-triangle barycentric
		vec3 edgeMask; // which edges to draw (opposite vertex 0/1/2)

		static VkVertexInputBindingDescription getBindingDescription() {
			VkVertexInputBindingDescription d{};
			d.binding = 0;
			d.stride = sizeof(Vertex);
			d.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			return d;
		}

		static array<VkVertexInputAttributeDescription, 4> getAttributeDescriptions() {
			array<VkVertexInputAttributeDescription, 4> a{};

			a[0].location = 0;
			a[0].binding = 0;
			a[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			a[0].offset = offsetof(Vertex, pos);

			a[1].location = 1;
			a[1].binding = 0;
			a[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
			a[1].offset = offsetof(Vertex, color);

			a[2].location = 2;
			a[2].binding = 0;
			a[2].format = VK_FORMAT_R32G32B32_SFLOAT; // bary
			a[2].offset = offsetof(Vertex, bary);

			a[3].location = 3;
			a[3].binding = 0;
			a[3].format = VK_FORMAT_R32G32B32_SFLOAT; // edgeMask
			a[3].offset = offsetof(Vertex, edgeMask);

			return a;
		}
	};

	Polygon(Scene *scene, const MVP &ubo, ScreenParams &screenParams, const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices);

	Params params{};

	void render() override;

	struct EdgeKey {
		uint32_t a, b;
		bool operator==(const EdgeKey &o) const noexcept { return a == o.a && b == o.b; }
	};

	struct EdgeKeyHash {
		size_t operator()(const EdgeKey &k) const noexcept { return (size_t(k.a) << 32) ^ size_t(k.b); }
	};

    template <typename T>
	static void expandForOutlines(const std::vector<T> &inVerts, const std::vector<uint32_t> &inIdx, std::vector<T> &outVerts, std::vector<uint32_t> &outIdx) {
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

			auto makeV = [&](const T &vin, glm::vec3 bary) {
				T v = vin;
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

  protected:
	void buildBVH() override;
	void createDescriptorSetLayout() override;
    void createUniformBuffers() override;
	void createBindingDescriptions() override;
	void createDescriptorPool() override;
	void createDescriptorSets() override;

  private:
	struct SubmeshRanges {
		uint32_t fillCount = 0;
		uint32_t shellFillFirst = 0, shellFillCount = 0;
		uint32_t shellStrokeFirst = 0, shellStrokeCount = 0;
	};
	SubmeshRanges submesh;

	// original input (pos/color only is fine; bary will be rebuilt)
	std::vector<Vertex> inputVertices;
	std::vector<uint16_t> inputIndices;

	// expanded (barycentric) mesh we actually upload
	std::vector<Vertex> vertices;

	VkDescriptorSetLayoutBinding paramsBinding{};
	std::vector<VkBuffer> paramsBuffers;
	std::vector<VkDeviceMemory> paramsBuffersMemory;
	std::vector<void *> paramsBuffersMapped;
};
