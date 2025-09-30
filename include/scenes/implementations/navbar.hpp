#pragma once

#include "rectangle.hpp"
#include "scene.hpp"
#include "texture.hpp"

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
	void renderPass1() override;
	void swapChainUpdate() override;

  private:
	unique_ptr<Rectangle> inventoryBtn;
	unique_ptr<Texture> inventoryBtnIcon;

	unique_ptr<Rectangle> recipesBtn;
	unique_ptr<Texture> recipesBtnIcon;

	unique_ptr<Rectangle> calendarBtn;
	unique_ptr<Texture> calendarBtnIcon;

	unique_ptr<Rectangle> cartBtn;
	unique_ptr<Texture> cartBtnIcon;
};
