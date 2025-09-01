#pragma once

#include <memory>
using std::unique_ptr;

class Scene {
  public:
	Scene() = default;
	Scene(Scene &&) = default;
	Scene(const Scene &) = delete;
	Scene &operator=(Scene &&) = delete;
	Scene &operator=(const Scene &) = delete;
	virtual ~Scene() = default;

	virtual void renderPass();
	virtual void swapChainUpdate();
};
