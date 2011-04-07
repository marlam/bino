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

#include <limits>
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


subtitle_renderer::subtitle_renderer() :
    _ass_initialized(false),
    _ass_library(NULL),
    _ass_renderer(NULL),
    _ass_track(NULL)
{
}

subtitle_renderer::~subtitle_renderer()
{
    if (_ass_track)
    {
        ass_free_track(_ass_track);
    }
    if (_ass_renderer)
    {
        ass_renderer_done(_ass_renderer);
    }
    if (_ass_library)
    {
        ass_library_done(_ass_library);
    }
}

bool subtitle_renderer::render_to_display_size(const subtitle_box &box) const
{
    return (box.format != subtitle_box::image);
}

void subtitle_renderer::prerender(const subtitle_box &box, int64_t timestamp, int width, int height, float pixel_aspect_ratio,
            int &bb_x, int &bb_y, int &bb_w, int &bb_h)
{
    _fmt = box.format;
    switch (_fmt)
    {
    case subtitle_box::text:
    case subtitle_box::ass:
        init_ass();
        prerender_ass(box, timestamp, width, height, pixel_aspect_ratio);
        break;
    case subtitle_box::image:
        prerender_img(box);
        break;
    }
    bb_x = _bb_x;
    bb_y = _bb_y;
    bb_w = _bb_w;
    bb_h = _bb_h;
}

void subtitle_renderer::render(uint32_t *bgra32_buffer)
{
    switch (_fmt)
    {
    case subtitle_box::text:
    case subtitle_box::ass:
        render_ass(bgra32_buffer);
        break;
    case subtitle_box::image:
        render_img(bgra32_buffer);
        break;
    }
}

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
        ass_set_hinting(_ass_renderer, ASS_HINTING_NATIVE);
        ass_set_fonts(_ass_renderer, NULL, "Sans", 1, NULL, 1);
        _ass_initialized = true;
    }
}

void subtitle_renderer::blend_ass_image(const ASS_Image *img, uint32_t *buf)
{
    const unsigned int R = (img->color >> 24u) & 0xffu;
    const unsigned int G = (img->color >> 16u) & 0xffu;
    const unsigned int B = (img->color >>  8u) & 0xffu;
    const unsigned int A = 255u - (img->color & 0xffu);

    unsigned char *src = img->bitmap;
    for (int src_y = 0; src_y < img->h; src_y++)
    {
        int dst_y = src_y + img->dst_y - _bb_y;
        if (dst_y >= _bb_h)
        {
            break;
        }
        for (int src_x = 0; src_x < img->w; src_x++)
        {
            unsigned int a = src[src_x] * A / 255u;
            int dst_x = src_x + img->dst_x - _bb_x;
            if (dst_x >= _bb_w)
            {
                break;
            }
            uint32_t oldval = buf[dst_y * _bb_w + dst_x];
            // XXX: The BGRA layout used here may be wrong on big endian system
            uint32_t newval = std::min(a + (oldval >> 24u), 255u) << 24u
                | ((a * R + (255u - a) * ((oldval >> 16u) & 0xffu)) / 255u) << 16u
                | ((a * G + (255u - a) * ((oldval >>  8u) & 0xffu)) / 255u) << 8u
                | ((a * B + (255u - a) * ((oldval       ) & 0xffu)) / 255u);
            buf[dst_y * _bb_w + dst_x] = newval;
        }
        src += img->stride;
    }
}

void subtitle_renderer::prerender_ass(const subtitle_box &box, int64_t timestamp, int width, int height, float pixel_aspect_ratio)
{
    // Set ASS parameters
    ass_set_frame_size(_ass_renderer, width, height);
    ass_set_aspect_ratio(_ass_renderer, 1.0, pixel_aspect_ratio);
    ass_set_font_scale(_ass_renderer, 1.0);

    // Put subtitle data into ASS track
    if (_ass_track)
    {
        ass_free_track(_ass_track);
    }
    _ass_track = ass_new_track(_ass_library);
    if (!_ass_track)
    {
        throw exc("Cannot initialize LibASS track");
    }
    if (box.format == subtitle_box::ass)
    {
        ass_process_codec_private(_ass_track, const_cast<char *>(box.style.c_str()), box.style.length());
        ass_process_data(_ass_track, const_cast<char *>(box.str.c_str()), box.str.length());
    }
    else
    {
        std::string style =
            "[Script Info]\n"
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
        ass_process_codec_private(_ass_track, const_cast<char *>(style.c_str()), style.length());
        ass_process_data(_ass_track, const_cast<char *>(str.c_str()), str.length());
    }

    // Render subtitle
    _ass_img = ass_render_frame(_ass_renderer, _ass_track, timestamp / 1000, NULL);

    // Determine bounding box
    int min_x = width;
    int max_x = -1;
    int min_y = height;
    int max_y = -1;
    ASS_Image *img = _ass_img;
    while (img && img->w > 0 && img->h > 0)
    {
        if (img->dst_x < min_x)
            min_x = img->dst_x;
        if (img->dst_x + img->w - 1 > max_x)
            max_x = img->dst_x + img->w - 1;
        if (img->dst_y < min_y)
            min_y = img->dst_y;
        if (img->dst_y + img->h - 1 > max_y)
            max_y = img->dst_y + img->h - 1;
        img = img->next;
    }
    if (max_x < 0)
    {
        _bb_x = 0;
        _bb_y = 0;
        _bb_w = 0;
        _bb_h = 0;
    }
    else
    {
        _bb_x = min_x;
        _bb_w = max_x - min_x + 1;
        _bb_y = min_y;
        _bb_h = max_y - min_y + 1;
    }
}

void subtitle_renderer::render_ass(uint32_t *bgra32_buffer)
{
    if (_bb_w > 0 && _bb_h > 0)
    {
        std::memset(bgra32_buffer, 0, _bb_w * _bb_h * sizeof(uint32_t));
        ASS_Image *img = _ass_img;
        while (img && img->w > 0 && img->h > 0)
        {
            blend_ass_image(img, bgra32_buffer);
            img = img->next;
        }
    }
}

void subtitle_renderer::prerender_img(const subtitle_box &box)
{
    _img_box = &box;
    // Determine bounding box
    int min_x = std::numeric_limits<int>::max();
    int max_x = -1;
    int min_y = std::numeric_limits<int>::max();
    int max_y = -1;
    for (size_t i = 0; i < box.images.size(); i++)
    {
        const subtitle_box::image_t &img = box.images[i];
        if (img.x < min_x)
            min_x = img.x;
        if (img.x + img.w - 1 > max_x)
            max_x = img.x + img.w - 1;
        if (img.y < min_y)
            min_y = img.y;
        if (img.y + img.h - 1 > max_y)
            max_y = img.y + img.h - 1;
    }
    if (max_x < 0)
    {
        _bb_x = 0;
        _bb_y = 0;
        _bb_w = 0;
        _bb_h = 0;
    }
    else
    {
        _bb_x = min_x;
        _bb_w = max_x - min_x + 1;
        _bb_y = min_y;
        _bb_h = max_y - min_y + 1;
    }
}

void subtitle_renderer::render_img(uint32_t *bgra32_buffer)
{
    if (_bb_w <= 0 || _bb_h <= 0)
    {
        return;
    }

    std::memset(bgra32_buffer, 0, _bb_w * _bb_h * sizeof(uint32_t));
    for (size_t i = 0; i < _img_box->images.size(); i++)
    {
        const subtitle_box::image_t &img = _img_box->images[i];
        const uint8_t *src = &(img.data[0]);
        for (int src_y = 0; src_y < img.h; src_y++)
        {
            int dst_y = src_y + img.y - _bb_y;
            if (dst_y >= _bb_h)
            {
                break;
            }
            for (int src_x = 0; src_x < img.w; src_x++)
            {
                int dst_x = src_x + img.x - _bb_x;
                if (dst_x >= _bb_w)
                {
                    break;
                }
                int palette_index = src[src_x];
                uint32_t palette_entry = reinterpret_cast<const uint32_t *>(&(img.palette[0]))[palette_index];
                unsigned int A = (palette_entry >> 24u);
                unsigned int R = (palette_entry >> 16u) & 0xffu;
                unsigned int G = (palette_entry >> 8u) & 0xffu;
                unsigned int B = palette_entry & 0xffu;
                uint32_t oldval = bgra32_buffer[dst_y * _bb_w + dst_x];
                // XXX: The BGRA layout used here may be wrong on big endian system
                uint32_t newval = std::min(A + (oldval >> 24u), 255u) << 24u
                    | ((A * R + (255u - A) * ((oldval >> 16u) & 0xffu)) / 255u) << 16u
                    | ((A * G + (255u - A) * ((oldval >>  8u) & 0xffu)) / 255u) << 8u
                    | ((A * B + (255u - A) * ((oldval       ) & 0xffu)) / 255u);
                bgra32_buffer[dst_y * _bb_w + dst_x] = newval;
            }
            src += img.linesize;
        }
    }
}
