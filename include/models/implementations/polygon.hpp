#pragma once

#include "model.hpp"
#include <vulkan/vulkan_core.h>

class Polygon : public Model {
  public:
	Polygon(const std::vector<Vertex> &vertices, const std::vector<uint16_t> &indices);
	Polygon(Polygon &&) = default;
	Polygon(const Polygon &) = delete;
	Polygon &operator=(Polygon &&) = delete;
	Polygon &operator=(const Polygon &) = delete;
	~Polygon() = default;
};
