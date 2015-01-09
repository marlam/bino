/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2011, 2012, 2015
 * Martin Lambers <marlam@marlam.de>
 * Joe <cuchac@email.cz>
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

#include <vector>

#include <GL/glew.h>

extern "C"
{
#include <ass/ass.h>
}

#include "base/pth.h"

#include "media_data.h"


class subtitle_renderer;

class subtitle_renderer_initializer : public thread
{
private:
    subtitle_renderer &_subtitle_renderer;

public:
    subtitle_renderer_initializer(subtitle_renderer &renderer);
    void run();
};

class subtitle_renderer
{
private:
    // Initialization
    subtitle_renderer_initializer _initializer;
    bool _initialized;
    const char *_fontconfig_conffile;
    const char *get_fontconfig_conffile();
    void init();
    friend class subtitle_renderer_initializer;

    // Static ASS data
    ASS_Library *_ass_library;
    ASS_Renderer *_ass_renderer;

    // Dynamic data (changes with each subtitle)
    subtitle_box::format_t _fmt;
    ASS_Track *_ass_track;
    ASS_Image *_ass_img;
    const subtitle_box *_img_box;
    int _bb_x, _bb_y, _bb_w, _bb_h;

    // ASS helper functions
    void blend_ass_image(const ASS_Image *img, uint32_t *buf);
    void set_ass_parameters(const parameters &params);

    // Rendering ASS and text subtitles
    bool prerender_ass(const subtitle_box &box, int64_t timestamp,
            const parameters &params,
            int width, int height, float pixel_aspect_ratio);
    void render_ass(uint32_t *bgra32_buffer);

    // Rendering bitmap subtitles
    bool prerender_img(const subtitle_box &box);
    void render_img(uint32_t *bgra32_buffer);

public:
    subtitle_renderer();
    ~subtitle_renderer();

    // Check if the subtitle renderer is initialized. Since initialization may
    // take a long time on systems where fontconfig needs to create its cache first,
    // it is done in a separate thread in the background.
    // You must make sure that the renderer is initialized before calling any of
    // the functions below!
    // In the case of initialization failure, this function will throw the appropriate
    // exception.
    bool is_initialized();

    /*
     * To render a subtitle, the following steps are necessary:
     * 1. Call render_to_display_size() to determine if the subtitle
     *    overlay image should have display size or video frame size.
     * 2. Call prerender() to determine the bounding box inside the
     *    overlay image that the subtitle will occupy.
     * 3. Clear the overlay image, and allocate a BGRA32 buffer for
     *    the bounding box.
     * 4. Call render() to draw the subtitle into the buffer.
     * 5. Update the overlay image with the subtitle bounding box
     *    image from the buffer.
     */

    // Return true if the subtitle should be rendered in display resolution.
    // Return false if the subtitle should be rendered in video frame resolution.
    bool render_to_display_size(const subtitle_box &box) const;

    // Prerender the subtitle, to determine the bounding box it will occupy.
    // The bounding box is relative to the given subtitle overlay size (width and height).
    // This function returns 'false' if the subtitle does not need to be rendered
    // because there was no change relative to the last rendered subtitle.
    bool prerender(const subtitle_box &box, int64_t timestamp,
            const parameters &params,
            int width, int height, float pixel_aspect_ratio,
            int &bb_x, int &bb_y, int &bb_w, int &bb_h);

    // Render the prerendered subtitle into the given BGRA32 buffer, which must
    // have the dimensions of the bounding box that was previously computed.
    void render(uint32_t *bgra32_buffer);
};

#endif
