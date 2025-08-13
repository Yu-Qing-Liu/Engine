#include "models.hpp"
#include "rectangle.hpp"
#include "triangle.hpp"

Models::Models() {
    models.emplace(TRIANGLE, std::make_unique<Triangle>());
    models.emplace(RECTANGLE, std::make_unique<Rectangle>());
}
