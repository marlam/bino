/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2011
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

#ifndef SUBTITLE_RENDERER_H
#define SUBTITLE_RENDERER_H

#include <GL/glew.h>

extern "C"
{
#include <ass/ass.h>
}

#include "media_data.h"


class subtitle_renderer
{
private:
    bool _ass_initialized;
    ASS_Library *_ass_library;
    ASS_Renderer *_ass_renderer;

    void init_ass();

    void render_ass(const video_frame &frame, const subtitle_box &box, uint32_t *rgba32_buffer);
    void blend_ass_image(const ASS_Image *img, int frame_width, int frame_height, uint32_t *buf);

public:
    subtitle_renderer();
    ~subtitle_renderer();

    // Render the subtitle box that belongs to the video frame into a buffer in RGBA32 format.
    // The buffer must have the correct size for frame.height * frame.width pixels of type uint32_t.
    void render(const video_frame &frame, const subtitle_box &box, uint32_t *rgba32_buffer);
};

#endif
