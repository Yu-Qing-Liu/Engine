#include "default.hpp"

void Default::renderPass() {
    models->models[Models::TRIANGLE]->draw();
    models->models[Models::RECTANGLE]->draw();
}

void Default::drawFrame() {
    models->models[Models::TRIANGLE]->updateUniformBuffer();
    models->models[Models::RECTANGLE]->updateUniformBuffer();
}
