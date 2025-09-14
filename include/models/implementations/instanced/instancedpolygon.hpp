#pragma once

#include "colors.hpp"
#include "instancedmodel.hpp"
#include <memory>
#include <unordered_map>

using std::shared_ptr;
using std::unordered_map;

struct InstancedPolygonData {
	mat4 model{1.0f};
	vec4 color{Colors::Green};
	vec4 outlineColor{Colors::Green};
	float outlineWidth{0.0f};
	float _pad0 = 0.0f, _pad1 = 0.0f, _pad2 = 0.0f;

	InstancedPolygonData() = default;

	InstancedPolygonData(vec3 pos, vec3 size, vec4 color = Colors::Green, vec4 outlineColor = Colors::Green, float outlineWidth = 0.0f, float borderRadius = 0.0f) : model(glm::translate(mat4(1.0f), pos) * glm::scale(mat4(1.0), size)), color(color), outlineColor(outlineColor), outlineWidth(outlineWidth) {}
};

class InstancedPolygon : public InstancedModel<InstancedPolygonData> {
  public:
	InstancedPolygon(InstancedPolygon &&) = delete;
	InstancedPolygon(const InstancedPolygon &) = delete;
	InstancedPolygon &operator=(InstancedPolygon &&) = delete;
	InstancedPolygon &operator=(const InstancedPolygon &) = delete;

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

	InstancedPolygon(const UBO &ubo, ScreenParams &screenParams, const vector<Vertex> &vertices, const vector<uint16_t> &indices, shared_ptr<unordered_map<int, InstancedPolygonData>> instances, uint32_t maxInstances = 65536);
	~InstancedPolygon() override = default;

  protected:
	// ---- Model overrides we need for instancing ----
	void createBindingDescriptions() override; // adds instance binding + attrs
	void setupGraphicsPipeline() override;	   // culling/depth tweaks (optional)

  private:
	std::vector<Vertex> vertices;

	// We’ll store both bindings here to pass them in createGraphicsPipeline()
	VkVertexInputBindingDescription vertexBD{};	  // binding 0 (per-vertex)
	VkVertexInputBindingDescription instanceBD{}; // binding 1 (per-instance)

	std::array<VkVertexInputBindingDescription, 2> bindings{};
};
