#pragma once

#include <vulkan/vulkan.h>

struct GLFWwindow;

class Surface {
  public:
	Surface(VkInstance instance, GLFWwindow *window);
	~Surface();

	VkSurfaceKHR getSurface() const { return surface; }

  private:
	VkInstance instance = VK_NULL_HANDLE; 
	GLFWwindow *window = nullptr;
	VkSurfaceKHR surface = VK_NULL_HANDLE;

	void createSurface();
};
