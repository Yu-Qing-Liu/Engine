#pragma once

#include "model.hpp"
#include <vulkan/vulkan_core.h>

class Polygon : public Model {
  public:
	Polygon(Polygon &&) = default;
	Polygon(const Polygon &) = delete;
	Polygon &operator=(Polygon &&) = delete;
	Polygon &operator=(const Polygon &) = delete;
	~Polygon() = default;

	struct Vertex {
		vec3 pos;
		vec4 color;

		static VkVertexInputBindingDescription getBindingDescription() {
			VkVertexInputBindingDescription bindingDescription{};
			bindingDescription.binding = 0;
			bindingDescription.stride = sizeof(Vertex);
			bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			return bindingDescription;
		}

		static array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
			array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

			attributeDescriptions[0].binding = 0;
			attributeDescriptions[0].location = 0;
			attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[0].offset = offsetof(Vertex, pos);

			attributeDescriptions[1].binding = 0;
			attributeDescriptions[1].location = 1;
			attributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
			attributeDescriptions[1].offset = offsetof(Vertex, color);

			return attributeDescriptions;
		}
	};

	Polygon(Scene &scene, const UBO &ubo, ScreenParams &screenParams, const std::vector<Vertex> &vertices, const std::vector<uint16_t> &indices);

  protected:
	void buildBVH() override;
	void createBindingDescriptions() override;

  private:
	vector<Vertex> vertices;
};
