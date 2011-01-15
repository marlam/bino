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


static std::vector<std::string> ffmpeg_versions(bool html)
{
    const char *ffmpeg_str = html
        ? "<a href=\"http://ffmpeg.org/\">FFmpeg</a>"
        : "FFmpeg";
    std::vector<std::string> v;
    v.push_back(str::asprintf("%s libavformat %d.%d.%d / %d.%d.%d", ffmpeg_str,
                LIBAVFORMAT_VERSION_MAJOR, LIBAVFORMAT_VERSION_MINOR, LIBAVFORMAT_VERSION_MICRO,
                avformat_version() >> 16, avformat_version() >> 8 & 0xff, avformat_version() & 0xff));
    v.push_back(str::asprintf("%s libavcodec %d.%d.%d / %d.%d.%d", ffmpeg_str,
                LIBAVCODEC_VERSION_MAJOR, LIBAVCODEC_VERSION_MINOR, LIBAVCODEC_VERSION_MICRO,
                avcodec_version() >> 16, avcodec_version() >> 8 & 0xff, avcodec_version() & 0xff));
    v.push_back(str::asprintf("%s libswscale %d.%d.%d / %d.%d.%d", ffmpeg_str,
                LIBSWSCALE_VERSION_MAJOR, LIBSWSCALE_VERSION_MINOR, LIBSWSCALE_VERSION_MICRO,
                swscale_version() >> 16, swscale_version() >> 8 & 0xff, swscale_version() & 0xff));
    return v;
}

static std::vector<std::string> openal_versions(bool html)
{
    const char *openal_str = html
        ? "<a href=\"http://kcat.strangesoft.net/openal.html\">OpenAL</a>"
        : "OpenAL";
    std::vector<std::string> v;
    ALCdevice *device = alcOpenDevice(NULL);
    if (device)
    {
        ALCcontext *context = alcCreateContext(device, NULL);
        if (context)
        {
            alcMakeContextCurrent(context);
            v.push_back(std::string(openal_str) + " version " + static_cast<const char *>(alGetString(AL_VERSION)));
            v.push_back(std::string(openal_str) + " renderer " + static_cast<const char *>(alGetString(AL_RENDERER)));
            v.push_back(std::string(openal_str) + " vendor " + static_cast<const char *>(alGetString(AL_VENDOR)));
            alcMakeContextCurrent(NULL);
            alcDestroyContext(context);
        }
        alcCloseDevice(device);
    }
    if (v.size() == 0)
    {
        v.push_back(std::string(openal_str) + " unknown");
    }
    return v;
}

static std::vector<std::string> opengl_versions(bool html)
{
    const char *opengl_str = html
        ? "<a href=\"http://www.opengl.org/\">OpenGL</a>"
        : "OpenGL";
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
        v.push_back(std::string(opengl_str) + " version " + reinterpret_cast<const char *>(glGetString(GL_VERSION)));
        v.push_back(std::string(opengl_str) + " renderer " + reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
        v.push_back(std::string(opengl_str) + " vendor " + reinterpret_cast<const char *>(glGetString(GL_VENDOR)));
        delete tmpwidget;
        if (qt_app_owner)
        {
            exit_qt();
        }
    }
    if (v.size() == 0)
    {
        v.push_back(std::string(opengl_str) + " unknown");
    }
    return v;
}

static std::vector<std::string> glew_versions(bool html)
{
    const char *glew_str = html
        ? "<a href=\"http://glew.sourceforge.net/\">GLEW</a>"
        : "GLEW";
    std::vector<std::string> v;
    v.push_back(str::asprintf("%s %s", glew_str, reinterpret_cast<const char *>(glewGetString(GLEW_VERSION))));
    return v;
}

static std::vector<std::string> equalizer_versions(bool html)
{
    const char *eq_str = html
        ? "<a href=\"http://www.equalizergraphics.com/\">Equalizer</a>"
        : "Equalizer";
    std::vector<std::string> v;
#if HAVE_LIBEQUALIZER
    v.push_back(str::asprintf("%s %d.%d.%d / %d.%d.%d", eq_str,
                EQ_VERSION_MAJOR, EQ_VERSION_MINOR, EQ_VERSION_PATCH,
                static_cast<int>(eq::Version::getMajor()),
                static_cast<int>(eq::Version::getMinor()),
                static_cast<int>(eq::Version::getPatch())));
#else
    v.push_back(str::asprintf("%s not included"));
#endif
    return v;
}

static std::vector<std::string> qt_versions(bool html)
{
    const char *qt_str = html
        ? "<a href=\"\">Qt</a>"
        : "Qt";
    std::vector<std::string> v;
    v.push_back(std::string("Qt ") + QT_VERSION_STR + " / " + qVersion());
    return v;
}

std::vector<std::string> lib_versions(bool html)
{
    std::vector<std::string> v;

    std::vector<std::string> ffmpeg_v = ffmpeg_versions(html);
    v.insert(v.end(), ffmpeg_v.begin(), ffmpeg_v.end());
    std::vector<std::string> openal_v = openal_versions(html);
    v.insert(v.end(), openal_v.begin(), openal_v.end());
    std::vector<std::string> opengl_v = opengl_versions(html);
    v.insert(v.end(), opengl_v.begin(), opengl_v.end());
    std::vector<std::string> glew_v = glew_versions(html);
    v.insert(v.end(), glew_v.begin(), glew_v.end());
    std::vector<std::string> equalizer_v = equalizer_versions(html);
    v.insert(v.end(), equalizer_v.begin(), equalizer_v.end());
    std::vector<std::string> qt_v = qt_versions(html);
    v.insert(v.end(), qt_v.begin(), qt_v.end());

    return v;
}
