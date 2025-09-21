#pragma once

#include "scene.hpp"

class NavBar : public Scene {
  public:
	NavBar(Scenes &scenes);
	NavBar(NavBar &&) = delete;
	NavBar(const NavBar &) = delete;
	NavBar &operator=(NavBar &&) = delete;
	NavBar &operator=(const NavBar &) = delete;
	~NavBar() = default;

	std::string getName() override { return "NavBar"; }

	void updateScreenParams() override;

	void updateComputeUniformBuffers() override;
	void computePass() override;

	void updateUniformBuffers() override;
	void renderPass() override;
	void swapChainUpdate() override;

  private:
};
