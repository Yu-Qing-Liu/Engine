#pragma once

#include <atomic>
#include <glm/glm.hpp>

namespace Mouse {

inline std::atomic<float> x;
inline std::atomic<float> y;

inline void set(float nx, float ny) {
	x.store(nx, std::memory_order_relaxed);
	y.store(ny, std::memory_order_relaxed);
}

inline void getPixel(float &ox, float &oy) {
	ox = x.load(std::memory_order_relaxed);
	oy = y.load(std::memory_order_relaxed);
}

// Full-swapchain NDC
inline glm::vec2 toNDC(float mx, float my, int winW, int winH, int sw, int sh) {
	// Map window coords -> swapchain pixel coords.
	// Compute content scale between window and swapchain.
	// (Avoid using framebuffer size here—use the actual swapchain extent!)
	float sx = (winW > 0) ? (float)sw / (float)winW : 1.0f;
	float sy = (winH > 0) ? (float)sh / (float)winH : 1.0f;

	float px = mx * sx; // swapchain pixels
	float py = my * sy;

	// Normalize to [0,1] in swapchain space
	float u = (sw > 0) ? px / (float)sw : 0.0f;
	float v = (sh > 0) ? py / (float)sh : 0.0f;

	// Map to NDC [-1,1], +Y up (projection has Y-flip)
	float x = u * 2.0f - 1.0f;
	float y = 1.0f - v * 2.0f;
	return {x, y};
}

// (vx,vy,vw,vh) are the model's viewport in swapchain pixels.
// `inside` (optional) is set true if the mouse lies within the viewport rect.
inline glm::vec2 toNDC(float mx, float my, int winW, int winH, int sw, int sh, float vx, float vy, float vw, float vh, bool *inside = nullptr) {
	// window -> swapchain pixels (handle DPI/content scale)
	const float sx = (winW > 0) ? float(sw) / float(winW) : 1.0f;
	const float sy = (winH > 0) ? float(sh) / float(winH) : 1.0f;
	float px = mx * sx;
	float py = my * sy;

	// inside test in swapchain space
	const bool in = (px >= vx && px <= vx + vw && py >= vy && py <= vy + vh);
	if (inside)
		*inside = in;

	// Clamp to viewport to avoid NaNs if used when out-of-bounds.
	px = std::min(std::max(px, vx), vx + vw);
	py = std::min(std::max(py, vy), vy + vh);

	// viewport-local [0..1]
	const float u = (vw > 0.f) ? (px - vx) / vw : 0.0f;
	const float v = (vh > 0.f) ? (py - vy) / vh : 0.0f;

	// Vulkan NDC for this viewport (Y up)
	return {u * 2.0f - 1.0f, 1.0f - v * 2.0f};
}

} // namespace Mouse
