//
// Created by chern0g0r on 05.01.2022.
//

#ifndef MIXAMORENDERER_SHADER_H
#define MIXAMORENDERER_SHADER_H

#include <GL/glew.h>
#include <string>
#include <stdexcept>

GLuint create_shader(GLenum type, const char * source);

GLuint create_program(GLuint vertex_shader, GLuint fragment_shader);

#endif //MIXAMORENDERER_SHADER_H
