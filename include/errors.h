//
// Created by chern0g0r on 05.01.2022.
//

#ifndef MIXAMORENDERER_ERRORS_H
#define MIXAMORENDERER_ERRORS_H

#include <GL/glew.h>
#include <stdexcept>
#include <SDL2/SDL.h>
#include "utils.h"


void sdl2_fail(std::string_view message);
void glew_fail(std::string_view message, GLenum error);
#endif //MIXAMORENDERER_ERRORS_H
