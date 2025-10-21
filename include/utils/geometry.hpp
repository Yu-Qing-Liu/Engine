#pragma once

#include "text.hpp"
#include <glm/glm.hpp>

using std::string;

namespace Geometry {

inline glm::vec3 alignTextCentered(const Text &text) {
    float w = text.getPixelWidth(text.textParams.text);
    float h = text.getPixelHeight();
    return glm::vec3{-w / 2.0f, h / 3.3f, 0.0f};
}

} // namespace Geometry
