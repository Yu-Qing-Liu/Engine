#pragma once

#include "instancedrectangle.hpp"
#include "widget.hpp"

class Grid : public Widget {
  public:
	Grid();
	Grid(Grid &&) = delete;
	Grid(const Grid &) = delete;
	Grid &operator=(Grid &&) = delete;
	Grid &operator=(const Grid &) = delete;
	~Grid() = default;

	struct StyleParams {
		vec2 cellSize = vec2(50.0f);
		vec4 cellColor = Colors::Gray(0.3);
		float cellBorderRadius = 25.0f;
		float gap = 12.0f;
		float scrollBarWidth = 24.0f;
		vec2 gridCenter = vec2(0.0f, 0.0f);
		vec2 gridDim = vec2(800.0f, 800.0f);
		vec4 sliderColor = Colors::Gray(0.55);
		vec4 sliderColorPressed = Colors::Gray;
		vec4 margins = vec4(0.0f);
		int numCols = -1; // dynamic;
	};

	Grid(Scene *scene, const Model::MVP &mvp, Model::ScreenParams &screenParams);

	void swapChainUpdate();
	void updateUniformBuffers();
	void render() override;

	bool enableControls = true;

	StyleParams styleParams{};
	size_t numItems = 200;

	unique_ptr<InstancedRectangle> grid;
	unique_ptr<InstancedRectangle> scrollBar;

	std::function<void(int idx, float x, float y, vec2 cellSize, Model::MVP gridMVP)> setGridItem;

	Model::ScreenParams sp;
	Model::ScreenParams bgSp;

  private:
	vec3 camPosOrtho = vec3(0.0f);
	vec3 lookAtCoords = vec3(0.0f);
	vec3 camTarget = vec3(0.0f);
	float zoom = 1.0f;

	double lastPointerX = -1.0;
	double lastPointerY = -1.0;

	float scrollMinY = 0.0f;
	float scrollMaxY = 0.0f;

	int rowsUsed = 0;
	float contentH = 0.0f;
	float gridW;
	float gridH;

	float trackX = 0.0f;
	float trackY = 0.0f;
	float trackH = 0.0f;

	bool usingSlider = false;
	bool s_hookedScroll = false;
	bool s_initY = false;
	float s_initialY = 0.0f;

	InstancedRectangleData slider{};

	void updateScreenParams();

	void createGrid();
	void createScrollBar();
	void updateSlider();

	void dragSliderToCursor();
	void applyVerticalDeltaClamped(float dy, float minY, float maxY);
	void mouseDragY(float &scrollMinY, float &scrollMaxY, bool inverted);
};
