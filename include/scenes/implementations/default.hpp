#include "scenes/scene.hpp"

class Default : Scene {
  public:
	Default(VkDevice &device, VkRenderPass &renderPass, VkExtent2D &swapChainExtent);
	Default(Default &&) = default;
	Default(const Default &) = delete;
	Default &operator=(Default &&) = delete;
	Default &operator=(const Default &) = delete;
	~Default() = default;

  private:
};
