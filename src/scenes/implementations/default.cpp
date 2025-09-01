#include "default.hpp"
#include "engine.hpp"
#include "objmodel.hpp"
#include "texture.hpp"
#include "polygon.hpp"
#include <memory>

Default::Default() {
    this->triangle = make_unique<Polygon>(
        std::vector<Model::Vertex> {
            {{0.0f, 0.5f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{-0.433f, -0.25f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
            {{0.433f, -0.25f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
        },
        std::vector<uint16_t> {
            0, 1, 2
        }
    );

    this->example = make_unique<Texture>(
        Engine::textureRootPath + "/example/example.png", 
        std::vector<Model::TexVertex> {
            {{0.0f, -0.5f, 0.25f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
            {{1.0f, -0.5f, 0.25f}, {0.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
            {{1.0f, 0.5f, 0.25f}, {0.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
            {{0.0f, 0.5f, 0.25f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},

            {{-0.5f, -0.5f, -0.25f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
            {{0.5f, -0.5f, -0.25f}, {0.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
            {{0.5f, 0.5f, -0.25f}, {0.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
            {{-0.5f, 0.5f, -0.25f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
        },
        std::vector<uint16_t> {
            0, 1, 2, 2, 3, 0,
            4, 5, 6, 6, 7, 4
        }
    );

    this->room = make_unique<OBJModel>(Engine::modelRootPath + "/example/example.obj");

    Text::TextParams tp{ Engine::fontRootPath + "/arial.ttf", 48 };
    this->text = make_unique<Text>(tp);
}

void Default::swapChainUpdate() {
    persp.proj = perspective(radians(45.0f), Engine::swapChainExtent.width / (float) Engine::swapChainExtent.height, 0.1f, 10.0f);
    orthographic.proj = ortho(0.0f, float(Engine::swapChainExtent.width), 0.0f, -float(Engine::swapChainExtent.height), -1.0f, 1.0f);
}

void Default::renderPass() {
    static auto startTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    persp.model = rotate(mat4(1.0f), time * radians(90.0f), vec3(0.0f, 0.0f, 1.0f));
    example->render(persp);
    triangle->render(persp);
    room->render(persp);
    text->renderText(orthographic, "Hello World", 1.0f);
}
