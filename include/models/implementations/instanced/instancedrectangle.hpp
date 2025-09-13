#include "colors.hpp"
#include "model.hpp"

class InstancedRectangle : public Model {
  public:
	InstancedRectangle();
	InstancedRectangle(InstancedRectangle &&) = delete;
	InstancedRectangle(const InstancedRectangle &) = delete;
	InstancedRectangle &operator=(InstancedRectangle &&) = delete;
	InstancedRectangle &operator=(const InstancedRectangle &) = delete;
	~InstancedRectangle() = default;

	InstancedRectangle(ScreenParams &screenParams);

	struct InstanceData {
		vec4 color = Colors::Green;
		vec4 outlineColor = Colors::Transparent(0.0);
		float outlineWidth = 0.0f; // pixels
		float borderRadius = 0.0f; // pixels
		float _pad1 = 0.0f, _pad2 = 0.0f;
	};

	struct Vertex {
		vec3 pos;

		static VkVertexInputBindingDescription getBindingDescription() {
			VkVertexInputBindingDescription bindingDescription{};
			bindingDescription.binding = 0;
			bindingDescription.stride = sizeof(Vertex);
			bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			return bindingDescription;
		}

		static array<VkVertexInputAttributeDescription, 1> getAttributeDescriptions() {
			array<VkVertexInputAttributeDescription, 1> attributeDescriptions{};

			attributeDescriptions[0].binding = 0;
			attributeDescriptions[0].location = 0;
			attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[0].offset = offsetof(Vertex, pos);

			return attributeDescriptions;
		}
	};
};
