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

#include "media_data.h"

#include "str.h"


audio_blob::audio_blob() :
    channels(-1),
    rate(-1),
    sample_format(u8),
    data(NULL),
    size(0),
    presentation_time(std::numeric_limits<int64_t>::min())
{
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
    return str::asprintf("%d channels, %d Hz, %s samples", channels, rate, sample_format_name);
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
