//
// Created by chern0g0r on 05.01.2022.
//

#include "errors.h"

void sdl2_fail(std::string_view message)
{
    throw std::runtime_error(to_string(message) + SDL_GetError());
}

void glew_fail(std::string_view message, GLenum error)
{
    throw std::runtime_error(to_string(message) + reinterpret_cast<const char *>(glewGetErrorString(error)));
}

void assimp_fail(std::string_view message){
    throw std::runtime_error(to_string(message));
}