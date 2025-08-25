#include "models.hpp"
#include "image.hpp"
#include "rectangle.hpp"
#include "triangle.hpp"

Models::Models() {
    shapes.emplace(TRIANGLE, std::make_unique<Triangle>());
    shapes.emplace(RECTANGLE, std::make_unique<Rectangle>());
    textures.emplace(IMAGE, std::make_unique<Image>(Engine::textureRootPath + "/example/example.png"));
}
