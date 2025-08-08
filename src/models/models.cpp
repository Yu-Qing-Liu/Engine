#include "models.hpp"
#include "triangle.hpp"

Models::Models() {
    models.emplace(TRIANGLE, std::make_unique<Triangle>(Engine::shaderRootPath + "/triangle"));
}
