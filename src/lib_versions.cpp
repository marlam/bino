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

#include "decoder_ffmpeg.h"
#include "audio_output_openal.h"
#include "video_output_opengl.h"
#include "video_output_opengl_qt.h"
#include "qt_app.h"
#if HAVE_LIBEQUALIZER
#include "player_equalizer.h"
#endif


std::vector<std::string> lib_versions()
{
    std::vector<std::string> v;

    std::vector<std::string> ffmpeg_v = ffmpeg_versions();
    v.insert(v.end(), ffmpeg_v.begin(), ffmpeg_v.end());
    std::vector<std::string> openal_v = openal_versions();
    v.insert(v.end(), openal_v.begin(), openal_v.end());
    std::vector<std::string> glew_v = glew_versions();
    v.insert(v.end(), glew_v.begin(), glew_v.end());
    std::vector<std::string> opengl_v = opengl_versions();
    v.insert(v.end(), opengl_v.begin(), opengl_v.end());
    std::vector<std::string> qt_v = qt_versions();
    v.insert(v.end(), qt_v.begin(), qt_v.end());
#if HAVE_LIBEQUALIZER
    std::vector<std::string> equalizer_v = equalizer_versions();
    v.insert(v.end(), equalizer_v.begin(), equalizer_v.end());
#endif

    return v;
}
