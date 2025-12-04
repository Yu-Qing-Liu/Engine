#pragma once

#include "rectangle.hpp"
#include "widget.hpp"

#include <glm/glm.hpp>

class Scrollbar : public Widget {
  public:
	Scrollbar(Scene *scene, Model *parent, vector<Model *> children = {});
	~Scrollbar();

	// Create GPU geometry & register input callbacks
	void init(const std::string &widgetName);

	struct Params {
		float scrollbarWidth = 8.0f;  // in pixels
		float scrollbarHeight = 0.0f; // 0 → use viewport height
		float trackBorderRadius = 10.0f;
		float thumbBorderRadiues = 10.0f;
		vec4 trackColor = Colors::Gray(0.35f);
		vec4 thumbColor = Colors::Gray(0.7f);
		vec3 margins = vec3(0.0f); // top, right, bottom
	};

	// Content vs. viewport height in pixels.
	// This determines the thumb size.
	//   - contentHeightPx: total scrollable content height
	//   - viewHeightPx   : visible window height
	void setRange(float contentHeightPx, float viewHeightPx);

	// Normalized scroll position [0,1], 0 = top, 1 = bottom
	void setValue(float t);
	float getValue() const { return value_; }

	Params &getParams() { return params; }

  private:
	void updateGeometry(); // update instances & cached rectangles
	void updateViewFromValue();
	void updateThumbFromValue();

	void applyScrollDelta(float deltaNorm);

  private:
	Params params{};

	Model *parent;
	vector<Model *> children;
	glm::mat4 baseView_ = glm::mat4(1.0f); // <--- view at “no scroll”

	std::unique_ptr<Rectangle> geometry;

	// Scroll model
	float contentHeightPx_ = 1.0f;
	float viewHeightPx_ = 1.0f;
	float value_ = 0.0f; // 0..1

	// Cached rectangles in *pixel* space: (x0, y0, x1, y1)
	glm::vec4 trackRectPx_{0.0f};
	glm::vec4 thumbRectPx_{0.0f};

	// Drag state
	bool dragging_ = false;
	float dragOffsetFromThumbCenterPx_ = 0.0f;

	// Event registration IDs
	std::string mouseClickId_;
	std::string cursorMoveId_;
	std::string scrollId_;
};
