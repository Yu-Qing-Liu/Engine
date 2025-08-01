#pragma once

#include "models/model.hpp"

class Triangle : public Model {
  public:
	Triangle(VkPhysicalDevice &physicalDevice, VkDevice &device, const std::string &modelRoot, VkRenderPass &renderPass, VkExtent2D &swapChainExtent);
	Triangle(Triangle &&) = default;
	Triangle(const Triangle &) = delete;
	Triangle &operator=(Triangle &&) = delete;
	Triangle &operator=(const Triangle &) = delete;
	~Triangle();

	void draw(VkCommandBuffer &commandBuffer, const vec3 &position = vec3(0.0f, 0.0f, 0.0f), const quat &rotation = quat(), const vec3 &scale = vec3(1.0f, 1.0f, 1.0f), const vec3 &color = vec3(1.0f, 1.0f, 1.0f)) override;
	void setup() override;

  private:

	VkVertexInputBindingDescription bindingDescription;
	std::vector<VkVertexInputAttributeDescription> attributeDescriptions;

	VkBuffer vertexBuffer;
	VkBufferCreateInfo bufferInfo{};
	VkMemoryRequirements memRequirements;
	VkMemoryAllocateInfo allocInfo{};
	VkDeviceMemory vertexBufferMemory;
	void *data;

	void createVertexBuffer();
};
