//
// Created by chern0g0r on 05.01.2022.
//

#include "utils.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

std::string to_string(std::string_view str) {
    return std::string(str.begin(), str.end());
}

void save_texture(GLuint target, const char * const filename) {

    int width, height;

    glGetTexLevelParameteriv(target, 0, GL_TEXTURE_WIDTH, &width);
    glGetTexLevelParameteriv(target, 0, GL_TEXTURE_HEIGHT, &height);

    auto *img = new std::vector<char>(width * height*4);

    glGetTexImage(target, 0, GL_RGBA, GL_UNSIGNED_BYTE, img->data());

    stbi_flip_vertically_on_write(true);

    stbi_write_png(filename, width, height, 4, img->data(), width*4);

    std::cout << "Texture wrote " << filename << '\n';

    delete img;
}