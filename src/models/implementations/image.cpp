#include "image.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.hpp>

Image::Image(const std::string &texturePath) : texturePath(texturePath),
Model(
    Engine::shaderRootPath + "/image", 
    {
        {{}}
    },
    {
        0, 1, 2, 2, 3, 0
    }
) {}

void Image::createTextureImage() {
    int texWidth, texHeight, texChannels;
    stbi_uc *pixels = stbi_load(texturePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

    VkDeviceSize imageSize = texWidth * texHeight * 4;

    if (!pixels) {
        throw std::runtime_error("failed to load texture image!");
    }
}


