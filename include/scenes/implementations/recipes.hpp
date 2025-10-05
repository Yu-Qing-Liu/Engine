#pragma once

#include "instancedrectangle.hpp"
#include "scene.hpp"
#include "texture.hpp"

using namespace std::chrono;

class Recipes : public Scene {
  public:
	Recipes(Scenes &scenes, bool show = true);
	Recipes(Recipes &&) = delete;
	Recipes(const Recipes &) = delete;
	Recipes &operator=(Recipes &&) = delete;
	Recipes &operator=(const Recipes &) = delete;
	~Recipes() = default;

	std::string getName() override { return "Recipes"; }

	void updateScreenParams() override;

	void updateComputeUniformBuffers() override;
	void computePass() override;

	void updateUniformBuffers() override;
	void renderPass() override;
	void renderPass1() override;
	void swapChainUpdate() override;

  private:
	unique_ptr<InstancedRectangle> grid;
	unique_ptr<InstancedRectangle> scrollBar;

	unique_ptr<Texture> addRecipeIcon;

	Model::ScreenParams spGrid{};
	float gridX;
	float gridY;
	float gridW;
	float gridH;

	float scrollMinY = 0.0f;
	float scrollMaxY = 0.0f;

	// ---- Layout constants (tweak here, used everywhere) ----
	static constexpr float kCellSize = 300.0f;
	static constexpr float kGap = 12.0f;
	static constexpr float kPadScale = 0.5f; // pad = kCellSize * kPadScale
	static constexpr float kScrollBarWidth = 24.0f;
	static constexpr float kMinThumb = 24.0f;
	static constexpr float kEps = 1e-4f;

	// ---- Derived per-frame / swapchain (shared across functions) ----
	float pitch = kCellSize + kGap;

	float padL = kCellSize * kPadScale;
	float padT = kCellSize * kPadScale;
	float padR = 0.0f;

	// Cached grid viewport size (grid-local)
	float gw = 0.0f;
	float gh = 0.0f;

	// Content metrics (for scroll range & thumb sizing)
	size_t numItems = 9; // set dynamically if needed
	int rowsUsed = 0;
	float contentH = 0.0f;

	// Scrollbar geometry cached so updateSlider() can use it
	float sbW = kScrollBarWidth;
	float btnH = kScrollBarWidth; // square buttons
	float trackX = 0.0f;
	float trackY = 0.0f;
	float trackH = 0.0f;

	bool usingSlider = false;

	InstancedRectangleData slider{};

	void createGrid();
	void createScrollBar();
	void updateSlider();
	void dragSliderToCursor();
};
