#include "models.hpp"
#include "engineutils.hpp"
#include "rectangle.hpp"

Models::Models() {
    models.emplace(RECTANGLE, std::make_unique<Rectangle>());
}
