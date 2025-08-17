#include "default.hpp"

void Default::renderPass() {
    models->shapes[Models::TRIANGLE]->setUniformBuffer(
        mat4(1.0f),
        lookAt(vec3(0.0f, 0.1f, 2.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, -2.0f)),
        perspective(radians(45.0f), Engine::swapChainExtent.width / (float) Engine::swapChainExtent.height, 0.1f, 10.0f)
    );
    models->shapes[Models::TRIANGLE]->draw();

    models->shapes[Models::RECTANGLE]->draw();
}

void Default::drawFrame() {
    models->shapes[Models::RECTANGLE]->updateUniformBuffer();
}
