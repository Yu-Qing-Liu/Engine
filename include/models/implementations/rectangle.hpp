#pragma once

#include "model.hpp"
#include <vulkan/vulkan_core.h>

class Rectangle : public Model {
  public:
	Rectangle();
	Rectangle(Rectangle &&) = default;
	Rectangle(const Rectangle &) = delete;
	Rectangle &operator=(Rectangle &&) = delete;
	Rectangle &operator=(const Rectangle &) = delete;
	~Rectangle() = default;

	void updateUniformBuffer() override;
};
