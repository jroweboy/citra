// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include <glad/glad.h>

namespace GLShader {

struct ProgramBinary {
    GLenum format;
    std::vector<u8> data;
};

/**
 * Utility function to create and compile an OpenGL GLSL shader
 * @param source String of the GLSL shader program
 * @param type Type of the shader (GL_VERTEX_SHADER, GL_GEOMETRY_SHADER or GL_FRAGMENT_SHADER)
 */
GLuint LoadShader(const char* source, GLenum type);

/**
 * Utility function to create and link an OpenGL GLSL shader program
 * @param shaders ID of shaders to attach to the program
 * @returns Handle of the newly created OpenGL program object
 */
GLuint LoadProgram(const std::vector<GLuint>& shaders);

/**
 * Utility function to create and link an OpenGL GLSL shader program given a binary
 * @param binary Struct with the parameters returned from glGetProgramBinary
 */

GLuint LoadBinary(const ProgramBinary& binary);

ProgramBinary GetBinary(GLuint program_id);

} // namespace GLShader
