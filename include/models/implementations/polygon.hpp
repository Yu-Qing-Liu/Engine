#pragma once

#include "model.hpp"
#include <vulkan/vulkan_core.h>

class Polygon : public Model {
  public:
	Polygon(Polygon &&) = delete;
	Polygon(const Polygon &) = delete;
	Polygon &operator=(Polygon &&) = delete;
	Polygon &operator=(const Polygon &) = delete;
	~Polygon();

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

	Polygon(Scene &scene, const UBO &ubo, ScreenParams &screenParams, const std::vector<Vertex> &vertices, const std::vector<uint16_t> &indices);

	void render() override;

  protected:
	void buildBVH() override;
	void createDescriptorSetLayout() override;
	void createBindingDescriptions() override;
	void createDescriptorPool() override;
	void createDescriptorSets() override;

	void createParamsBuffer();

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

	void expandForOutlines(const std::vector<Vertex> &inVerts, const std::vector<uint16_t> &inIdx, std::vector<Vertex> &outVerts, std::vector<uint16_t> &outIdx);
};
