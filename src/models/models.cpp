#include "models.hpp"
#include "engineutils.hpp"
#include "rectangle.hpp"
#include "triangle.hpp"

Models::Models() {
    models.emplace(TRIANGLE, std::make_unique<Triangle>(Engine::shaderRootPath + "/triangle"));
    models.emplace(RECTANGLE, std::make_unique<Rectangle>(Engine::shaderRootPath + "/rectangle"));
}
