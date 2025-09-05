#include "raytracing.hpp"
#include "engine.hpp"
#include "objmodel.hpp"
#include "polygon.hpp"
#include "scenes.hpp"
#include <GLFW/glfw3.h>
#include <memory>
#include <optional>

RayTracing::RayTracing(Scenes &scenes) : Scene(scenes) {
    cube1 = make_unique<Polygon>(
        *this,
        std::vector<Polygon::Vertex>{
            // idx, position                 // color (RGBA)
            /*0*/ {{-0.5f, -0.5f, -0.5f},     {1.0f, 0.0f, 0.0f, 1.0f}}, // LBB
            /*1*/ {{ 0.5f, -0.5f, -0.5f},     {0.0f, 1.0f, 0.0f, 1.0f}}, // RBB
            /*2*/ {{ 0.5f,  0.5f, -0.5f},     {0.0f, 0.0f, 1.0f, 1.0f}}, // RTB
            /*3*/ {{-0.5f,  0.5f, -0.5f},     {1.0f, 1.0f, 0.0f, 1.0f}}, // LTB
            /*4*/ {{-0.5f, -0.5f,  0.5f},     {1.0f, 0.0f, 1.0f, 1.0f}}, // LBF
            /*5*/ {{ 0.5f, -0.5f,  0.5f},     {0.0f, 1.0f, 1.0f, 1.0f}}, // RBF
            /*6*/ {{ 0.5f,  0.5f,  0.5f},     {1.0f, 1.0f, 1.0f, 1.0f}}, // RTF
            /*7*/ {{-0.5f,  0.5f,  0.5f},     {0.0f, 0.0f, 0.0f, 1.0f}}, // LTF
        },
        std::vector<uint16_t>{
            // Front  (+Z)
            4, 5, 6,   6, 7, 4,
            // Back   (-Z)
            1, 0, 3,   3, 2, 1,
            // Left   (-X)
            0, 4, 7,   7, 3, 0,
            // Right  (+X)
            5, 1, 2,   2, 6, 5,
            // Top    (+Y)
            3, 7, 6,   6, 2, 3,
            // Bottom (-Y)
            0, 1, 5,   5, 4, 0,
        }
    );
    cube1->setOnHover([]() {
        std::cout << "Polygon 1 hit " << Engine::time << std::endl;
    });
    cube1->setRayTraceEnabled(true);

    cube2 = make_unique<Polygon>(
        *this,
        std::vector<Polygon::Vertex>{
            // idx, position                 // color (RGBA)
            /*0*/ {{-0.5f, -0.5f, -0.5f},     {1.0f, 0.0f, 0.0f, 0.35f}}, // LBB
            /*1*/ {{ 0.5f, -0.5f, -0.5f},     {0.0f, 1.0f, 0.0f, 0.35f}}, // RBB
            /*2*/ {{ 0.5f,  0.5f, -0.5f},     {0.0f, 0.0f, 1.0f, 0.35f}}, // RTB
            /*3*/ {{-0.5f,  0.5f, -0.5f},     {1.0f, 1.0f, 0.0f, 0.35f}}, // LTB
            /*4*/ {{-0.5f, -0.5f,  0.5f},     {1.0f, 0.0f, 1.0f, 0.35f}}, // LBF
            /*5*/ {{ 0.5f, -0.5f,  0.5f},     {0.0f, 1.0f, 1.0f, 0.35f}}, // RBF
            /*6*/ {{ 0.5f,  0.5f,  0.5f},     {1.0f, 1.0f, 1.0f, 0.35f}}, // RTF
            /*7*/ {{-0.5f,  0.5f,  0.5f},     {0.0f, 0.0f, 0.0f, 0.35f}}, // LTF
        },
        std::vector<uint16_t>{
            // Front  (+Z)
            4, 5, 6,   6, 7, 4,
            // Back   (-Z)
            1, 0, 3,   3, 2, 1,
            // Left   (-X)
            0, 4, 7,   7, 3, 0,
            // Right  (+X)
            5, 1, 2,   2, 6, 5,
            // Top    (+Y)
            3, 7, 6,   6, 2, 3,
            // Bottom (-Y)
            0, 1, 5,   5, 4, 0,
        }
    );
    cube2->setOnHover([]() {
        std::cout << "Polygon 2 hit " << Engine::time << std::endl;
    });
    cube2->setRayTraceEnabled(true);
}

void RayTracing::updateScreenParams() {
    screenParams.viewport.x        = 0.0f;
    screenParams.viewport.y        = 0.0f;
    screenParams.viewport.width    = (float) Engine::swapChainExtent.width;
    screenParams.viewport.height   = (float) Engine::swapChainExtent.height;
    screenParams.viewport.minDepth = 0.0f;
    screenParams.viewport.maxDepth = 1.0f;
    screenParams.scissor.offset = {0, 0};
    screenParams.scissor.extent = Engine::swapChainExtent;
}

void RayTracing::swapChainUpdate() {
    cube1->updateUniformBuffer(std::nullopt, std::nullopt, perspective(radians(45.0f), screenParams.viewport.width / screenParams.viewport.height, 0.1f, 10.0f));
    cube1->updateScreenParams(screenParams);

    cube2->updateUniformBuffer(std::nullopt, std::nullopt, perspective(radians(45.0f), screenParams.viewport.width / screenParams.viewport.height, 0.1f, 10.0f));
    cube2->updateScreenParams(screenParams);
}


void RayTracing::updateComputeUniformBuffers() {}

void RayTracing::computePass() {}

void RayTracing::updateUniformBuffers() {
    // Parameters you can tweak
    const float amplitude = 1.0f;   // how far left/right
    const float speed     = 1.5f;    // how fast (radians/sec)

    // Position along X: [-amplitude, +amplitude]
    float x = amplitude * std::sin(speed * Engine::time);

    cube1->updateUniformBuffer(translate(mat4(1.0f), vec3(x, x, 0.0f)));
    cube2->updateUniformBuffer(scale(persp.model, vec3(2.0f, 2.0f, 2.0f)) * rotate(mat4(1.0f), Engine::time * radians(90.0f), vec3(0.0f, 0.0f, 1.0f)));
}

void RayTracing::renderPass() {
    cube1->render(persp, screenParams);
    cube2->render(persp, screenParams);
}
