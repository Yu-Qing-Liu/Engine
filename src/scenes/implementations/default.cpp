#include "default.hpp"

void Default::render() {
    models->models[Models::RECTANGLE]->draw();
}

void Default::drawFrame() {
    models->models[Models::RECTANGLE]->updateUniformBuffer();
}
