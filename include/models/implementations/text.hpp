#pragma once

#include "model.hpp"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <vulkan/vulkan_core.h>

class Text {
  public:
	Text();
	Text(Text &&) = default;
	Text(const Text &) = delete;
	Text &operator=(Text &&) = delete;
	Text &operator=(const Text &) = delete;
	~Text() = default;
};