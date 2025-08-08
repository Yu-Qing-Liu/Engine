#pragma once

#include "models/model.hpp"

class Triangle : public Model {
  public:
	Triangle(const std::string &shaderPath);
	Triangle(Triangle &&) = default;
	Triangle(const Triangle &) = delete;
	Triangle &operator=(Triangle &&) = delete;
	Triangle &operator=(const Triangle &) = delete;
	~Triangle();

	void draw(const vec3 &position = vec3(0.0f, 0.0f, 0.0f), const quat &rotation = quat(), const vec3 &scale = vec3(1.0f, 1.0f, 1.0f), const vec3 &color = vec3(1.0f, 1.0f, 1.0f)) override;
	void setup() override;

  private:
	VkVertexInputBindingDescription bindingDescription;
	std::vector<VkVertexInputAttributeDescription> attributeDescriptions;

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMemory;
	void *data;

	void createVertexBuffer();
};
