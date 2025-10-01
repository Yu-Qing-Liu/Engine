#pragma once

#include "instancedpolygon.hpp"
#include "instancedrectangle.hpp"
#include "scene.hpp"
#include "text.hpp"

#include <chrono>

using namespace std::chrono;

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
	weekday selectedDay;
	int hoveredId = -1;

	unique_ptr<InstancedPolygon> dayBtns;
	vector<unique_ptr<Text>> dayLabels;

	int numMeals = 3;
	unique_ptr<InstancedRectangle> mealBtns;
};
;
