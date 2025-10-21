#pragma once

#include "scene.hpp"
#include "text.hpp"
#include "texture.hpp"
#include "grid.hpp"

using namespace std::chrono;

class Inventory : public Scene {
  public:
	Inventory(Scenes &scenes, bool show = true);
	Inventory(Inventory &&) = delete;
	Inventory(const Inventory &) = delete;
	Inventory &operator=(Inventory &&) = delete;
	Inventory &operator=(const Inventory &) = delete;
	~Inventory() = default;

	struct Item {
		unique_ptr<Text> name;
		unique_ptr<Texture> icon;
		unique_ptr<Text> quantity;
	};

	std::string getName() override { return "Inventory"; }

	void updateScreenParams() override;

	void updateComputeUniformBuffers() override;
	void computePass() override;

	void updateUniformBuffers() override;
	void renderPass() override;
	void renderPass1() override;
	void swapChainUpdate() override;

  private:
	vector<Item> items;
	unique_ptr<Grid> grid;
};
