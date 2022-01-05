//
// Created by chern0g0r on 05.01.2022.
//

#ifndef MIXAMORENDERER_UTILS_H
#define MIXAMORENDERER_UTILS_H

#include <GL/glew.h>

#include <string>
#include <vector>
#include <iostream>

std::string to_string(std::string_view str);

void save_texture(GLuint target, const char * const filename);
#endif //MIXAMORENDERER_UTILS_H
