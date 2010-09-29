/*
 * This file is part of bino, a program to play stereoscopic videos.
 *
 * Copyright (C) 2010  Martin Lambers <marlam@marlam.de>
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

#include "config.h"

#include <cstring>

#include <GL/glew.h>
#include <GL/glu.h>

#include "debug.h"
#include "exc.h"
#include "str.h"
#include "msg.h"

#include "xgl.h"


/*
 * Push and Pop things
 */

static void *xglPush(std::vector<unsigned char> &stack, size_t s)
{
    stack.resize(stack.size() + s);
    return &(stack[stack.size() - s]);
}

static void *xglTop(std::vector<unsigned char> &stack, size_t s)
{
    return &(stack[stack.size() - s]);
}

static void xglPop(std::vector<unsigned char> &stack, size_t s)
{
    stack.resize(stack.size() - s);
}

void xgl::PushProgram(std::vector<unsigned char> &stack)
{
    glGetIntegerv(GL_CURRENT_PROGRAM, static_cast<GLint *>(xglPush(stack, sizeof(GLint))));
}

void xgl::PopProgram(std::vector<unsigned char> &stack)
{
    const GLint *prg = static_cast<const GLint *>(xglTop(stack, sizeof(GLint)));
    glUseProgram(*prg);
    xglPop(stack, sizeof(GLint));
}

void xgl::PushViewport(std::vector<unsigned char> &stack)
{
    glGetIntegerv(GL_VIEWPORT, static_cast<GLint *>(xglPush(stack, 4 * sizeof(GLint))));
}

void xgl::PopViewport(std::vector<unsigned char> &stack)
{
    const GLint *vp = static_cast<const GLint *>(xglTop(stack, 4 * sizeof(GLint)));
    glViewport(vp[0], vp[1], vp[2], vp[3]);
    xglPop(stack, 4 * sizeof(GLint));
}

void xgl::PushFBO(std::vector<unsigned char> &stack)
{
    glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, static_cast<GLint *>(xglPush(stack, sizeof(GLint))));
}

void xgl::PopFBO(std::vector<unsigned char> &stack)
{
    const GLint *fbo = static_cast<const GLint *>(xglTop(stack, sizeof(GLint)));
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, *fbo);
    xglPop(stack, sizeof(GLint));
}

void xgl::PushModelViewMatrix(std::vector<unsigned char> &stack)
{
    glGetFloatv(GL_MODELVIEW_MATRIX, static_cast<GLfloat *>(xglPush(stack, 16 * sizeof(GLfloat))));
}

void xgl::PopModelViewMatrix(std::vector<unsigned char> &stack)
{
    GLint m;
    glGetIntegerv(GL_MATRIX_MODE, &m);
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(static_cast<const GLfloat *>(xglTop(stack, 16 * sizeof(GLfloat))));
    xglPop(stack, 16 * sizeof(GLfloat));
    glMatrixMode(m);
}

void xgl::PushProjectionMatrix(std::vector<unsigned char> &stack)
{
    glGetFloatv(GL_PROJECTION_MATRIX, static_cast<GLfloat *>(xglPush(stack, 16 * sizeof(GLfloat))));
}

void xgl::PopProjectionMatrix(std::vector<unsigned char> &stack)
{
    GLint m;
    glGetIntegerv(GL_MATRIX_MODE, &m);
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(static_cast<const GLfloat *>(xglTop(stack, 16 * sizeof(GLfloat))));
    xglPop(stack, 16 * sizeof(GLfloat));
    glMatrixMode(m);
}

void xgl::PushEverything(std::vector<unsigned char> &stack)
{
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
    xgl::PushModelViewMatrix(stack);
    xgl::PushProjectionMatrix(stack);
    xgl::PushFBO(stack);
    xgl::PushViewport(stack);
    xgl::PushProgram(stack);
}

void xgl::PopEverything(std::vector<unsigned char> &stack)
{
    xgl::PopProgram(stack);
    xgl::PopViewport(stack);
    xgl::PopFBO(stack);
    xgl::PopProjectionMatrix(stack);
    xgl::PopModelViewMatrix(stack);
    glPopClientAttrib();
    glPopAttrib();
}


/*
 * Error checking
 */

bool xgl::CheckFBO(const GLenum target, const std::string &where)
{
    GLenum status = glCheckFramebufferStatusEXT(target);
    if (status != GL_FRAMEBUFFER_COMPLETE_EXT)
    {
        std::string errstr;
        switch (status)
        {
            case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT:
                errstr = "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT";
                break;

            case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
                errstr = "GL_FRAMEBUFFER_UNSUPPORTED_EXT";
                break;

            case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT:
                errstr = "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT";
                break;

            case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
                errstr = "GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT";
                break;

            case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT:
                errstr = "GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT";
                break;

            case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT:
                errstr = "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT";
                break;

            case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT:
                errstr = "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT";
                break;

            case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT:
                errstr = "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT";
                break;

            case 0:
                errstr = "in glCheckFramebufferStatus";
                break;

            default:
                errstr = str::asprintf("0x%X", static_cast<unsigned int>(status));
                break;
        }
        std::string pfx = (where.length() > 0 ? where + ": " : "");
        throw exc(pfx + "OpenGL FBO error " + errstr);
        return false;
    }
    return true;
}

bool xgl::CheckFBO(const GLenum target)
{
    return xgl::CheckFBO(target, "");
}

bool xgl::CheckError(const std::string &where)
{
    GLenum e = glGetError();
    if (e != GL_NO_ERROR)
    {
        std::string pfx = (where.length() > 0 ? where + ": " : "");
        throw exc(pfx + "OpenGL error " + str::asprintf("0x%04X: ", static_cast<unsigned int>(e))
                + reinterpret_cast<const char *>(gluErrorString(e)));
        return false;
    }
    return true;
}

bool xgl::CheckError()
{
    return xgl::CheckError("");
}


/*
 * Shaders and Programs
 */

static void xglKillCrlf(char *str)
{
    size_t l = strlen(str);
    if (l > 0 && str[l - 1] == '\n')
        str[--l] = '\0';
    if (l > 0 && str[l - 1] == '\r')
        str[l - 1] = '\0';
}

static int xglSkipWhitespace(const std::string &s, int i)
{
    while (isspace(s[i]))
        i++;
    return i;
}

std::string xgl::ShaderSourcePrep(const std::string &src, const std::string &defines)
{
    std::string prepped_src(src);
    int defines_index = 0;
    for (;;)
    {
        defines_index = xglSkipWhitespace(defines, defines_index);
        if (defines[defines_index] == '\0')
            break;
        assert(defines[defines_index] == '$');
        int name_start = defines_index;
        defines_index++;
        while (isalnum(defines[defines_index]) || defines[defines_index] == '_')
            defines_index++;
        int name_end = defines_index - 1;
        defines_index = xglSkipWhitespace(defines, defines_index);
        assert(defines[defines_index] == '=');
        defines_index = xglSkipWhitespace(defines, defines_index + 1);
        int value_start = defines_index;
        while (isalnum(defines[defines_index]) || defines[defines_index] == '_')
            defines_index++;
        int value_end = defines_index - 1;
        str::replace(prepped_src,
                std::string(defines, name_start, name_end - name_start + 1),
                std::string(defines, value_start, value_end - value_start + 1));
        defines_index = xglSkipWhitespace(defines, defines_index);
        if (defines[defines_index] == ',')
            defines_index++;
    }
    return prepped_src;
}

GLuint xgl::CompileShader(const std::string &name, GLenum type, const std::string &src)
{
    msg::dbg("compiling %s shader %s",
            type == GL_VERTEX_SHADER ? "vertex" : type == GL_GEOMETRY_SHADER ? "geometry" : "fragment",
            name.c_str());

    GLuint shader = glCreateShader(type);
    const GLchar *glsrc = src.c_str();
    glShaderSource(shader, 1, &glsrc, NULL);
    glCompileShader(shader);

    std::string log;
    GLint e, l;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &e);
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &l);
    if (l > 0)
    {
        char *tmplog = new char[l];
        glGetShaderInfoLog(shader, l, NULL, tmplog);
        xglKillCrlf(tmplog);
        log = std::string(tmplog);
        delete[] tmplog;
    }
    else
    {
        log = std::string("");
    }

    if (e == GL_TRUE && log.length() > 0)
    {
        msg::wrn("%s shader '%s': compiler warning:",
                type == GL_VERTEX_SHADER ? "vertex" : type == GL_GEOMETRY_SHADER_ARB ? "geometry" : "fragment",
                name.c_str());
        msg::wrn_txt("%s", log.c_str());
    }
    else if (e != GL_TRUE)
    {
        std::string when = str::asprintf("%s shader '%s': compilation failed",
                type == GL_VERTEX_SHADER ? "vertex" : type == GL_GEOMETRY_SHADER_ARB ? "geometry" : "fragment",
                name.c_str());
        std::string what = str::asprintf("\n%s", log.length() > 0 ? log.c_str() : "unknown error");
        throw exc(when + ": " + what);
    }
    return shader;
}

GLuint xgl::CreateProgram(GLuint vshader, GLuint gshader, GLuint fshader)
{
    assert(vshader != 0 || gshader != 0 || fshader != 0);

    GLuint program = glCreateProgram();
    if (vshader != 0)
        glAttachShader(program, vshader);
    if (gshader != 0)
        glAttachShader(program, gshader);
    if (fshader != 0)
        glAttachShader(program, fshader);

    return program;
}

GLuint xgl::CreateProgram(const std::string &name,
        const std::string &vshader_src, const std::string &gshader_src, const std::string &fshader_src)
{
    GLuint vshader = 0, gshader = 0, fshader = 0;

    if (vshader_src.length() > 0)
        vshader = xgl::CompileShader(name, GL_VERTEX_SHADER, vshader_src);
    if (gshader_src.length() > 0)
        gshader = xgl::CompileShader(name, GL_GEOMETRY_SHADER_ARB, gshader_src);
    if (fshader_src.length() > 0)
        fshader = xgl::CompileShader(name, GL_FRAGMENT_SHADER, fshader_src);

    return xgl::CreateProgram(vshader, gshader, fshader);
}

void xgl::LinkProgram(const std::string &name, const GLuint prg)
{
    msg::dbg("linking OpenGL program %s", name.c_str());

    glLinkProgram(prg);

    std::string log;
    GLint e, l;
    glGetProgramiv(prg, GL_LINK_STATUS, &e);
    glGetProgramiv(prg, GL_INFO_LOG_LENGTH, &l);
    if (l > 0)
    {
        char *tmplog = new char[l];
        glGetProgramInfoLog(prg, l, NULL, tmplog);
        xglKillCrlf(tmplog);
        log = std::string(tmplog);
        delete[] tmplog;
    }
    else
    {
        log = std::string("");
    }

    if (e == GL_TRUE && log.length() > 0)
    {
        msg::wrn("OpenGL program '%s': linker warning:", name.c_str());
        msg::wrn_txt("%s", log.c_str());
    }
    else if (e != GL_TRUE)
    {
        std::string when = str::asprintf("OpenGL program '%s': linking failed", name.c_str());
        std::string what = str::asprintf("\n%s", log.length() > 0 ? log.c_str() : "unknown error");
        throw exc(when + ": " + what);
    }
}

void xgl::DeleteProgram(GLuint program)
{
    if (glIsProgram(program))
    {
        GLint shader_count;
        glGetProgramiv(program, GL_ATTACHED_SHADERS, &shader_count);
        GLuint *shaders = new GLuint[shader_count];
        glGetAttachedShaders(program, shader_count, NULL, shaders);
        for (int i = 0; i < shader_count; i++)
            glDeleteShader(shaders[i]);
        delete[] shaders;
        glDeleteProgram(program);
    }
}

void xgl::DeletePrograms(GLsizei n, const GLuint *programs)
{
    for (GLsizei i = 0; i < n; i++)
    {
        xgl::DeleteProgram(programs[i]);
    }
}
