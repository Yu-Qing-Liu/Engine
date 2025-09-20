#include "scene.hpp"
#include "scenes.hpp"

bool Scene::mouseMode = true;

Scene::Scene(Scenes &scenes) : scenes(scenes) {
	screenParams.viewport.x = 0.0f;
	screenParams.viewport.y = 0.0f;
	screenParams.viewport.width = (float)Engine::swapChainExtent.width;
	screenParams.viewport.height = (float)Engine::swapChainExtent.height;
	screenParams.viewport.minDepth = 0.0f;
	screenParams.viewport.maxDepth = 1.0f;
	screenParams.scissor.offset = {0, 0};
	screenParams.scissor.extent = Engine::swapChainExtent;
    
	auto kbState = [this](int key, int, int action, int) {
		if (key >= 0 && key <= GLFW_KEY_LAST) {
			if (action == GLFW_PRESS)
				keyDown[key] = true;
			if (action == GLFW_RELEASE)
				keyDown[key] = false;
		}
	};
	Events::keyboardCallbacks.push_back(kbState);
}

void Scene::disableMouseMode() {
    Scene::mouseMode = false;

	GLFWwindow *win = Engine::window;
	if (win) {
		int ww, hh;
		glfwGetWindowSize(win, &ww, &hh);
		lastPointerX = ww * 0.5;
		lastPointerY = hh * 0.5;
		glfwSetCursorPos(win, lastPointerX, lastPointerY);
	}

	// Initialize yaw/pitch from current view direction so we face the scene
	{
		glm::vec3 f0 = glm::normalize(lookAtCoords - camPos);
		// fallback if lookAtCoords==camPos
		if (!glm::all(glm::greaterThan(glm::abs(f0), glm::vec3(1e-6f))))
			f0 = glm::normalize(glm::vec3(-1, -1, -1)); // only as fallback, not always

		yaw = atan2f(f0.y, f0.x);
		pitch = asinf(glm::clamp(f0.z, -1.0f, 1.0f));
	}

	// capture cursor
	if (Engine::window) {
		glfwSetInputMode(Engine::window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		if (glfwRawMouseMotionSupported())
			glfwSetInputMode(Engine::window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
		lastPointerX = -1.0;
		lastPointerY = -1.0;
	}
}

void Scene::enableMouseMode() {
    Scene::mouseMode = true;

	GLFWwindow *win = Engine::window;
	if (Engine::window) {
		glfwSetInputMode(Engine::window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		if (glfwRawMouseMotionSupported())
			glfwSetInputMode(Engine::window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
	}
}

void Scene::firstPersonMouseControls() {
	GLFWwindow *win = Engine::window;
	if (!win)
		return;
	const bool captured = glfwGetInputMode(win, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
	if (!captured)
		return;
	if (!glfwGetWindowAttrib(win, GLFW_FOCUSED))
		return;

	// center of the window
	int ww = 0, hh = 0;
	glfwGetWindowSize(win, &ww, &hh);
	const double cx = ww * 0.5;
	const double cy = hh * 0.5;

	// read current cursor, compute delta from center
	double mx = 0.0, my = 0.0;
	glfwGetCursorPos(win, &mx, &my);

	// first frame after focus or startup: just recenter without a jump
	if (lastPointerX < 0.0 || lastPointerY < 0.0) {
		glfwSetCursorPos(win, cx, cy);
		lastPointerX = cx;
		lastPointerY = cy;
		return;
	}

	const double dx = mx - cx;
	const double dy = my - cy;

	// immediately re-center so next frame's delta is relative
	glfwSetCursorPos(win, cx, cy);
	lastPointerX = cx;
	lastPointerY = cy;

	// update yaw/pitch from mouse delta
	yaw -= float(dx) * mouseSens;	// yaw wraps naturally → 360°+
	pitch -= float(dy) * mouseSens; // invert if you prefer
	pitch = glm::clamp(pitch, glm::radians(-89.0f), glm::radians(89.0f));

	// build forward from yaw/pitch (Z-up)
	const float cyaw = cosf(yaw), syaw = sinf(yaw);
	const float cp = cosf(pitch), sp = sinf(pitch);
	glm::vec3 f = glm::normalize(glm::vec3(cyaw * cp, syaw * cp, sp));

	// keep your current look distance (fallback to 1)
	float lookDist = glm::length(lookAtCoords - camPos);
	if (!(lookDist > 1e-4f))
		lookDist = 1.0f;

	// drive the existing lookAt target
	lookAtCoords = camPos + f * lookDist;
}

void Scene::firstPersonKeyboardControls(float sensitivity) {
	GLFWwindow *win = Engine::window;
	if (!win)
		return;
	const bool captured = glfwGetInputMode(win, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
	if (!captured)
		return;
	if (!glfwGetWindowAttrib(win, GLFW_FOCUSED))
		return;

	// read keys directly from GLFW
	auto down = [&](int k) { return glfwGetKey(win, k) == GLFW_PRESS; };

	float moveX = 0.f, moveY = 0.f, moveZ = 0.f; // strafe right, forward, up
	if (down(GLFW_KEY_D) || down(GLFW_KEY_RIGHT))
		moveX += 1.f;
	if (down(GLFW_KEY_A) || down(GLFW_KEY_LEFT))
		moveX -= 1.f;
	if (down(GLFW_KEY_W) || down(GLFW_KEY_UP))
		moveY += 1.f;
	if (down(GLFW_KEY_S) || down(GLFW_KEY_DOWN))
		moveY -= 1.f;
	if (down(GLFW_KEY_E))
		moveZ += 1.f;
	if (down(GLFW_KEY_Q))
		moveZ -= 1.f;

	// speed modifiers (same as before)
	if (down(GLFW_KEY_LEFT_CONTROL) || down(GLFW_KEY_RIGHT_CONTROL))
		sensitivity *= 5.0f;
	if (down(GLFW_KEY_LEFT_ALT) || down(GLFW_KEY_RIGHT_ALT))
		sensitivity *= 0.2f;

	// ------- build camera-aligned basis from current aim (cursor) -------
	const glm::vec3 worldUp(0, 0, 1);

	// forward = where the cursor points (what you use in lookAt())
	glm::vec3 f = lookAtCoords - camPos;
	if (glm::dot(f, f) < 1e-12f)
		f = glm::vec3(0, 1, 0);
	f = glm::normalize(f);

	// right = perpendicular to forward in the horizontal sense
	glm::vec3 right = glm::cross(f, worldUp);
	if (glm::dot(right, right) < 1e-12f)
		right = glm::vec3(1, 0, 0);
	right = glm::normalize(right);

	// up for flying (keeps Q/E vertical lift)
	const glm::vec3 up = worldUp;

	// movement aligned to cursor (W/S uses full forward, including pitch)
	glm::vec3 delta = moveX * right + moveY * f + moveZ * up;

	if (glm::dot(delta, delta) > 0.0f) {
		delta = glm::normalize(delta) * (camSpeed * sensitivity * (Engine::deltaTime * 100 > 0 ? Engine::deltaTime * 100 : 1.f));
		camPos += delta;
		lookAtCoords += delta; // pan the *actual* aim point you render with
		camTarget += delta;	   // keep in sync if you still use camTarget elsewhere
	}
}

void Scene::updateRayTraceUniformBuffers() {
	for (const auto &m : models) {
		if (m->rayTracingEnabled) {
			m->updateRayTraceUniformBuffer();
		}
	}
}

void Scene::rayTraces() {
	for (auto *m : models) {
		if (m && m->rayTracingEnabled) {
			m->rayTrace();
		}
	}

	Model *closest = nullptr;
	float bestLen = std::numeric_limits<float>::infinity();

	for (auto *m : models) {
		if (!m) {
			continue;
		}
		if (!m->rayTracingEnabled) {
			continue;
		}
		if (!m->rayLength.has_value()) {
			continue;
		}
		float d = *m->rayLength;
		if (d < bestLen) {
			bestLen = d;
			closest = m;
		}
	}

	if (closest) {
		closest->setMouseIsOver(true);
		if (closest->onMouseHover) {
			closest->onMouseHover();
		}
	}
	for (auto *m : models) {
		if (closest && m == closest) {
			continue;
		}
		if (!m) {
			continue;
		}
		if (!m->rayTracingEnabled) {
			continue;
		}
		m->setMouseIsOver(false);
	}
}

void Scene::updateScreenParams() {}

void Scene::updateComputeUniformBuffers() {}
void Scene::computePass() {}

void Scene::updateUniformBuffers() {}
void Scene::renderPass() {}
void Scene::swapChainUpdate() {}
