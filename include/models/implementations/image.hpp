#pragma once

#include "model.hpp"
#include <vulkan/vulkan_core.h>

class Image : public Model {
  public:
	Image(const std::string &texturePath);
	Image(Image &&) = default;
	Image(const Image &) = delete;
	Image &operator=(Image &&) = delete;
	Image &operator=(const Image &) = delete;
	~Image() = default;

	const std::string texturePath;

  private:
	void createTextureImage();
};
