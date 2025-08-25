#include "default.hpp"

void Default::renderPass() {
    // models->shapes[Models::TRIANGLE]->setUniformBuffer(
    //     mat4(1.0f),
    //     lookAt(vec3(0.0f, 0.1f, 2.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, -2.0f)),
    //     perspective(radians(45.0f), Engine::swapChainExtent.width / (float) Engine::swapChainExtent.height, 0.1f, 10.0f)
    // );
    // models->shapes[Models::TRIANGLE]->render();

    // models->shapes[Models::RECTANGLE]->setUniformBuffer(
    //     mat4(1.0f),
    //     lookAt(vec3(0.0f, 0.1f, 2.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, -2.0f)),
    //     perspective(radians(45.0f), Engine::swapChainExtent.width / (float) Engine::swapChainExtent.height, 0.1f, 10.0f)
    // );
    // models->shapes[Models::RECTANGLE]->render();

    models->textures[Models::LOGO]->setOnFrameUpdate([](Model &logo) {
        static auto startTime = std::chrono::high_resolution_clock::now();
        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        logo.setUniformBuffer(
            rotate(mat4(1.0f), time * radians(90.0f), vec3(1.0f, 0.0f, 1.0f)), 
            lookAt(vec3(0.0f, 0.1f, 3.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 2.0f)),
            perspective(radians(45.0f), Engine::swapChainExtent.width / (float) Engine::swapChainExtent.height, 0.1f, 10.0f)
        );
    });
    models->textures[Models::LOGO]->render();
}

void Default::drawFrame() {
    models->textures[Models::LOGO]->draw();
}
