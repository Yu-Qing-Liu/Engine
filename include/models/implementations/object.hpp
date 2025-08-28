#pragma once

#include "model.hpp"
#include <vulkan/vulkan_core.h>

class Object : public Model {
  public:
	Object(const std::string &model_path);
	Object(Object &&) = default;
	Object(const Object &) = delete;
	Object &operator=(Object &&) = delete;
	Object &operator=(const Object &) = delete;
	~Object() = default;
};
