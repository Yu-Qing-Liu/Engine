#include "scrollbar.hpp"

#include "events.hpp"
#include "mouse.hpp"

#include <algorithm>
#include <cmath>

using glm::vec2;
using glm::vec3;
using glm::vec4;

namespace {
inline float clamp01(float v) { return std::max(0.0f, std::min(1.0f, v)); }
} // namespace

Scrollbar::Scrollbar(Scene *scene, Model *parent, vector<Model *> children) : parent(parent), children(children), Widget(scene) {}

Scrollbar::~Scrollbar() {
	if (!mouseClickId_.empty()) {
		Events::unregisterMouseClick(mouseClickId_);
		mouseClickId_.clear();
	}
	if (!cursorMoveId_.empty()) {
		Events::unregisterCursor(cursorMoveId_);
		cursorMoveId_.clear();
	}
	if (!scrollId_.empty()) {
		Events::unregisterScroll(scrollId_);
		scrollId_.clear();
	}
}

void Scrollbar::setRange(float contentHeightPx, float viewHeightPx) {
	contentHeightPx_ = std::max(1.0f, contentHeightPx);
	viewHeightPx_ = std::max(1.0f, viewHeightPx);
	updateGeometry();
	updateViewFromValue();
}

void Scrollbar::setValue(float t) {
	const float clamped = clamp01(t);
	if (std::abs(clamped - value_) < 1e-5f)
		return;

	value_ = clamped;

	updateThumbFromValue();
	updateViewFromValue();
}

void Scrollbar::applyScrollDelta(float deltaNorm) {
	if (deltaNorm == 0.0f)
		return;
	setValue(value_ + deltaNorm);
}

void Scrollbar::updateViewFromValue() {
	if (!parent)
		return;

	const float maxOffset = std::max(0.0f, contentHeightPx_ - viewHeightPx_);
	const float offset = value_ * maxOffset;

	glm::mat4 scrolledView = glm::translate(baseView_, glm::vec3(0.0f, -offset, 0.0f));
	parent->setView(scrolledView);
	for (const auto &p : children) {
		p->setView(scrolledView);
	}
}

void Scrollbar::updateThumbFromValue() {
	if (!geometry)
		return;

	const float topMargin = params.margins.x;
	const float rightMargin = params.margins.y;
	const float bottomMargin = params.margins.z;

	const float trackWidthPx = params.scrollbarWidth;

	// Track geometry in local space
	const float trackLeftLocal = parent->getViewport().width - rightMargin - trackWidthPx;
	const float trackRightLocal = trackLeftLocal + trackWidthPx;
	const float trackTopLocal = topMargin;
	const float trackBottomLocal = parent->getViewport().height - bottomMargin;

	const float trackCenterXLocal = 0.5f * (trackLeftLocal + trackRightLocal);
	const float trackHeight = trackBottomLocal - trackTopLocal;

	// Thumb size: proportional to view/content, clamped
	float thumbRatio = 1.0f;
	if (contentHeightPx_ > viewHeightPx_)
		thumbRatio = viewHeightPx_ / contentHeightPx_;

	const float minThumbPx = 20.0f;
	float thumbHeight = std::max(minThumbPx, trackHeight * thumbRatio);
	thumbHeight = std::min(thumbHeight, trackHeight);

	const float movableRange = std::max(0.0f, trackHeight - thumbHeight);

	// value_ = 0 → top, 1 → bottom, in local coords
	const float topOffsetLocal = value_ * movableRange;
	const float thumbTopLocal = trackTopLocal + topOffsetLocal;
	const float thumbBottomLocal = thumbTopLocal + thumbHeight;
	const float thumbCenterYLocal = 0.5f * (thumbTopLocal + thumbBottomLocal);

	// Screen-space rect for hit-testing
	const float thumbX0px = parent->getViewport().x + trackLeftLocal;
	const float thumbX1px = parent->getViewport().x + trackRightLocal;
	const float thumbY0px = parent->getViewport().y + thumbTopLocal;
	const float thumbY1px = parent->getViewport().y + thumbBottomLocal;

	thumbRectPx_ = vec4(thumbX0px, thumbY0px, thumbX1px, thumbY1px);

	// Thumb instance in local coords
	Rectangle::InstanceData thumb{};
	thumb.model = glm::mat4(1.0f);
	thumb.model = glm::translate(thumb.model, vec3(trackCenterXLocal, thumbCenterYLocal, 0.0f));
	thumb.model = glm::scale(thumb.model, vec3(trackWidthPx, thumbHeight, 1.0f));
	thumb.color = params.thumbColor;
	thumb.outlineColor = params.thumbColor;
	thumb.outlineWidth = 0.0f;
	thumb.borderRadius = params.thumbBorderRadiues;

	geometry->upsertInstance(1, thumb);
}

void Scrollbar::updateGeometry() {
	if (!geometry)
		return;

	const float topMargin = params.margins.x;
	const float rightMargin = params.margins.y;
	const float bottomMargin = params.margins.z;

	const float trackWidthPx = params.scrollbarWidth;

	// Local coordinate space (0..parent->getViewport().width/height)
	const float trackLeftLocal = parent->getViewport().width - rightMargin - trackWidthPx;
	const float trackRightLocal = trackLeftLocal + trackWidthPx;
	const float trackTopLocal = topMargin;
	const float trackBottomLocal = parent->getViewport().height - bottomMargin;

	const float trackCenterXLocal = 0.5f * (trackLeftLocal + trackRightLocal);
	const float trackCenterYLocal = 0.5f * (trackTopLocal + trackBottomLocal);
	const float trackHeightPx = trackBottomLocal - trackTopLocal;

	// Screen-space rect for hit-testing (add parent->getViewport().width/y)
	const float x0px = parent->getViewport().x + trackLeftLocal;
	const float x1px = parent->getViewport().x + trackRightLocal;
	const float y0px = parent->getViewport().y + trackTopLocal;
	const float y1px = parent->getViewport().y + trackBottomLocal;

	trackRectPx_ = vec4(x0px, y0px, x1px, y1px);

	// Instance #0: track (local coords)
	Rectangle::InstanceData track{};
	track.model = glm::mat4(1.0f);
	track.model = glm::translate(track.model, vec3(trackCenterXLocal, trackCenterYLocal, 0.0f));
	track.model = glm::scale(track.model, vec3(trackWidthPx, trackHeightPx, 1.0f));
	track.color = params.trackColor;
	track.outlineColor = params.trackColor;
	track.outlineWidth = 0.0f;
	track.borderRadius = params.trackBorderRadius;
	geometry->upsertInstance(0, track);

	// Instance #1: thumb (positioned based on current value_)
	updateThumbFromValue();
}

void Scrollbar::init(const std::string &name) {
	// Geometry setup
	geometry = std::make_unique<Rectangle>(scene);
	geometry->setMaxInstances(2);
	geometry->init();
	geometry->enableRayPicking();

	const auto &vp = parent->getVP();
	const auto &vpv = parent->getViewport();

	baseView_ = vp.view; // remember original view (no scroll)

	geometry->setView(vp.view);
	geometry->setProj(vp.proj);
	geometry->setViewport(vpv.width, vpv.height, vpv.x, vpv.y);

	geometry->onScreenResize = [this](Model *m, float, float, float, float) {
		const auto &vp = parent->getVP();
		const auto &vpv = parent->getViewport();

		geometry->setView(vp.view);
		geometry->setProj(vp.proj);
		geometry->setViewport(vpv.width, vpv.height, vpv.x, vpv.y);
		updateGeometry();
	};

	models[name] = geometry.get();
	updateGeometry();

	// =============== Input wiring ===============

	// 1) Mouse click: start/stop dragging, or page up/down
	mouseClickId_ = Events::registerMouseClick([this](int button, int action, int /*mods*/) {
		if (button != Events::MOUSE_BUTTON_LEFT)
			return;

		float mx = 0.0f, my = 0.0f;
		Mouse::getPixel(mx, my);

		if (action == Events::ACTION_PRESS) {
			if (!geometry || !geometry->picking)
				return;

			const auto &hit = geometry->picking->hitInfo;
			if (!hit.hit)
				return;

			// Thumb clicked → begin drag
			if (hit.primId == 1) {
				dragging_ = true;

				const float thumbCenterY = 0.5f * (thumbRectPx_.y + thumbRectPx_.w);
				dragOffsetFromThumbCenterPx_ = my - thumbCenterY;
				return;
			}

		} else if (action == Events::ACTION_RELEASE) {
			dragging_ = false;
		}
	});

	// 2) Mouse move: update drag
	cursorMoveId_ = Events::registerCursor([this](GLFWwindow* win, double mx, double my) {
		if (!dragging_)
			return;

		const float trackTop = trackRectPx_.y;
		const float trackBottom = trackRectPx_.w;
		const float trackHeight = trackBottom - trackTop;
		const float thumbHeight = thumbRectPx_.w - thumbRectPx_.y;

		if (trackHeight <= 0.0f || thumbHeight <= 0.0f)
			return;

		const float movable = std::max(0.0f, trackHeight - thumbHeight);
		if (movable <= 0.0f) {
			setValue(0.0f);
			return;
		}

		const float desiredCenterY = static_cast<float>(my) - dragOffsetFromThumbCenterPx_;

		const float minC = trackTop + thumbHeight * 0.5f;
		const float maxC = trackBottom - thumbHeight * 0.5f;
		const float clampedC = std::max(minC, std::min(maxC, desiredCenterY));

		const float newOffset = (clampedC - (trackTop + thumbHeight * 0.5f));
		const float newValue = newOffset / movable;

		setValue(newValue);
	});

	// 3) Mouse wheel scroll → move value_
	scrollId_ = Events::registerScroll([this](double /*xoff*/, double yoff) {
		// Convention: positive yoff usually means "scroll up"
		const float scrollSpeed = 0.08f; // tweak to taste
		applyScrollDelta(-static_cast<float>(yoff) * scrollSpeed);
	});
}
