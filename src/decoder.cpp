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

#include "decoder.h"

decoder::decoder() throw ()
{
}

decoder::~decoder()
{
}

std::string decoder::audio_sample_format_name(enum audio_sample_format f)
{
    switch (f)
    {
    case audio_sample_u8:
        return "u8";
        break;
    case audio_sample_s16:
        return "s16";
        break;
    case audio_sample_f32:
        return "float";
        break;
    case audio_sample_d64:
        return "double";
        break;
    }
}

int decoder::audio_sample_format_bits(enum audio_sample_format f)
{
    switch (f)
    {
    case audio_sample_u8:
        return 8;
        break;
    case audio_sample_s16:
        return 16;
        break;
    case audio_sample_f32:
        return 32;
        break;
    case audio_sample_d64:
        return 64;
        break;
    }
}

size_t decoder::tags() const
{
    return _tag_names.size();
}

const char *decoder::tag_name(size_t i) const
{
    return _tag_names[i].c_str();
}

const char *decoder::tag_value(size_t i) const
{
    return _tag_values[i].c_str();
}

const char *decoder::tag_value(const char *tag_name) const
{
    for (size_t i = 0; i < _tag_names.size(); i++)
    {
        if (std::string(tag_name) == _tag_names[i])
        {
            return _tag_values[i].c_str();
        }
    }
    return NULL;
}
