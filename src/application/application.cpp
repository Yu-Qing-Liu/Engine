#include "application.hpp"
#include "assets.hpp"
#include "events.hpp"
#include "mouse.hpp"
#include <stdexcept>

Application::~Application() { cleanup(); }

int Application::run() {
	Assets::initialize();
	initWindow();
	initEngine();
	mainLoop();
	return 0;
}

void Application::initWindow() {
	glfwSetErrorCallback([](int code, const char *desc) { std::fprintf(stderr, "[GLFW] Error %d: %s\n", code, desc); });
	if (!glfwInit()) {
		throw std::runtime_error("Failed to init GLFW");
	}
	if (!glfwVulkanSupported()) {
		throw std::runtime_error("GLFW was not compiled with vulkan support");
	}

	int plat = glfwGetPlatform();
	std::fprintf(stderr, "[GLFW] platform = %d (%s)\n", plat, plat == GLFW_PLATFORM_WAYLAND ? "Wayland" : plat == GLFW_PLATFORM_X11 ? "X11" : "OTHER");

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	int initialW = 1920;
	int initialH = 1080;

	window = glfwCreateWindow(initialW, initialH, "Vulkan Engine", nullptr, nullptr);
	if (!window) {
		glfwTerminate();
		throw std::runtime_error("Failed to create GLFW window");
	}

	glfwSetWindowUserPointer(window, this);
	glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);

	glfwSetMouseButtonCallback(window, Events::handleMouseCallbacks);
	glfwSetKeyCallback(window, Events::handleKeyboardCallbacks);
	glfwSetCharCallback(window, Events::handleCharacterInputCallbacks);
	glfwSetWindowFocusCallback(window, Events::handleWindowFocusedCallbacks);
	glfwSetScrollCallback(window, Events::handleScrollCallbacks);

	glfwSetCursorPosCallback(window, [](GLFWwindow *win, double mx, double my) {
		Mouse::set(float(mx), float(my));
		Events::handleCursorCallbacks(win, mx, my);
	});
}

void Application::framebufferResizeCallback(GLFWwindow *win, int, int) {
	auto app = reinterpret_cast<Application *>(glfwGetWindowUserPointer(win));
	if (app) {
		app->framebufferResized = true;
	}
}

void Application::initEngine() {
	engine = std::make_shared<Engine>();
	engine->init(window);
	scenes = std::make_unique<Scenes>(engine);
}

void Application::updateTime() {
	const double nowMs = glfwGetTime() * 1000.0;

	static bool initialized = false;
	static double baseMs = 0.0;		 // when we started/rebased
	static double lastFrameMs = 0.0; // previous frame timestamp

	if (!initialized) {
		initialized = true;
		baseMs = nowMs;
		lastFrameMs = nowMs;
		timeSinceLastFrameMs = 0.0;
		elapsedTimeMs = 0.0;
		return;
	}

	double dt = nowMs - lastFrameMs;
	lastFrameMs = nowMs;

	// Clamp weird jumps (minimize/unfocus)
	if (dt < 0.0)
		dt = 0.0;
	if (dt > 250.0)
		dt = 250.0;

	timeSinceLastFrameMs = dt;
	elapsedTimeMs = nowMs - baseMs;

	// Rebase occasionally to avoid precision loss in doubles over very long runs
	constexpr double kRebaseEveryMs = 60.0 * 60.0 * 1000.0; // 1 hour
	if (elapsedTimeMs > kRebaseEveryMs) {
		baseMs += kRebaseEveryMs;
		elapsedTimeMs = nowMs - baseMs; // keep continuity
	}
}

void Application::mainLoop() {
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		updateTime();

		Events::onUpdate.dispatch(timeSinceLastFrameMs);

		// CPU loop pass
		scenes->tick(timeSinceLastFrameMs, elapsedTimeMs);

		// Render + present:
		engine->drawFrame(*scenes, framebufferResized);

		// reset the flag if we handled it
		framebufferResized = false;

		if (firstFrame) {
			engine->recreateSwapchain(*scenes);
            firstFrame = false;
		}
	}

	// GPU idle before shutdown
	if (engine->getDevice() != VK_NULL_HANDLE) {
		vkDeviceWaitIdle(engine->getDevice());
	}
}

void Application::cleanup() {
	scenes.reset();
	engine.reset();
	if (window) {
		glfwDestroyWindow(window);
		window = nullptr;
	}
	glfwTerminate();
}
