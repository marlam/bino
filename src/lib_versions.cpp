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

#include "config.h"

extern "C"
{
#define __STDC_CONSTANT_MACROS
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

#include <AL/al.h>
#include <AL/alc.h>

#include <GL/glew.h>

#include <QGLWidget>

#if HAVE_LIBEQUALIZER
#include <eq/eq.h>
#endif

#include "qt_app.h"

#include "str.h"


static std::vector<std::string> ffmpeg_versions()
{
    std::vector<std::string> v;
    v.push_back(str::asprintf("libavformat %d.%d.%d / %d.%d.%d",
                LIBAVFORMAT_VERSION_MAJOR, LIBAVFORMAT_VERSION_MINOR, LIBAVFORMAT_VERSION_MICRO,
                avformat_version() >> 16, avformat_version() >> 8 & 0xff, avformat_version() & 0xff));
    v.push_back(str::asprintf("libavcodec %d.%d.%d / %d.%d.%d",
                LIBAVCODEC_VERSION_MAJOR, LIBAVCODEC_VERSION_MINOR, LIBAVCODEC_VERSION_MICRO,
                avcodec_version() >> 16, avcodec_version() >> 8 & 0xff, avcodec_version() & 0xff));
    v.push_back(str::asprintf("libswscale %d.%d.%d / %d.%d.%d",
                LIBSWSCALE_VERSION_MAJOR, LIBSWSCALE_VERSION_MINOR, LIBSWSCALE_VERSION_MICRO,
                swscale_version() >> 16, swscale_version() >> 8 & 0xff, swscale_version() & 0xff));
    return v;
}

static std::vector<std::string> openal_versions()
{
    std::vector<std::string> v;
    ALCdevice *device = alcOpenDevice(NULL);
    if (device)
    {
        ALCcontext *context = alcCreateContext(device, NULL);
        if (context)
        {
            alcMakeContextCurrent(context);
            v.push_back(std::string("version ") + static_cast<const char *>(alGetString(AL_VERSION)));
            v.push_back(std::string("renderer ") + static_cast<const char *>(alGetString(AL_RENDERER)));
            v.push_back(std::string("vendor ") + static_cast<const char *>(alGetString(AL_VENDOR)));
            alcMakeContextCurrent(NULL);
            alcDestroyContext(context);
        }
        alcCloseDevice(device);
    }
    if (v.size() == 0)
    {
        v.push_back("unknown");
    }
    return v;
}

static std::vector<std::string> opengl_versions()
{
    std::vector<std::string> v;
#ifdef Q_WS_X11
    const char *display = getenv("DISPLAY");
    bool have_display = (display && display[0] != '\0');
#else
    bool have_display = true;
#endif
    if (have_display)
    {
        bool qt_app_owner = init_qt();
        QGLWidget *tmpwidget = new QGLWidget();
        tmpwidget->makeCurrent();
        v.push_back(std::string("version ") + reinterpret_cast<const char *>(glGetString(GL_VERSION)));
        v.push_back(std::string("renderer ") + reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
        v.push_back(std::string("vendor ") + reinterpret_cast<const char *>(glGetString(GL_VENDOR)));
        delete tmpwidget;
        if (qt_app_owner)
        {
            exit_qt();
        }
    }
    if (v.size() == 0)
    {
        v.push_back("unknown");
    }
    return v;
}

static std::vector<std::string> glew_versions()
{
    std::vector<std::string> v;
    v.push_back(reinterpret_cast<const char *>(glewGetString(GLEW_VERSION)));
    return v;
}

static std::vector<std::string> equalizer_versions()
{
    std::vector<std::string> v;
#if HAVE_LIBEQUALIZER
    v.push_back(str::asprintf("%d.%d.%d / %d.%d.%d",
                EQ_VERSION_MAJOR, EQ_VERSION_MINOR, EQ_VERSION_PATCH,
                static_cast<int>(eq::Version::getMajor()),
                static_cast<int>(eq::Version::getMinor()),
                static_cast<int>(eq::Version::getPatch())));
#else
    v.push_back("not included");
#endif
    return v;
}

static std::vector<std::string> qt_versions()
{
    std::vector<std::string> v;
    v.push_back(std::string(QT_VERSION_STR) + " / " + qVersion());
    return v;
}

std::vector<std::string> lib_versions(bool html)
{
    const char *ffmpeg_str = html
        ? "<a href=\"http://ffmpeg.org/\">FFmpeg</a>"
        : "FFmpeg";
    const char *openal_str = html
        ? "<a href=\"http://kcat.strangesoft.net/openal.html\">OpenAL</a>"
        : "OpenAL";
    const char *opengl_str = html
        ? "<a href=\"http://www.opengl.org/\">OpenGL</a>"
        : "OpenGL";
    const char *glew_str = html
        ? "<a href=\"http://glew.sourceforge.net/\">GLEW</a>"
        : "GLEW";
    const char *equalizer_str = html
        ? "<a href=\"http://www.equalizergraphics.com/\">Equalizer</a>"
        : "Equalizer";
    const char *qt_str = html
        ? "<a href=\"\">Qt</a>"
        : "Qt";

    static std::vector<std::string> ffmpeg_v;
    if (ffmpeg_v.size() == 0)
    {
        ffmpeg_v = ffmpeg_versions();
    }
    static std::vector<std::string> openal_v;
    if (openal_v.size() == 0)
    {
        openal_v = openal_versions();
    }
    static std::vector<std::string> opengl_v;
    if (opengl_v.size() == 0)
    {
        opengl_v = opengl_versions();
    }
    static std::vector<std::string> glew_v;
    if (glew_v.size() == 0)
    {
        glew_v = glew_versions();
    }
    static std::vector<std::string> equalizer_v;
    if (equalizer_v.size() == 0)
    {
        equalizer_v = equalizer_versions();
    }
    static std::vector<std::string> qt_v;
    if (qt_v.size() == 0)
    {
        qt_v = qt_versions();
    }

    std::vector<std::string> v;
    for (size_t i = 0; i < ffmpeg_v.size(); i++)
    {
        v.push_back(std::string(ffmpeg_str) + " " + ffmpeg_v[i]);
    }
    for (size_t i = 0; i < openal_v.size(); i++)
    {
        v.push_back(std::string(openal_str) + " " + openal_v[i]);
    }
    for (size_t i = 0; i < opengl_v.size(); i++)
    {
        v.push_back(std::string(opengl_str) + " " + opengl_v[i]);
    }
    for (size_t i = 0; i < glew_v.size(); i++)
    {
        v.push_back(std::string(glew_str) + " " + glew_v[i]);
    }
    for (size_t i = 0; i < equalizer_v.size(); i++)
    {
        v.push_back(std::string(equalizer_str) + " " + equalizer_v[i]);
    }
    for (size_t i = 0; i < qt_v.size(); i++)
    {
        v.push_back(std::string(qt_str) + " " + qt_v[i]);
    }

    return v;
}
