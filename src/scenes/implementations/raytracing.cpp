#include "raytracing.hpp"
#include "engine.hpp"
#include "objmodel.hpp"
#include "polygon.hpp"
#include "scenes.hpp"
#include "colors.hpp"
#include <GLFW/glfw3.h>
#include <memory>
#include <optional>

RayTracing::RayTracing(Scenes &scenes) : Scene(scenes) {
    cube1 = make_unique<Polygon>(
        *this,
        persp,
        screenParams,
        std::vector<Polygon::Vertex>{
            // idx, position                 // color (RGBA)
            /*0*/ {{-0.5f, -0.5f, -0.5f},     {1.0f, 1.0f, 1.0f, 1.0f}}, // LBB
            /*1*/ {{ 0.5f, -0.5f, -0.5f},     {1.0f, 1.0f, 1.0f, 1.0f}}, // RBB
            /*2*/ {{ 0.5f,  0.5f, -0.5f},     {1.0f, 1.0f, 1.0f, 1.0f}}, // RTB
            /*3*/ {{-0.5f,  0.5f, -0.5f},     {1.0f, 1.0f, 1.0f, 1.0f}}, // LTB
            /*4*/ {{-0.5f, -0.5f,  0.5f},     {1.0f, 1.0f, 1.0f, 1.0f}}, // LBF
            /*5*/ {{ 0.5f, -0.5f,  0.5f},     {1.0f, 1.0f, 1.0f, 1.0f}}, // RBF
            /*6*/ {{ 0.5f,  0.5f,  0.5f},     {1.0f, 1.0f, 1.0f, 1.0f}}, // RTF
            /*7*/ {{-0.5f,  0.5f,  0.5f},     {1.0f, 1.0f, 1.0f, 1.0f}}, // LTF
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
    cube1->params.color = Colors::RED;
    cube1->params.outlineColor = Colors::RED;
    cube1->onMouseEnter = [this]() {
        std::cout << "Mouse Entered" << std::endl;
        cube1->params.outlineColor = Colors::YELLOW;
    };
    cube1->onMouseExit = [this]() {
        std::cout << "Mouse Exited" << std::endl;
        cube1->params.outlineColor = Colors::RED;
    };
    cube1->setRayTraceEnabled(true);
}

void RayTracing::updateScreenParams() {
    screenParams.viewport.x        = (float) Engine::swapChainExtent.width / 2;
    screenParams.viewport.y        = 0.0f;
    screenParams.viewport.width    = (float) Engine::swapChainExtent.width / 2;
    screenParams.viewport.height   = (float) Engine::swapChainExtent.height / 2;
    screenParams.viewport.minDepth = 0.0f;
    screenParams.viewport.maxDepth = 1.0f;
    screenParams.scissor.offset = {(int32_t)screenParams.viewport.x, (int32_t)screenParams.viewport.y};
    screenParams.scissor.extent = {(uint32_t)screenParams.viewport.width, (uint32_t)screenParams.viewport.height};
}

void RayTracing::swapChainUpdate() {
    persp.proj = perspective(radians(45.0f), screenParams.viewport.width / screenParams.viewport.height, 0.1f, 10.0f);
    cube1->updateUniformBuffer(std::nullopt, std::nullopt, persp.proj);
}

void RayTracing::updateComputeUniformBuffers() {}

void RayTracing::computePass() {}

void RayTracing::updateUniformBuffers() {
    cube1->updateUniformBuffer(scale(persp.model, vec3(2.0f, 2.0f, 2.0f)) * rotate(mat4(1.0f), Engine::time * radians(90.0f), vec3(0.0f, 0.0f, 1.0f)));
}

void RayTracing::renderPass() {
    cube1->render();
}
