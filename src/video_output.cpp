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

#include "video_output.h"


video_output_state::video_output_state() throw ()
    : contrast(0.0f), brightness(0.0f), hue(0.0f), saturation(0.0f),
    fullscreen(false), swap_eyes(false)
{
}

video_output_state::~video_output_state()
{
}


video_output::video_output(bool receive_notifications) throw ()
    : controller(receive_notifications)
{
}

video_output::~video_output()
{
}

std::string video_output::mode_name(enum mode m)
{
    std::string name;
    switch (m)
    {
    case automatic:
        name = "automatic";
        break;
    case stereo:
        name = "stereo";
        break;
    case mono_left:
        name = "mono-left";
        break;
    case mono_right:
        name = "mono-right";
        break;
    case top_bottom:
        name = "top-bottom";
        break;
    case top_bottom_half:
        name = "top-bottom-half";
        break;
    case left_right:
        name = "left-right";
        break;
    case left_right_half:
        name = "left-right-half";
        break;
    case even_odd_rows:
        name = "even-odd-rows";
        break;
    case even_odd_columns:
        name = "even-odd-columns";
        break;
    case checkerboard:
        name = "checkerboard";
        break;
    case anaglyph_red_cyan_monochrome:
        name = "anaglyph-red-cyan-monochrome";
        break;
    case anaglyph_red_cyan_full_color:
        name = "anaglyph-red-cyan-full-color";
        break;
    case anaglyph_red_cyan_half_color:
        name = "anaglyph-red-cyan-half-color";
        break;
    case anaglyph_red_cyan_dubois:
        name = "anaglyph-red-cyan-dubois";
        break;
    }
    return name;
}

enum video_output::mode video_output::mode_from_name(const std::string &name, bool *ok)
{
    enum mode m = mono_left;
    if (ok)
    {
        *ok = true;
    }
    if (name == "automatic")
    {
        m = automatic;
    }
    else if (name == "stereo")
    {
        m = stereo;
    }
    else if (name == "mono-left")
    {
        m = mono_left;
    }
    else if (name == "mono-right")
    {
        m = mono_right;
    }
    else if (name == "top-bottom")
    {
        m = top_bottom;
    }
    else if (name == "top-bottom-half")
    {
        m = top_bottom_half;
    }
    else if (name == "left-right")
    {
        m = left_right;
    }
    else if (name == "left-right-half")
    {
        m = left_right_half;
    }
    else if (name == "even-odd-rows")
    {
        m = even_odd_rows;
    }
    else if (name == "even-odd-columns")
    {
        m = even_odd_columns;
    }
    else if (name == "checkerboard")
    {
        m = checkerboard;
    }
    else if (name == "anaglyph-red-cyan-monochrome")
    {
        m = anaglyph_red_cyan_monochrome;
    }
    else if (name == "anaglyph-red-cyan-full-color")
    {
        m = anaglyph_red_cyan_full_color;
    }
    else if (name == "anaglyph-red-cyan-half-color")
    {
        m = anaglyph_red_cyan_half_color;
    }
    else if (name == "anaglyph-red-cyan-dubois")
    {
        m = anaglyph_red_cyan_dubois;
    }
    else
    {
        if (ok)
        {
            *ok = false;
        }
    }
    return m;
}

bool video_output::mode_is_2d(enum video_output::mode m)
{
    return (m == mono_left || m == mono_right);
}
