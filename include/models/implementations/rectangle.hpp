#pragma once

#include "models/model.hpp"

class Rectangle : public Model {
  public:
	Rectangle(const std::string &shaderPath);
	Rectangle(Rectangle &&) = default;
	Rectangle(const Rectangle &) = delete;
	Rectangle &operator=(Rectangle &&) = delete;
	Rectangle &operator=(const Rectangle &) = delete;
	~Rectangle();

	void draw(const vec3 &position = vec3(0.0f, 0.0f, 0.0f), const quat &rotation = quat(), const vec3 &scale = vec3(1.0f, 1.0f, 1.0f), const vec3 &color = vec3(1.0f, 1.0f, 1.0f)) override;
	void setup() override;

  private:
	VkVertexInputBindingDescription bindingDescription;
	std::vector<VkVertexInputAttributeDescription> attributeDescriptions;

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMemory;

	void createVertexBuffer();
    void createIndexBuffer();
};
