#pragma once

#include "model.hpp"
#include <memory>

using std::make_unique;
using std::unique_ptr;

class Scenes;

class Scene {
  public:
	Scene(Scenes &scenes);
	Scene(Scene &&) = delete;
	Scene(const Scene &) = delete;
	Scene &operator=(Scene &&) = delete;
	Scene &operator=(const Scene &) = delete;
	virtual ~Scene() = default;

	struct ClosestHit {
		Model *model = nullptr;
		float distance = std::numeric_limits<float>::infinity();
	};

	virtual std::string getName() = 0;

	vector<Model *> models;
	bool is3D = true;

	void updateRayTraceUniformBuffers();
	ClosestHit rayTraces();
	void applyHover(Model *globalClosest);

	virtual void updateScreenParams();

	virtual void updateComputeUniformBuffers();
	virtual void computePass();

	virtual void updateUniformBuffers();
	virtual void renderPass();
	virtual void renderPass1();
	virtual void swapChainUpdate();

  protected:
	Scenes &scenes;
	Model::ScreenParams screenParams;

	Model::MVP mvp{};

	float fovH = 0.0f;
	float fovV = 0.0f;
	float baseH = 0.0f;
	float baseW = 0.0f;

	static bool mouseMode;

	// Camera state (meters)
	glm::vec3 camPos{12.0f, 12.0f, 12.0f};
	glm::vec3 camPosOrtho{12.0f, 12.0f, 12.0f};
	glm::vec3 camTarget{0.0f, 0.0f, 0.0f};
	glm::vec3 camUp{0.0f, 0.0f, 1.0f};
	float camSpeed = 1.0f;

	// FPS mouselook state
	float yaw = 0.0f;		  // radians, wraps freely
	float pitch = 0.0f;		  // radians, clamp to ~(-89°, +89°)
	float mouseSens = 0.001f; // tweak to taste
	vec3 lookAtCoords = {0.0f, 0.0f, 0.0f};

	// mouse-aim state
	double lastPointerX = -1.0;
	double lastPointerY = -1.0;

	vec2 viewCenter = vec2{0.0f};
	float zoom = 1.0f;

	std::array<bool, GLFW_KEY_LAST + 1> keyDown{};

	bool scrollTopInit = false;
	float scrollTopY = 0.0f;

	void disableMouseMode();
	void enableMouseMode();

	void firstPersonMouseControls();
	void firstPersonKeyboardControls(float sensitivity = 1.0f);

	void mapMouseControls();
	void scrollBarMouseControls(float &scrollMinY, float &scrollMaxY, bool inverted);
	void mapKeyboardControls();

	std::function<void(double)> mouseScrollCallback;

  private:
	void applyVerticalDeltaClamped(float dy, float minY, float maxY);
};
