#include "camera.hpp"
#include "events.hpp"
#include "mouse.hpp"

#include <GLFW/glfw3.h>
#include <functional>
#include <iostream>
#include <memory>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace glm;
using std::vector;

namespace UserInput {

using unregisterCb = std::function<void()>;

// ----------------------------------------------------
// First-person camera: WASD + mouse look
// ----------------------------------------------------
inline unregisterCb cameraAWSD(mat4 &view, std::function<bool()> condition = []() { return true; }) {
	struct State {
		glm::vec3 position{0.0f};
		float yaw = 0.0f;
		float pitch = 0.0f;

		bool firstMouse = true;

		GLFWwindow *window = nullptr;

		// Key states
		bool keyW = false;
		bool keyA = false;
		bool keyS = false;
		bool keyD = false;
		bool keySpace = false;
		bool keyShift = false;
	};
	auto state = std::make_shared<State>();

	auto recalcView = [state, &view, condition]() {
		if (!condition()) {
			return;
		}

		if (state->position == vec3(0.0f)) {
			// Initialize from existing view matrix
			{
				glm::mat4 invView = glm::inverse(view);
				state->position = glm::vec3(invView[3]);

				// camera forward direction (camera -> forward)
				glm::vec3 dir = -glm::vec3(invView[2]);
				if (!glm::any(glm::isnan(dir))) {
					dir = glm::normalize(dir);
					state->pitch = std::asin(glm::clamp(dir.z, -1.0f, 1.0f));
					state->yaw = std::atan2(dir.y, dir.x);
				}
			}
		}

		const float pitchLimit = glm::radians(89.0f);
		state->pitch = glm::clamp(state->pitch, -pitchLimit, pitchLimit);

		glm::vec3 dir;
		dir.x = cos(state->pitch) * cos(state->yaw);
		dir.y = cos(state->pitch) * sin(state->yaw);
		dir.z = sin(state->pitch);
		dir = glm::normalize(dir);

		const glm::vec3 up(0.0f, 0.0f, 1.0f);
		view = glm::lookAt(state->position, state->position + dir, up);
	};

	// Mouse look (infinite by recentering cursor every frame)
	auto cursorEvent = Events::registerCursor([state, recalcView, condition](GLFWwindow *win, float mx, float my) {
		if (!condition()) {
			return;
		}

		if (!state->window)
			state->window = win;

		// Hide/disable cursor (optional but nice for FPS-style)
		auto mode = glfwGetInputMode(win, GLFW_CURSOR);
		if (mode == GLFW_CURSOR_NORMAL) {
			state->firstMouse = true;
			state->position = vec3(0.f);
			state->yaw = 0.0f;
			state->pitch = 0.0f;
			glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		}

		int w, h;
		glfwGetWindowSize(win, &w, &h);
		const double centerX = w * 0.5;
		const double centerY = h * 0.5;

		if (state->firstMouse) {
			glfwSetCursorPos(win, centerX, centerY);
			state->firstMouse = false;
			return;
		}

		// Relative movement from window center
		float dx = mx - static_cast<float>(centerX);
		float dy = my - static_cast<float>(centerY);

		const float sensitivity = 0.0025f;
		state->yaw -= dx * sensitivity;
		state->pitch -= dy * sensitivity;

		// Recenter so we never hit window borders
		glfwSetCursorPos(win, centerX, centerY);

		recalcView();
	});

	// Track key states: no movement here, just booleans
	auto keyEvent = Events::registerKeyPress([state, condition](int key, int, int action, int) {
		if (!condition()) {
			return;
		}

		bool isDown = (action != Events::ACTION_RELEASE);

		switch (key) {
		case GLFW_KEY_W:
			state->keyW = isDown;
			break;
		case GLFW_KEY_S:
			state->keyS = isDown;
			break;
		case GLFW_KEY_A:
			state->keyA = isDown;
			break;
		case GLFW_KEY_D:
			state->keyD = isDown;
			break;
		case GLFW_KEY_SPACE:
			state->keySpace = isDown;
			break;
		case GLFW_KEY_LEFT_SHIFT:
		case GLFW_KEY_RIGHT_SHIFT:
			state->keyShift = isDown;
			break;
		default:
			break;
		}
	});

	// Per-frame movement: no key repeat delay
	auto updateEvent = Events::registerUpdate([state, recalcView, condition](float dt) {
		if (!condition()) {
			return;
		}

		if (!(state->keyW || state->keyA || state->keyS || state->keyD || state->keySpace || state->keyShift))
			return;

		// Reconstruct direction vectors from yaw/pitch
		glm::vec3 forward;
		forward.x = cos(state->pitch) * cos(state->yaw);
		forward.y = cos(state->pitch) * sin(state->yaw);
		forward.z = sin(state->pitch);
		forward = glm::normalize(forward);

		const glm::vec3 up(0.0f, 0.0f, 1.0f);
		glm::vec3 right = glm::normalize(glm::cross(forward, up));

		const float moveSpeed = 5.0f; // units per second
		float step = moveSpeed * dt / 1000.f;

		bool moved = false;

		if (state->keyW) {
			state->position += forward * step;
			moved = true;
		}
		if (state->keyS) {
			state->position -= forward * step;
			moved = true;
		}
		if (state->keyA) {
			state->position -= right * step;
			moved = true;
		}
		if (state->keyD) {
			state->position += right * step;
			moved = true;
		}
		if (state->keySpace) {
			state->position += up * step;
			moved = true;
		}
		if (state->keyShift) {
			state->position -= up * step;
			moved = true;
		}

		if (moved) {
			recalcView();
		}
	});

	return [cursorEvent, keyEvent, updateEvent, state]() {
		Events::unregisterCursor(cursorEvent);
		Events::unregisterKeyPress(keyEvent);
		Events::unregisterUpdate(updateEvent);

		// Restore cursor state if we disabled it
		glfwSetInputMode(state->window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	};
}

// ----------------------------------------------------
// 2D map camera: pan + zoom
// ----------------------------------------------------
inline unregisterCb camera2D(mat4 &view, const mat4 &proj, const float &vpx, const float &vpy, const float &vpw, const float &vph, const float &fbw, const float &fbh, std::function<bool()> condition) {

	struct State {
		// Camera basis & position
		glm::vec3 camPos{0.0f, 0.0f, 0.0f};
		glm::vec3 forward{0.0f, 0.0f, -1.0f};
		glm::vec3 up{0.0f, 0.0f, 1.0f};
		glm::vec3 right{1.0f, 0.0f, 0.0f};

		// Plane we are "looking at"
		glm::vec3 planeNormal{0.0f, 0.0f, 1.0f};
		glm::vec3 planePoint{0.0f, 0.0f, 0.0f};

		bool hasBaseView = false;

		bool dragging = false;
		bool firstDragFrame = true;
		float lastX = 0.0f;
		float lastY = 0.0f;

		GLFWwindow *window = nullptr;
	};
	auto state = std::make_shared<State>();

	auto recalcView = [state, &view]() {
		if (!state->hasBaseView) {
			state->hasBaseView = true;

			glm::mat4 invView = glm::inverse(view);
			state->camPos = glm::vec3(invView[3]);

			glm::vec3 right = glm::vec3(invView[0]);
			glm::vec3 up = glm::vec3(invView[1]);
			glm::vec3 fwd = -glm::vec3(invView[2]); // camera forward

			if (!glm::any(glm::isnan(right)))
				right = glm::normalize(right);
			if (!glm::any(glm::isnan(up)))
				up = glm::normalize(up);
			if (!glm::any(glm::isnan(fwd)))
				fwd = glm::normalize(fwd);

			state->right = right;
			state->up = up;
			state->forward = fwd;

			// Define "map plane" as the plane in front of the initial camera
			state->planeNormal = fwd;
			// Arbitrary distance in front; adjust if you like
			const float initialDist = 10.0f;
			state->planePoint = state->camPos + fwd * initialDist;
		}

		glm::vec3 camPos = state->camPos;
		glm::vec3 target = camPos + state->forward;
		view = glm::lookAt(camPos, target, state->up);
	};

	recalcView();

	// ----------- WASD: move in plane (signs chosen so it "feels right") -----------
	auto kbEvent = Events::registerKeyPress([state, recalcView, condition](int key, int, int action, int mods) {
		if (!condition()) {
			state->hasBaseView = false;
			return;
		}

		if (action != Events::ACTION_PRESS && action != Events::ACTION_REPEAT)
			return;

		float step = 0.5f;
		if (mods & Events::MOD_SHIFT_KEY)
			step *= 4.0f;

		// W: up, S: down, A: left, D: right (in screen space)
		// These signs match the earlier version you said felt correct.
		switch (key) {
		case GLFW_KEY_W:
			state->camPos -= state->up * step;
			break;
		case GLFW_KEY_S:
			state->camPos += state->up * step;
			break;
		case GLFW_KEY_A:
			state->camPos += state->right * step;
			break;
		case GLFW_KEY_D:
			state->camPos -= state->right * step;
			break;
		default:
			break;
		}

		recalcView();
	});

	// ----------- Mouse move: drag → move in plane -----------
	auto cursorEvent = Events::registerCursor([state, recalcView, condition, &vpx, &vpy, &vpw, &vph, &fbw, &fbh](GLFWwindow *win, float mx, float my) {
		if (!condition()) {
			state->dragging = false;
			state->hasBaseView = false;
			return;
		}

		if (!state->window)
			state->window = win;

		int winW = 0, winH = 0;
		glfwGetWindowSize(win, &winW, &winH);
		if (winW <= 0 || winH <= 0 || fbw <= 0.0f || fbh <= 0.0f)
			return;

		const int sw = static_cast<int>(fbw);
		const int sh = static_cast<int>(fbh);

		bool inside = false;
		glm::vec2 ndc = Mouse::toNDC(mx, my, winW, winH, sw, sh, vpx, vpy, vpw, vph, &inside);
		(void)ndc; // kept for debugging; not needed for drag math here

		if (!state->dragging) {
			state->lastX = mx;
			state->lastY = my;
			return;
		}

		if (state->firstDragFrame) {
			state->lastX = mx;
			state->lastY = my;
			state->firstDragFrame = false;
			return;
		}

		float dx = mx - state->lastX;
		float dy = my - state->lastY;
		state->lastX = mx;
		state->lastY = my;

		// Simple heuristic: movement scales with distance to plane
		float distToPlane = glm::dot(state->planeNormal, state->planePoint - state->camPos);
		distToPlane = (glm::max)(distToPlane, 0.1f);
		const float pixelsToWorld = distToPlane * 0.001f; // tweak as needed

		// Signs match WASD: drag right → world moves right, i.e. camera moves left
		state->camPos -= dx * pixelsToWorld * state->right;
		state->camPos += dy * pixelsToWorld * state->up;

		recalcView();
	});

	// ----------- Mouse button: start/stop drag -----------
	auto mouseEvent = Events::registerMouseClick([state, condition](int button, int action, int) {
		if (!condition()) {
			state->dragging = false;
			state->hasBaseView = false;
			return;
		}

		if (button == GLFW_MOUSE_BUTTON_LEFT) {
			if (action == Events::ACTION_PRESS) {
				state->dragging = true;
				state->firstDragFrame = true;
			} else if (action == Events::ACTION_RELEASE) {
				state->dragging = false;
			}
		}
	});

	// ----------- Scroll: dolly along plane normal, but keep cursor point fixed -----------
	auto scrollEvent = Events::registerScroll([state, recalcView, condition, &vpx, &vpy, &vpw, &vph, &fbw, &fbh, &view, &proj](double /*xoff*/, double yoff) {
		if (!condition()) {
			state->hasBaseView = false;
			return;
		}
		if (yoff == 0.0)
			return;

		if (fbw <= 0.0f || fbh <= 0.0f || vpw <= 0.0f || vph <= 0.0f)
			return;
		if (!state->window)
			return;

		int winW = 0, winH = 0;
		glfwGetWindowSize(state->window, &winW, &winH);
		if (winW <= 0 || winH <= 0)
			return;

		// Cursor pos → NDC inside this viewport (trusted helper)
		double mxD = 0.0, myD = 0.0;
		glfwGetCursorPos(state->window, &mxD, &myD);
		float mx = static_cast<float>(mxD);
		float my = static_cast<float>(myD);

		const int sw = static_cast<int>(fbw);
		const int sh = static_cast<int>(fbh);

		bool inside = false;
		glm::vec2 ndc = Mouse::toNDC(mx, my, winW, winH, sw, sh, vpx, vpy, vpw, vph, &inside);
		if (!inside)
			return;

		// -------- distance to plane BEFORE zoom --------
		glm::vec3 camPos = state->camPos; // authoritative camera position
		glm::vec3 n = glm::normalize(state->planeNormal);
		glm::vec3 p0 = state->planePoint;

		float distOld = glm::dot(n, p0 - camPos); // signed distance along plane normal
		if (distOld <= 0.0f)
			return;

		// -------- visible half-width/half-height on plane BEFORE zoom --------
		// Perspective matrix (OpenGL-style): proj[1][1] = 1 / tan(fovy/2)
		float tanHalfFov = 1.0f / proj[1][1];
		float aspect = proj[1][1] / proj[0][0];

		float halfH_b = distOld * tanHalfFov; // visible half-height on plane
		float halfW_b = halfH_b * aspect;	  // visible half-width on plane

		// -------- apply zoom (like your example, but on distance) -------
		const float zoomSpeed = 0.05f; // tweak to taste
		float zoomFactor = std::exp(static_cast<float>(yoff) * zoomSpeed);
		// yoff > 0 -> zoom in -> distance shrinks
		float distNew = distOld / zoomFactor;
		distNew = (glm::max)(distNew, 0.1f);

		// visible size scales with distance in perspective:
		// newHalf = half_b * (distNew / distOld)
		float zoom_ratio = distNew / distOld; // analogous to your oldZoom/newZoom
		float oneMinus = 1.0f - zoom_ratio;

		// -------- shift camera sideways so cursor world point stays pinned --------
		// ndc.x, ndc.y in [-1,1], right/up are camera basis vectors
		glm::vec3 lateral = ndc.x * halfW_b * oneMinus * state->right + ndc.y * halfH_b * oneMinus * -state->up;

		// -------- move camera along plane normal to new distance --------
		// dot(n, p0 - (camPos + k*n)) = distNew  => k = distOld - distNew
		glm::vec3 normalMove = (distOld - distNew) * n;

		state->camPos = camPos + normalMove + lateral;

		// Rebuild view from updated camPos / basis
		recalcView();
	});

	return [kbEvent, cursorEvent, mouseEvent, scrollEvent]() {
		Events::unregisterKeyPress(kbEvent);
		Events::unregisterCursor(cursorEvent);
		Events::unregisterMouseClick(mouseEvent);
		Events::unregisterScroll(scrollEvent);
	};
}

// ----------------------------------------------------
// 3D orbit camera: click-drag rotate + scroll zoom
// ----------------------------------------------------
inline unregisterCb camera3D(mat4 &view, const vec3 &target, std::function<bool()> condition = [] { return true; }) {
	/*
	 * 3D Map camera
	 * Mouse click-drag -> rotate cam
	 * Mouse scroll -> zoom
	 */

	struct State {
		float distance = 0.0f;
		float yaw = 0.0f;
		float pitch = glm::radians(30.0f);

		bool rotating = false;
		bool firstDragFrame = true;
		float lastX = 0.0f;
		float lastY = 0.0f;
	};
	auto state = std::make_shared<State>();

	auto recalcView = [state, &view, &target, condition]() {
		if (!condition()) {
			return;
		}

		if (state->distance == 0.0f) {
			// Try to infer initial orbit from existing view
			{
				mat4 invView = glm::inverse(view);
				vec3 camPos = vec3(invView[3]);

				state->distance = glm::length(camPos - target);
				if (state->distance < 0.1f)
					state->distance = 0.1f;

				vec3 dir = glm::normalize(target - camPos); // camera -> target
				if (!glm::any(glm::isnan(dir))) {
					state->pitch = std::asin(glm::clamp(dir.z, -1.0f, 1.0f));
					state->yaw = std::atan2(dir.y, dir.x);
				}
			}
		}

		const float pitchLimit = glm::radians(89.0f);
		state->pitch = glm::clamp(state->pitch, -pitchLimit, pitchLimit);
		state->distance = (glm::max)(state->distance, 0.1f);

		vec3 dir;
		dir.x = cos(state->pitch) * cos(state->yaw);
		dir.y = cos(state->pitch) * sin(state->yaw);
		dir.z = sin(state->pitch);
		dir = glm::normalize(dir);

		vec3 camPos = target - dir * state->distance;
		const vec3 up(0.0f, 0.0f, 1.0f);

		view = glm::lookAt(camPos, target, up);
	};

	auto cursorEvent = Events::registerCursor([state, recalcView, condition](GLFWwindow *win, float mx, float my) {
		if (!condition()) {
			return;
		}

		if (!state->rotating) {
			state->lastX = mx;
			state->lastY = my;
			return;
		}

		if (state->firstDragFrame) {
			state->lastX = mx;
			state->lastY = my;
			state->firstDragFrame = false;
			return;
		}

		float dx = mx - state->lastX;
		float dy = my - state->lastY;
		state->lastX = mx;
		state->lastY = my;

		const float rotSpeed = 0.005f;
		state->yaw -= dx * rotSpeed;
		state->pitch -= dy * rotSpeed;

		recalcView();
	});

	auto mouseEvent = Events::registerMouseClick([state, condition](int button, int action, int) {
		if (!condition()) {
			return;
		}

		if (button == GLFW_MOUSE_BUTTON_LEFT) {
			if (action == Events::ACTION_PRESS) {
				state->rotating = true;
				state->firstDragFrame = true;
			} else if (action == Events::ACTION_RELEASE) {
				state->rotating = false;
			}
		}
	});

	auto scrollEvent = Events::registerScroll([state, recalcView, condition](double, double yoff) {
		if (!condition()) {
			return;
		}

		if (yoff == 0.0)
			return;

		const float zoomFactor = 1.1f;
		if (yoff > 0.0)
			state->distance /= zoomFactor; // zoom in
		else
			state->distance *= zoomFactor; // zoom out

		recalcView();
	});

	return [cursorEvent, mouseEvent, scrollEvent]() {
		Events::unregisterCursor(cursorEvent);
		Events::unregisterMouseClick(mouseEvent);
		Events::unregisterScroll(scrollEvent);
	};
}

inline unregisterCb camera2DZoom(mat4 &view, const vec3 &target, std::function<bool()> condition = [] { return true; }) {
	// Scroll: dolly in/out along the line from camera to target
	auto scrollEvent = Events::registerScroll([&view, &target, condition](double /*xoff*/, double yoff) {
		if (!condition())
			return;
		if (yoff == 0.0)
			return;

		// Reconstruct camera position and up vector from current view
		glm::mat4 invView = glm::inverse(view);
		glm::vec3 camPos = glm::vec3(invView[3]); // camera world position
		glm::vec3 up = glm::vec3(invView[1]);	  // camera "up" in world

		if (!glm::any(glm::isnan(up)))
			up = glm::normalize(up);
		else
			up = glm::vec3(0.0f, 1.0f, 0.0f);

		// Direction from camera to target
		glm::vec3 toTarget = target - camPos;
		float dist = glm::length(toTarget);
		if (dist <= 0.0f)
			return;

		glm::vec3 dir = toTarget / dist; // normalized

		// Zoom by changing distance only
		const float zoomFactor = 1.1f;
		if (yoff > 0.0)
			dist /= zoomFactor; // zoom in
		else
			dist *= zoomFactor; // zoom out

		dist = (glm::max)(dist, 0.1f); // clamp min distance

		// New camera position, still looking at the same target
		camPos = target - dir * dist;
		view = glm::lookAt(camPos, target, up);
	});

	return [scrollEvent]() { Events::unregisterScroll(scrollEvent); };
}

} // namespace UserInput
