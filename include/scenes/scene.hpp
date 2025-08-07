#pragma once

#include "models.hpp"
#include <vulkan/vulkan_core.h>

class Scene {
  public:
	Scene();
	Scene(Scene &&) = default;
	Scene(const Scene &) = delete;
	Scene &operator=(Scene &&) = delete;
	Scene &operator=(const Scene &) = delete;
	virtual ~Scene();

	virtual void render();

  protected:
	std::unique_ptr<Models> models;
};
