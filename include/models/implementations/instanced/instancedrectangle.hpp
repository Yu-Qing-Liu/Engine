#pragma once

#include "colors.hpp"
#include "instancedmodel.hpp"
#include <memory>
#include <unordered_map>

using std::shared_ptr;
using std::unordered_map;

struct InstancedRectangleData {
	mat4 model{1.0f};
	vec4 color{Colors::Green};
	vec4 outlineColor{Colors::Transparent(0.0f)};
	float outlineWidth{0.0f};
	float borderRadius{0.0f};
	float _pad0[2]{0.0f, 0.0f};

	InstancedRectangleData() = default;

	InstancedRectangleData(float x, float y, vec2 size, vec4 color = Colors::Green, vec4 outlineColor = Colors::Transparent(0.0f), float outlineWidth = 0.0f, float borderRadius = 0.0f) : model(glm::translate(mat4(1.0f), vec3(x, y, 0.0f)) * glm::scale(mat4(1.0), vec3(size, 1.0))), color(color), outlineColor(outlineColor), outlineWidth(outlineWidth), borderRadius(borderRadius) {}
};

class InstancedRectangle : public InstancedModel<InstancedRectangleData> {
  public:
	InstancedRectangle(InstancedRectangle &&) = delete;
	InstancedRectangle(const InstancedRectangle &) = delete;
	InstancedRectangle &operator=(InstancedRectangle &&) = delete;
	InstancedRectangle &operator=(const InstancedRectangle &) = delete;

	struct Vertex {
		vec3 pos;

		static VkVertexInputBindingDescription getBindingDescription() {
			VkVertexInputBindingDescription bd{};
			bd.binding = 0;
			bd.stride = sizeof(Vertex);
			bd.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			return bd;
		}

		static std::array<VkVertexInputAttributeDescription, 1> getAttributeDescriptions() {
			std::array<VkVertexInputAttributeDescription, 1> a{};
			a[0].binding = 0;
			a[0].location = 0;
			a[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			a[0].offset = offsetof(Vertex, pos);
			return a;
		}
	};

	InstancedRectangle(Scene *scene, const MVP &ubo, ScreenParams &screenParams, shared_ptr<unordered_map<int, InstancedRectangleData>> instances, uint32_t maxInstances = 65536);
	~InstancedRectangle() override = default;

  protected:
	// ---- Model overrides we need for instancing ----
	void createBindingDescriptions() override; // adds instance binding + attrs
	void setupGraphicsPipeline() override;	   // culling/depth tweaks (optional)
	void buildBVH() override;

  private:
	std::vector<Vertex> vertices = {
		{{-0.5f, -0.5f, 0.0f}},
		{{0.5f, -0.5f, 0.0f}},
		{{0.5f, 0.5f, 0.0f}},
		{{-0.5f, 0.5f, 0.0f}},
	};
};
