#pragma once

#include "instancedpolygon.hpp"
#include "scene.hpp"

class Menu : public Scene {
  public:
	Menu(Scenes &scenes);
	Menu(Menu &&) = delete;
	Menu(const Menu &) = delete;
	Menu &operator=(Menu &&) = delete;
	Menu &operator=(const Menu &) = delete;
	~Menu() = default;

	std::string getName() override { return "Menu"; }

	void updateScreenParams() override;

	void updateComputeUniformBuffers() override;
	void computePass() override;

	void updateUniformBuffers() override;
	void renderPass() override;
	void renderPass1() override;
	void swapChainUpdate() override;

  private:
	unique_ptr<InstancedPolygon> days;
};
;
