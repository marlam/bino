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

#include "config.h"

#include <cstring>
#include <stdint.h>

#include <GL/glew.h>

extern "C"
{
#include <ass/ass.h>
}

#include "exc.h"
#include "str.h"
#include "blob.h"
#include "msg.h"

#include "subtitle_renderer.h"


static void libass_msg_callback(int level, const char *fmt, va_list args, void *)
{
    msg::level_t l;
    switch (level)
    {
    default:
    case 0:
        l = msg::ERR;
        break;
    case 1:
        l = msg::ERR;
        break;
    case 2:
        l = msg::WRN;
        break;
    case 3:
        l = msg::WRN;
        break;
    case 4:
        l = msg::WRN;
        break;
    case 5:
        l = msg::INF;
        break;
    case 6:
        l = msg::DBG;
        break;
    case 7:
        l = msg::DBG;
        break;
    }
    std::string s = str::vasprintf(fmt, args);
    msg::msg(l, std::string("LibASS: ") + s);
}

subtitle_renderer::subtitle_renderer() :
    _ass_initialized(false),
    _ass_library(NULL),
    _ass_renderer(NULL)
{
}

subtitle_renderer::~subtitle_renderer()
{
    if (_ass_renderer)
    {
        ass_renderer_done(_ass_renderer);
    }
    if (_ass_library)
    {
        ass_library_done(_ass_library);
    }
}

void subtitle_renderer::init_ass()
{
    if (!_ass_initialized)
    {
        _ass_library = ass_library_init();
        if (!_ass_library)
        {
            throw exc("Cannot initialize LibASS");
        }
        ass_set_message_cb(_ass_library, libass_msg_callback, NULL);
        ass_set_fonts_dir(_ass_library, "");
        ass_set_extract_fonts(_ass_library, 1);
        ass_set_style_overrides(_ass_library, NULL);
        _ass_renderer = ass_renderer_init(_ass_library);
        if (!_ass_renderer)
        {
            throw exc("Cannot initialize LibASS renderer");
        }
        ass_set_margins(_ass_renderer, 0, 0, 0, 0);
        ass_set_use_margins(_ass_renderer, 1);
        ass_set_font_scale(_ass_renderer, 1.0);
        ass_set_hinting(_ass_renderer, ASS_HINTING_LIGHT);
        ass_set_fonts(_ass_renderer, NULL, "Sans", 1, NULL, 1);
        _ass_initialized = true;
    }
}

void subtitle_renderer::render(const video_frame &frame, const subtitle_box &box, uint32_t *rgba32_buffer)
{
    init_ass();

    switch (box.format)
    {
    case subtitle_box::text:
    case subtitle_box::ass:
        render_ass(frame, box, rgba32_buffer);
        break;
    }
}

void subtitle_renderer::render_ass(const video_frame &frame, const subtitle_box &box, uint32_t *rgba32_buffer)
{
    // Set ASS parameters
    ass_set_frame_size(_ass_renderer, frame.width, frame.height);

    // Put subtitle data into ASS track
    ASS_Track *ass_track = ass_new_track(_ass_library);
    if (!ass_track)
    {
        throw exc("Cannot initialize LibASS track");
    }
    if (box.format == subtitle_box::ass)
    {
        ass_process_codec_private(ass_track, const_cast<char *>(box.style.c_str()), box.style.length());
        ass_process_data(ass_track, const_cast<char *>(box.str.c_str()), box.str.length());
    }
    else
    {
        std::string style =
            "ScriptType: v4.00+\n"
            "\n"
            "[V4+ Styles]\n"
            "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, "
            "OutlineColour, BackColour, Bold, Italic, Underline, BorderStyle, "
            "Outline, Shadow, Alignment, MarginL, MarginR, MarginV, AlphaLevel, Encoding\n"
            "Style: Default,Arial,16,&Hffffff,&Hffffff,&H0,&H0,0,0,0,1,1,0,2,10,10,10,0,0\n"
            "\n"
            "[Events]\n"
            "Format: Layer, Start, End, Text\n"
            "\n";
        std::string str = "Dialogue: 0,0:00:00.00,9:00:00.00," + box.str;
        ass_process_codec_private(ass_track, const_cast<char *>(style.c_str()), style.length());
        ass_process_data(ass_track, const_cast<char *>(str.c_str()), str.length());
    }

    // Render subtitle
    ASS_Image *img = ass_render_frame(_ass_renderer, ass_track, box.presentation_start_time / 1000, NULL);

    // Render into buffer
    std::memset(rgba32_buffer, 0, frame.width * frame.height * sizeof(uint32_t));
    while (img && img->w > 0 && img->h > 0)
    {
        blend_ass_image(img, frame.width, frame.height, rgba32_buffer);
        img = img->next;
    }

    // Cleanup
    ass_free_track(ass_track);
}

void subtitle_renderer::blend_ass_image(const ASS_Image *img, int frame_width, int frame_height, uint32_t *buf)
{
    const unsigned int R = (img->color >> 24);
    const unsigned int G = (img->color >> 16) & 0xff;
    const unsigned int B = (img->color >> 8) & 0xff;
    const unsigned int A = 255 - (img->color & 0xff);

    unsigned char *src = img->bitmap;
    for (int src_y = 0; src_y < img->h; src_y++)
    {
        int dst_y = img->dst_y + src_y;
        if (dst_y >= frame_height)
        {
            break;
        }
        for (int src_x = 0; src_x < img->w; src_x++)
        {
            unsigned int k = src[src_x] * A / 255;
            int dst_x = img->dst_x + src_x;
            if (dst_x >= frame_width)
            {
                break;
            }
            uint32_t *dst = buf + (dst_y * frame_width + dst_x);
            unsigned int old_r = (*dst >> 24);
            unsigned int old_g = (*dst >> 16) & 0xff;
            unsigned int old_b = (*dst >> 8) & 0xff;
            unsigned int r = (k * R + (255 - k) * old_r) / 255;
            unsigned int g = (k * G + (255 - k) * old_g) / 255;
            unsigned int b = (k * B + (255 - k) * old_b) / 255;
            *dst = (r << 24) | (g << 16) | (b << 8) | k;
        }
        src += img->stride;
    }
}
