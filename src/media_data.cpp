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

#include <limits>
#include <cstring>

#include "media_data.h"

#include "str.h"
#include "msg.h"
#include "dbg.h"


video_frame::video_frame() :
    raw_width(-1),
    raw_height(-1),
    raw_aspect_ratio(0.0f),
    width(-1),
    height(-1),
    aspect_ratio(0.0f),
    layout(bgra32),
    color_space(srgb),
    value_range(u8_full),
    chroma_location(center),
    stereo_layout(mono),
    stereo_layout_swap(false),
    presentation_time(std::numeric_limits<int64_t>::min())
{
    for (int i = 0; i < 2; i++)
    {
        for (int p = 0; p < 3; p++)
        {
            data[i][p] = NULL;
            line_size[i][p] = 0;
        }
    }
}

void video_frame::set_view_dimensions()
{
    width = raw_width;
    height = raw_height;
    aspect_ratio = raw_aspect_ratio;
    if (stereo_layout == left_right)
    {
        width /= 2;
        aspect_ratio /= 2.0f;
    }
    else if (stereo_layout == left_right_half)
    {
        width /= 2;
    }
    else if (stereo_layout == top_bottom)
    {
        height /= 2;
        aspect_ratio *= 2.0f;
    }
    else if (stereo_layout == top_bottom_half)
    {
        height /= 2;
    }
    else if (stereo_layout == even_odd_rows)
    {
        height /= 2;
        //aspect_ratio *= 2.0f;
        // The only video files I know of which use row-alternating format (those from stereopia.com)
        // do not want this adjustment of aspect ratio.
    }
}

std::string video_frame::stereo_layout_to_string(stereo_layout_t stereo_layout, bool stereo_layout_swap)
{
    std::string s;
    switch (stereo_layout)
    {
    case mono:
        s = "mono";
        break;
    case separate:
        s = stereo_layout_swap ? "separate-swap" : "separate";
        break;
    case top_bottom:
        s = stereo_layout_swap ? "bottom-top" : "top-bottom";
        break;
    case top_bottom_half:
        s = stereo_layout_swap ? "bottom-top-half" : "top-bottom-half";
        break;
    case left_right:
        s = stereo_layout_swap ? "right-left" : "left-right";
        break;
    case left_right_half:
        s = stereo_layout_swap ? "right-left-half" : "left-right-half";
        break;
    case even_odd_rows:
        s = stereo_layout_swap ? "odd-even-rows" : "even-odd-rows";
        break;
    }
    return s;
}

void video_frame::stereo_layout_from_string(const std::string &s, stereo_layout_t &stereo_layout, bool &stereo_layout_swap)
{
    if (s == "mono")
    {
        stereo_layout = mono;
        stereo_layout_swap = false;
    }
    else if (s == "separate-swap")
    {
        stereo_layout = separate;
        stereo_layout_swap = true;
    }
    else if (s == "separate")
    {
        stereo_layout = separate;
        stereo_layout_swap = false;
    }
    else if (s == "bottom-top")
    {
        stereo_layout = top_bottom;
        stereo_layout_swap = true;
    }
    else if (s == "top-bottom")
    {
        stereo_layout = top_bottom;
        stereo_layout_swap = false;
    }
    else if (s == "bottom-top-half")
    {
        stereo_layout = top_bottom_half;
        stereo_layout_swap = true;
    }
    else if (s == "top-bottom-half")
    {
        stereo_layout = top_bottom_half;
        stereo_layout_swap = false;
    }
    else if (s == "right-left")
    {
        stereo_layout = left_right;
        stereo_layout_swap = true;
    }
    else if (s == "left-right")
    {
        stereo_layout = left_right;
        stereo_layout_swap = false;
    }
    else if (s == "right-left-half")
    {
        stereo_layout = left_right_half;
        stereo_layout_swap = true;
    }
    else if (s == "left-right-half")
    {
        stereo_layout = left_right_half;
        stereo_layout_swap = false;
    }
    else if (s == "odd-even-rows")
    {
        stereo_layout = even_odd_rows;
        stereo_layout_swap = true;
    }
    else if (s == "even-odd-rows")
    {
        stereo_layout = even_odd_rows;
        stereo_layout_swap = false;
    }
    else
    {
        // safe fallback
        stereo_layout = mono;
        stereo_layout_swap = false;
    }
}

std::string video_frame::format_name() const
{
    std::string name = str::asprintf("%dx%d-%g:1-", raw_width, raw_height, raw_aspect_ratio);
    switch (layout)
    {
    case bgra32:
        name += "bgra32";
        break;
    case yuv444p:
        name += "yuv444p";
        break;
    case yuv422p:
        name += "yuv422p";
        break;
    case yuv420p:
        name += "yuv420p";
        break;
    }
    switch (color_space)
    {
    case srgb:
        name += "-srgb";
        break;
    case yuv601:
        name += "-601";
        break;
    case yuv709:
        name += "-709";
        break;
    }
    if (layout != bgra32)
    {
        switch (value_range)
        {
        case u8_full:
            name += "-jpeg";
            break;
        case u8_mpeg:
            name += "-mpeg";
            break;
        }
    }
    if (layout == yuv422p || layout == yuv420p)
    {
        switch (chroma_location)
        {
        case center:
            name += "-c";
            break;
        case left:
            name += "-l";
            break;
        case topleft:
            name += "-tl";
            break;
        }
    }
    name += "-";
    name += stereo_layout_to_string(stereo_layout, stereo_layout_swap);
    return name;
}

std::string video_frame::format_info() const
{
    return str::asprintf("%dx%d, %g:1", width, height, aspect_ratio);
}

static int next_multiple_of_4(int x)
{
    return (x / 4 + (x % 4 == 0 ? 0 : 1)) * 4;
}

void video_frame::copy_plane(int view, int plane, void *buf) const
{
    char *dst = reinterpret_cast<char *>(buf);
    const char *src = NULL;
    size_t src_offset = 0;
    size_t src_row_size = 0;
    size_t dst_row_width = 0;
    size_t dst_row_size = 0;
    size_t lines = 0;

    switch (layout)
    {
    case bgra32:
        dst_row_width = width * 4;
        dst_row_size = dst_row_width;
        lines = height;
        break;

    case yuv444p:
        dst_row_width = width;
        dst_row_size = next_multiple_of_4(dst_row_width);
        lines = height;
        break;

    case yuv422p:
        if (plane == 0)
        {
            dst_row_width = width;
            dst_row_size = next_multiple_of_4(dst_row_width);
            lines = height;
        }
        else
        {
            dst_row_width = width / 2;
            dst_row_size = next_multiple_of_4(dst_row_width);
            lines = height;
        }
        break;

    case yuv420p:
        if (plane == 0)
        {
            dst_row_width = width;
            dst_row_size = next_multiple_of_4(dst_row_width);
            lines = height;
        }
        else
        {
            dst_row_width = width / 2;
            dst_row_size = next_multiple_of_4(dst_row_width);
            lines = height / 2;
        }
        break;
    }

    if (stereo_layout_swap)
    {
        view = (view == 0 ? 1 : 0);
    }
    switch (stereo_layout)
    {
    case mono:
        src = static_cast<const char *>(data[0][plane]);
        src_row_size = line_size[0][plane];
        src_offset = 0;
        break;
    case separate:
        src = static_cast<const char *>(data[view][plane]);
        src_row_size = line_size[view][plane];
        src_offset = 0;
        break;
    case top_bottom:
    case top_bottom_half:
        src = static_cast<const char *>(data[0][plane]);
        src_row_size = line_size[0][plane];
        src_offset = view * lines * src_row_size;
        break;
    case left_right:
    case left_right_half:
        src = static_cast<const char *>(data[0][plane]);
        src_row_size = line_size[0][plane];
        src_offset = view * dst_row_width;
        break;
    case even_odd_rows:
        src = static_cast<const char *>(data[0][plane]);
        src_row_size = 2 * line_size[0][plane];
        src_offset = view * line_size[0][plane];
        break;
    }

    size_t dst_offset = 0;
    for (size_t y = 0; y < lines; y++)
    {
        std::memcpy(dst + dst_offset, src + src_offset, dst_row_width);
        dst_offset += dst_row_size;
        src_offset += src_row_size;
    }
}

audio_blob::audio_blob() :
    channels(-1),
    rate(-1),
    sample_format(u8),
    data(NULL),
    size(0),
    presentation_time(std::numeric_limits<int64_t>::min())
{
}

std::string audio_blob::format_info() const
{
    return str::asprintf("%d channels, %g kHz, %d bit", channels, rate / 1e3f, sample_bits());
}

std::string audio_blob::format_name() const
{
    const char *sample_format_name;
    switch (sample_format)
    {
    case u8:
        sample_format_name = "u8";
        break;
    case s16:
        sample_format_name = "s16";
        break;
    case f32:
        sample_format_name = "f32";
        break;
    case d64:
        sample_format_name = "d64";
        break;
    }
    return str::asprintf("%d-%d-%s", channels, rate, sample_format_name);
}

int audio_blob::sample_bits() const
{
    switch (sample_format)
    {
    case u8:
        return 8;
        break;
    case s16:
        return 16;
        break;
    case f32:
        return 32;
        break;
    case d64:
        return 64;
        break;
    }
}

parameters::parameters() :
    stereo_mode(stereo),
    stereo_mode_swap(false),
    parallax(0.0f),
    crosstalk_r(0.0f),
    crosstalk_g(0.0f),
    crosstalk_b(0.0f),
    ghostbust(0.0f),
    contrast(0.0f),
    brightness(0.0f),
    hue(0.0f),
    saturation(0.0f)
{
}

std::string parameters::stereo_mode_to_string(stereo_mode_t stereo_mode, bool stereo_mode_swap)
{
    std::string s;
    switch (stereo_mode)
    {
    case stereo:
        s = "stereo";
        break;
    case mono_left:
        s = "mono-left";
        break;
    case mono_right:
        s = "mono-right";
        break;
    case top_bottom:
        s = "top-bottom";
        break;
    case top_bottom_half:
        s = "top-bottom-half";
        break;
    case left_right:
        s = "left-right";
        break;
    case left_right_half:
        s = "left-right-half";
        break;
    case even_odd_rows:
        s = "even-odd-rows";
        break;
    case even_odd_columns:
        s = "even-odd-columns";
        break;
    case checkerboard:
        s = "checkerboard";
        break;
    case anaglyph_red_cyan_monochrome:
        s = "anaglyph-red-cyan-monochrome";
        break;
    case anaglyph_red_cyan_full_color:
        s = "anaglyph-red-cyan-full-color";
        break;
    case anaglyph_red_cyan_half_color:
        s = "anaglyph-red-cyan-half-color";
        break;
    case anaglyph_red_cyan_dubois:
        s = "anaglyph-red-cyan-dubois";
        break;
    }
    if (stereo_mode_swap)
    {
        s += "-swap";
    }
    return s;
}

void parameters::stereo_mode_from_string(const std::string &s, stereo_mode_t &stereo_mode, bool &stereo_mode_swap)
{
    size_t x = s.find_last_of("-");
    std::string t;
    if (x != std::string::npos && s.substr(x) == "-swap")
    {
        t = s.substr(0, x);
        stereo_mode_swap = true;
    }
    else
    {
        t = s;
        stereo_mode_swap = false;
    }
    if (t == "stereo")
    {
        stereo_mode = stereo;
    }
    else if (t == "mono-left")
    {
        stereo_mode = mono_left;
    }
    else if (t == "mono-right")
    {
        stereo_mode = mono_right;
    }
    else if (t == "top-bottom")
    {
        stereo_mode = top_bottom;
    }
    else if (t == "top-bottom-half")
    {
        stereo_mode = top_bottom_half;
    }
    else if (t == "left-right")
    {
        stereo_mode = left_right;
    }
    else if (t == "left-right-half")
    {
        stereo_mode = left_right_half;
    }
    else if (t == "even-odd-rows")
    {
        stereo_mode = even_odd_rows;
    }
    else if (t == "even-odd-columns")
    {
        stereo_mode = even_odd_columns;
    }
    else if (t == "checkerboard")
    {
        stereo_mode = checkerboard;
    }
    else if (t == "anaglyph-red-cyan-monochrome" || t == "anaglyph-monochrome")
    {
        stereo_mode = anaglyph_red_cyan_monochrome;
    }
    else if (t == "anaglyph-red-cyan-full-color" || t == "anaglyph-full-color")
    {
        stereo_mode = anaglyph_red_cyan_full_color;
    }
    else if (t == "anaglyph-red-cyan-half-color" || t == "anaglyph-half-color")
    {
        stereo_mode = anaglyph_red_cyan_half_color;
    }
    else if (t == "anaglyph-red-cyan-dubois" || t == "anaglyph-dubois" || t == "anaglyph")
    {
        stereo_mode = anaglyph_red_cyan_dubois;
    }
    else
    {
        // safe fallback
        stereo_mode = mono_left;
    }
}
