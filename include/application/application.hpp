#pragma once

#include "engine.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class Application {
  public:
	Application() = default;
	~Application();

	int run();

  private:
	GLFWwindow *window = nullptr;
	bool framebufferResized = false;
	bool firstFrame = true;

	std::shared_ptr<Engine> engine;
	std::unique_ptr<Scenes> scenes;

	double timeSinceLastFrameMs = 0.0; // FPS
	double elapsedTimeMs = 0.0;		   // Total elapsed time since start (reset at large value)

	void initWindow();
	void initEngine();
	void mainLoop();
	void cleanup();

	static void framebufferResizeCallback(GLFWwindow *win, int width, int height);

	void updateTime();
};
