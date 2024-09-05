/*
 * This file is part of Bino, a 3D video player.
 *
 * Copyright (C) 2022, 2023, 2024
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

#pragma once

#include <QString>
#include <QSurfaceFormat>

// Global boolean variable that tells if the OpenGL flavor is OpenGL ES or desktop GL
extern bool IsOpenGLES;
void initializeIsOpenGLES(const QSurfaceFormat& format);

// Read a complete file into a QString (without error checking;
// intended to be used for resource files)
QString readFile(const char* fileName);

// Check for OpenGL errors
#define CHECK_GL() \
    { \
        GLenum err = glGetError(); \
        if (err != GL_NO_ERROR) { \
            LOG_FATAL("%s:%d: OpenGL error 0x%04X in function %s", __FILE__, __LINE__, err, Q_FUNC_INFO); \
            std::exit(1); \
        } \
    }

// Check for existence of GL_ARB_texture_filter_anisotropic
// or GL_EXT_texture_filter_anisotropic (which does the same)
#ifndef GL_TEXTURE_MAX_ANISOTROPY
# define GL_TEXTURE_MAX_ANISOTROPY 0x84FE
#endif
bool checkTextureAnisotropicFilterAvailability();

// Mipmap generation does not work on MacOS OpenGL 4.1, see https://github.com/marlam/bino/issues/25
// Simply disable all use of mipmaps as a crude workaround.
#if __APPLE__
    #undef  GL_LINEAR_MIPMAP_LINEAR
    #define GL_LINEAR_MIPMAP_LINEAR GL_LINEAR
#endif

// Shortcut to get a string from OpenGL
const char* getOpenGLString(QOpenGLExtraFunctions* gl, GLenum p);
