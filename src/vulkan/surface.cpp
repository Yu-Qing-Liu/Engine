#include "surface.hpp"
#include <GLFW/glfw3.h>
#include <stdexcept>

Surface::Surface(VkInstance inst, GLFWwindow *win) : instance(inst), window(win) { createSurface(); }

Surface::~Surface() {
	if (surface != VK_NULL_HANDLE) {
		vkDestroySurfaceKHR(instance, surface, nullptr);
		surface = VK_NULL_HANDLE;
	}
}

void Surface::createSurface() {
	if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
		throw std::runtime_error("failed to create window surface");
	}
}
