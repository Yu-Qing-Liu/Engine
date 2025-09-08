#pragma once

#include "model.hpp"
#include <vulkan/vulkan_core.h>

class Rectangle : public Model {
  public:
	Rectangle(Rectangle &&) = delete;
	Rectangle(const Rectangle &) = delete;
	Rectangle &operator=(Rectangle &&) = delete;
	Rectangle &operator=(const Rectangle &) = delete;
	~Rectangle();

	Rectangle(Scene &scene, const UBO &ubo, ScreenParams &screenParams);

	void render() override;

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

  protected:
	void buildBVH() override;
	void createDescriptorSetLayout() override;
	void createBindingDescriptions() override;
	void createDescriptorPool() override;
	void createDescriptorSets() override;
	void createGraphicsPipeline() override;

	void createParamsBuffer();

  private:
	std::vector<Vertex> vertices = {
		{{-0.5f, -0.5f, 0.0f}},
		{{0.5f, -0.5f, 0.0f}},
		{{0.5f, 0.5f, 0.0f}},
		{{-0.5f, 0.5f, 0.0f}},
	};

	VkDescriptorSetLayoutBinding paramsBinding{};
	std::vector<VkBuffer> paramsBuffers;
	std::vector<VkDeviceMemory> paramsBuffersMemory;
	std::vector<void *> paramsBuffersMapped;
};
