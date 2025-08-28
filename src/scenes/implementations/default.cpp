#include "default.hpp"
#include "image.hpp"
#include "polygon.hpp"

Default::Default() {
    this->triangle = make_unique<Polygon>(
        std::vector<Model::Vertex> {
            {{0.0f, 0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
            {{-0.433f, -0.25f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            {{0.433f, -0.25f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        },
        std::vector<uint16_t> {
            0, 1, 2
        }
    );

    this->example = make_unique<Image>(Engine::textureRootPath + "/example/example.png");
    frameCallbacks.emplace_back(
        [this]() {
            static auto startTime = std::chrono::high_resolution_clock::now();
            auto currentTime = std::chrono::high_resolution_clock::now();
            float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

            example->setUniformBuffer(
                rotate(mat4(1.0f), time * radians(90.0f), vec3(0.0f, 0.0f, 1.0f)), 
                lookAt(vec3(2.0f, 2.0f, 2.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 1.0f)),
                perspective(radians(45.0f), Engine::swapChainExtent.width / (float) Engine::swapChainExtent.height, 0.1f, 10.0f)
            );
        }
    );
}

void Default::renderPass() {
    example->render();
    triangle->render(
        mat4(1.0f),
        lookAt(vec3(2.0f, 2.0f, 2.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 1.0f)),
        perspective(radians(45.0f), Engine::swapChainExtent.width / (float) Engine::swapChainExtent.height, 0.1f, 10.0f)
    );
}
