/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010-2011
 * Martin Lambers <marlam@marlam.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef XGL_H
#define XGL_H

#include <vector>
#include <string>

#include <GL/glew.h>


namespace xgl
{
    void PushProgram(std::vector<unsigned char> &stack);
    void PopProgram(std::vector<unsigned char> &stack);
    void PushViewport(std::vector<unsigned char> &stack);
    void PopViewport(std::vector<unsigned char> &stack);
    void PushFBO(std::vector<unsigned char> &stack);
    void PopFBO(std::vector<unsigned char> &stack);
    void PushModelViewMatrix(std::vector<unsigned char> &stack);
    void PopModelViewMatrix(std::vector<unsigned char> &stack);
    void PushProjectionMatrix(std::vector<unsigned char> &stack);
    void PopProjectionMatrix(std::vector<unsigned char> &stack);

    void PushEverything(std::vector<unsigned char> &stack);
    void PopEverything(std::vector<unsigned char> &stack);


    /*
     * Error checking
     */

    /**
     * \param target    One of GL_FRAMEBUFFER_EXT, GL_READ_FRAMEBUFFER_EXT, GL_DRAW_FRAMEBUFFER_EXT.
     * \param where     Location of the check.
     * \returns         True.
     *
     * Checks the status of the current GL framebuffer object of the given type.
     * If an error occured, an appropriate exception is thrown.\n
     * The purpose of the return value is to be able to put this function
     * into an assert statement.
     */
    bool CheckFBO(const GLenum target, const std::string &where);
    bool CheckFBO(const GLenum target);

    /**
     * \param where     Location of the check.
     * \returns         True.
     *
     * Checks if a GL error occured.
     * If an error occured, an appropriate exception is thrown.\n
     * The purpose of the return value is to be able to put this function
     * into an assert statement.
     */
    bool CheckError(const std::string &where);
    bool CheckError();


    /*
     * Shaders and Programs
     */

    /**
     * \param name      Name of the shader. Can be an arbitrary string.
     * \param type      GL_VERTEX_SHADER or GL_GEOMETRY_SHADER or GL_FRAGMENT_SHADER.
     * \param src       The source code of the shader.
     * \returns         The GL shader object.
     *
     * Creates a GL shader object. The \a name of the shader is only used
     * for error reporting purposes. If compilation fails, an exception is thrown.
     */
    GLuint CompileShader(const std::string &name, GLenum type, const std::string &src);

    /**
     * \param vshader   A vertex shader, or 0.
     * \param gshader   A geometry shader, or 0.
     * \param fshader   A fragment shader, or 0.
     * \returns         The GL program object.
     *
     * Creates a GL program object.
     */
    GLuint CreateProgram(GLuint vshader, GLuint gshader, GLuint fshader);

    /**
     * \param name              Name of the program. Can be an arbitrary string.
     * \param vshader_src       Source of the vertex shader, or an empty string.
     * \param gshader_src       Source of the geometry shader, or an empty string.
     * \param fshader_src       Source of the fragment shader, or an empty string.
     * \returns                 The GL program object.
     *
     * Creates a GL program object. The \a name of the program is only used
     * for error reporting purposes.
     */
    GLuint CreateProgram(const std::string &name,
            const std::string &vshader_src,
            const std::string &gshader_src,
            const std::string &fshader_src);

    /**
     * \param name      Name of the program. Can be an arbitrary string.
     * \param prg       The GL program object.
     * \returns         The relinked GL program object.
     *
     * Links a GL program object. The \a name of the program is only used
     * for error reporting purposes. If linking fails, an exception is thrown.
     */
    void LinkProgram(const std::string &name, const GLuint prg);

    /**
     * \param program   The program.
     *
     * Deletes a GL program and all its associated shaders. Does nothing if
     * \a program is not a valid program.
     */
    void DeleteProgram(GLuint program);

    /**
     * \param n         The number of programs.
     * \param program   The programs.
     *
     * Deletes the given GL programs and all their associated shaders. Does nothing if
     * \a program is not a valid program.
     */
    void DeletePrograms(GLsizei n, const GLuint *programs);
};

#endif
