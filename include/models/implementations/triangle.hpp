#pragma once

#include "model.hpp"
#include <vulkan/vulkan_core.h>

class Triangle : public Model {
  public:
	Triangle();
	Triangle(Triangle &&) = default;
	Triangle(const Triangle &) = delete;
	Triangle &operator=(Triangle &&) = delete;
	Triangle &operator=(const Triangle &) = delete;
	~Triangle() = default;
};
