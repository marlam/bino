/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010-2011
 * Martin Lambers <marlam@marlam.de>
 * Frédéric Devernay <frederic.devernay@inrialpes.fr>
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

#include <cctype>
#include <cstdlib>

#include "debug.h"
#include "exc.h"
#include "msg.h"
#include "str.h"

#include "input.h"


input::input() throw ()
    : _video_width(-1), _video_height(-1), _video_frame_rate_num(-1), _video_frame_rate_den(-1),
    _audio_rate(-1), _audio_channels(-1), _audio_sample_format(decoder::audio_sample_u8), _duration()
{
}

input::~input()
{
}

std::string input::mode_name(enum mode m)
{
    std::string name;
    switch (m)
    {
    case mono:
        name = "mono";
        break;
    case separate:
        name = "separate";
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
    case automatic:
        name = "automatic";
        break;
    }
    return name;
}

enum input::mode input::mode_from_name(const std::string &name, bool *ok)
{
    enum mode m = mono;
    if (ok)
    {
        *ok = true;
    }
    if (name == "mono")
    {
        m = mono;
    }
    else if (name == "separate")
    {
        m = separate;
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
    else if (name == "automatic")
    {
        m = automatic;
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

bool input::mode_is_2d(enum input::mode m)
{
    return (m == mono);
}

void input::open(std::vector<decoder *> decoders,
        int video0_decoder, int video0_stream,
        int video1_decoder, int video1_stream,
        int audio_decoder, int audio_stream,
        enum mode mode)
{
    const char *tag_value;

    if (video0_decoder == -1)
    {
        throw exc("no video stream");
    }
    if (mode == separate && video1_decoder == -1)
    {
        throw exc("need two video streams");
    }
    if ((mode == left_right || mode == left_right_half)
            && decoders.at(video0_decoder)->video_width(video0_stream) % 2 != 0)
    {
        throw exc("invalid width of left-right video stream");
    }
    if ((mode == top_bottom || mode == top_bottom_half)
            && decoders.at(video0_decoder)->video_height(video0_stream) % 2 != 0)
    {
        throw exc("invalid height of top-bottom video stream");
    }
    if (mode == even_odd_rows && decoders.at(video0_decoder)->video_height(video0_stream) % 2 != 0)
    {
        throw exc("invalid height of even-odd-rows video stream");
    }
    if (video1_decoder != -1)
    {
        if (decoders.at(video1_decoder)->video_width(video1_stream)
                != decoders.at(video0_decoder)->video_width(video0_stream)
            || decoders.at(video1_decoder)->video_height(video1_stream)
                != decoders.at(video0_decoder)->video_height(video0_stream))
        {
            throw exc("video streams have different dimensions");
        }
        if (decoders.at(video1_decoder)->video_aspect_ratio_numerator(video1_stream)
                != decoders.at(video0_decoder)->video_aspect_ratio_numerator(video0_stream)
                || decoders.at(video1_decoder)->video_aspect_ratio_denominator(video1_stream)
                != decoders.at(video0_decoder)->video_aspect_ratio_denominator(video0_stream))
        {
            throw exc("video streams have different aspect ratios");
        }
        if (decoders.at(video1_decoder)->video_frame_rate_numerator(video1_stream)
                != decoders.at(video0_decoder)->video_frame_rate_numerator(video0_stream)
            || decoders.at(video1_decoder)->video_frame_rate_denominator(video1_stream)
                > decoders.at(video0_decoder)->video_frame_rate_denominator(video0_stream))
        {
            throw exc("video streams have different frame rates");
        }
        if (decoders.at(video1_decoder)->video_frame_format(video1_stream)
                != decoders.at(video0_decoder)->video_frame_format(video0_stream))
        {
            throw exc("video streams have different frame formats");
        }
    }
    _mode = mode;
    _swap_eyes = false;
    _initial_skip = 0;
    _decoders = decoders;
    _video_decoders[0] = video0_decoder;
    _video_streams[0] = video0_stream;
    _video_decoders[1] = video1_decoder;
    _video_streams[1] = video1_stream;
    _audio_decoder = audio_decoder;
    _audio_stream = audio_stream;
    _video_width = _decoders.at(_video_decoders[0])->video_width(_video_streams[0]);
    _video_height = _decoders.at(_video_decoders[0])->video_height(_video_streams[0]);
    _video_aspect_ratio = static_cast<float>(_decoders.at(_video_decoders[0])->video_aspect_ratio_numerator(_video_streams[0]))
        / static_cast<float>(_decoders.at(_video_decoders[0])->video_aspect_ratio_denominator(_video_streams[0]));
    /* Check some tags defined at this link: http://www.3dtv.at/Knowhow/StereoWmvSpec_en.aspx
     * This is necessary to make the example movies provided by 3dtv.at work out of the box. */
    if ((tag_value = _decoders.at(_video_decoders[0])->tag_value("StereoscopicSkip")))
    {
        try
        {
            _initial_skip = str::to<int64_t>(tag_value);
        }
        catch (...)
        {
        }
    }
    if ((tag_value = _decoders.at(_video_decoders[0])->tag_value("StereoscopicLayout")))
    {
        if (std::string(tag_value) == "SideBySideRF" || std::string(tag_value) == "OverUnderRT")
        {
            _swap_eyes = true;
        }
    }
    /* If we have more than one video stream, the input mode must be "separate". */
    if (_mode == automatic)
    {
        if (video1_decoder != -1)
        {
            _mode = separate;
        }
    }
    /* First, try to determine the input mode from meta data if none is given. */
    if (_mode == automatic)
    {
        if ((tag_value = _decoders.at(_video_decoders[0])->tag_value("StereoscopicLayout")))
        {
            if (std::string(tag_value) == "SideBySideRF" || std::string(tag_value) == "SideBySideLF")
            {
                _mode = left_right;
                if ((tag_value = _decoders.at(_video_decoders[0])->tag_value("StereoscopicHalfWidth"))
                        && std::string(tag_value) == "1")
                {
                    _mode = left_right_half;
                }
            }
            else if (std::string(tag_value) == "OverUnderRT" || std::string(tag_value) == "OverUnderLT")
            {
                _mode = top_bottom;
                if ((tag_value = _decoders.at(_video_decoders[0])->tag_value("StereoscopicHalfHeight"))
                        && std::string(tag_value) == "1")
                {
                    _mode = top_bottom_half;
                }
            }
        }
    }
    /* If that fails, try to determine the input mode by looking at the file name.
     * These are the file name conventions described here:
     * http://www.tru3d.com/technology/3D_Media_Formats_Software.php?file=TriDef%20Supported%203D%20Formats */
    if (_mode == automatic)
    {
        std::string name = _decoders.at(video0_decoder)->file_name();
        size_t last_dot = name.find_last_of('.');
        name = name.substr(0, last_dot);
        for (size_t i = 0; i < name.length(); i++)
        {
            name[i] = std::tolower(name[i]);
        }
        if (name.length() >= 3 && name.substr(name.length() - 3, 3) == "-lr")
        {
            _mode = left_right;
        }
        else if (name.length() >= 3 && name.substr(name.length() - 3, 3) == "-rl")
        {
            _mode = left_right;
            _swap_eyes = true;
        }
        else if (name.length() >= 4
                && (name.substr(name.length() - 4, 4) == "-lrh"
                    || name.substr(name.length() - 4, 4) == "-lrq"))
        {
            _mode = left_right_half;
        }
        else if (name.length() >= 4
                && (name.substr(name.length() - 4, 4) == "-rlh"
                    || name.substr(name.length() - 4, 4) == "-rlq"))
        {
            _mode = left_right_half;
            _swap_eyes = true;
        }
        else if (name.length() >= 3
                && (name.substr(name.length() - 3, 3) == "-tb"
                    || name.substr(name.length() - 3, 3) == "-ab"))
        {
            _mode = top_bottom;
        }
        else if (name.length() >= 3
                && (name.substr(name.length() - 3, 3) == "-bt"
                    || name.substr(name.length() - 3, 3) == "-ba"))
        {
            _mode = top_bottom;
            _swap_eyes = true;
        }
        else if (name.length() >= 4
                && (name.substr(name.length() - 4, 4) == "-tbh"
                    || name.substr(name.length() - 4, 4) == "-abq"))
        {
            _mode = top_bottom_half;
        }
        else if (name.length() >= 4
                && (name.substr(name.length() - 4, 4) == "-bth"
                    || name.substr(name.length() - 4, 4) == "-baq"))
        {
            _mode = top_bottom_half;
            _swap_eyes = true;
        }
        else if (name.length() >= 3 && name.substr(name.length() - 3, 3) == "-eo")
        {
            _mode = even_odd_rows;
            // all image lines are given in this case, and there should be no interpolation [TODO]
        }
        else if (name.length() >= 3 && name.substr(name.length() - 3, 3) == "-oe")
        {
            _mode = even_odd_rows;
            _swap_eyes = true;
            // all image lines are given in this case, and there should be no interpolation [TODO]
        }
        else if ((name.length() >= 4 && name.substr(name.length() - 4, 4) == "-eoq")
            || (name.length() >= 5 && name.substr(name.length() - 5, 5) == "-3dir"))
        {
            _mode = even_odd_rows;
        }
        else if ((name.length() >= 4 && name.substr(name.length() - 4, 4) == "-oeq")
            || (name.length() >= 4 && name.substr(name.length() - 4, 4) == "-3di"))
        {
            _mode = even_odd_rows;
            _swap_eyes = true;
        }
        else if (name.length() >= 3 && name.substr(name.length() - 3, 3) == "-2d")
        {
            _mode = mono;
        }
    }
    /* If that fails, too, try to determine the input mode from the resolution.*/
    if (_mode == automatic)
    {
        if (_video_width > _video_height)
        {
            if (_video_width / 2 > _video_height)
            {
                _mode = left_right;
            }
            else
            {
                _mode = mono;
            }
        }
        else
        {
            _mode = top_bottom;
        }
    }
    /* At this point, _mode != automatic */

    if (_mode == left_right)
    {
        _video_width /= 2;
        _video_aspect_ratio /= 2.0f;
    }
    else if (_mode == left_right_half)
    {
        _video_width /= 2;
    }
    else if (_mode == top_bottom)
    {
        _video_height /= 2;
        _video_aspect_ratio *= 2.0f;
    }
    else if (_mode == top_bottom_half)
    {
        _video_height /= 2;
    }
    else if (_mode == even_odd_rows)
    {
        _video_height /= 2;
        //_video_aspect_ratio *= 2.0f;
        // The only video files I know of which use row-alternating format (those from stereopia.com)
        // do not want this adjustment of aspect ratio.
    }
    _video_frame_rate_num = _decoders.at(_video_decoders[0])->video_frame_rate_numerator(_video_streams[0]);
    _video_frame_rate_den = _decoders.at(_video_decoders[0])->video_frame_rate_denominator(_video_streams[0]);
    _video_frame_format = _decoders.at(_video_decoders[0])->video_frame_format(_video_streams[0]);

    if (audio_stream != -1)
    {
        _audio_rate = _decoders.at(_audio_decoder)->audio_rate(_audio_stream);
        _audio_channels = _decoders.at(_audio_decoder)->audio_channels(_audio_stream);
        _audio_sample_format = _decoders.at(_audio_decoder)->audio_sample_format(_audio_stream);
    }

    _decoders.at(_video_decoders[0])->activate_video_stream(_video_streams[0]);
    _duration = _decoders.at(_video_decoders[0])->video_duration(_video_streams[0]);
    if (_video_streams[1] != -1)
    {
        _decoders.at(_video_decoders[1])->activate_video_stream(_video_streams[1]);
        _duration = std::min(_duration, _decoders.at(_video_decoders[1])->video_duration(_video_streams[1]));
    }
    if (_audio_stream != -1)
    {
        _decoders.at(_audio_decoder)->activate_audio_stream(_audio_stream);
        _duration = std::min(_duration, _decoders.at(_audio_decoder)->audio_duration(_audio_stream));
    }

    // Skip the initial advertisement in 3dtv.at examples.
    if (_initial_skip > 0)
    {
        _duration -= _initial_skip;
        try
        {
            seek(0);
        }
        catch (...)
        {
            _duration += _initial_skip;
        }
    }

    msg::dbg("video0=%d,%d video1=%d,%d, audio=%d,%d",
            _video_decoders[0], _video_streams[0],
            _video_decoders[1], _video_streams[1],
            _audio_decoder, _audio_stream);
    msg::inf("input:");
    msg::inf("    video: %dx%d, format %s,",
            video_width(), video_height(),
            decoder::video_frame_format_name(video_frame_format()).c_str());
    msg::inf("        aspect ratio %g:1, %g fps, %g seconds,",
            video_aspect_ratio(),
            static_cast<float>(video_frame_rate_numerator()) / static_cast<float>(video_frame_rate_denominator()),
            duration() / 1e6f);
    msg::inf("        stereo mode %s, input eye swap %s",
            (_mode == separate ? "separate-streams"
             : _mode == top_bottom ? "top-bottom" : _mode == top_bottom_half ?  "top-bottom-half"
             : _mode == left_right ? "left-right" : _mode == left_right_half ?  "left-right-half"
             : _mode == even_odd_rows ? "even-odd-rows" : "off"),
            (_swap_eyes ? "on" : "off"));

    if (audio_stream != -1)
    {
        msg::inf("    audio: %d channels, %d Hz, sample format %s",
                audio_channels(), audio_rate(), decoder::audio_sample_format_name(audio_sample_format()).c_str());
    }
    else
    {
        msg::inf("    audio: none");
    }
}

int64_t input::read_video_frame()
{
    int64_t t = _decoders.at(_video_decoders[0])->read_video_frame(_video_streams[0]);
    if (t < 0)
    {
        return t;
    }
    if (_video_decoders[1] != -1)
    {
        int64_t t2 = _decoders.at(_video_decoders[1])->read_video_frame(_video_streams[1]);
        if (t2 < 0)
        {
            return t2;
        }
    }
    return t;
}

void input::prepare_video_frame()
{
    _decoders.at(_video_decoders[0])->get_video_frame(_video_streams[0], video_frame_format(),
            _video_data[0], _video_data_line_size[0]);
    if (_mode == separate)
    {
        _decoders.at(_video_decoders[1])->get_video_frame(_video_streams[1], video_frame_format(),
                _video_data[1], _video_data_line_size[1]);
    }
}

static int next_multiple_of_4(int x)
{
    return (x / 4 + (x % 4 == 0 ? 0 : 1)) * 4;
}

void input::get_video_frame(int view, int plane, void *buf)
{
    if (_swap_eyes)
    {
        view = (view == 0 ? 1 : 0);
    }

    uint8_t *dst = reinterpret_cast<uint8_t *>(buf);
    uint8_t *src;
    size_t src_offset;
    size_t src_row_size;
    size_t dst_row_width;
    size_t dst_row_size;
    size_t height;

    if (video_frame_format() == decoder::frame_format_yuv420p)
    {
        if (plane == 0)
        {
            dst_row_width = video_width();
            dst_row_size = next_multiple_of_4(dst_row_width);
            height = video_height();
        }
        else
        {
            dst_row_width = video_width() / 2;
            dst_row_size = next_multiple_of_4(dst_row_width);
            height = video_height() / 2;
        }
    }
    else
    {
        dst_row_width = video_width() * 4;
        dst_row_size = dst_row_width;
        height = video_height();
    }

    switch (_mode)
    {
    case separate:
        src = _video_data[view][plane];
        src_row_size = _video_data_line_size[view][plane];
        src_offset = 0;
        break;
    case top_bottom:
    case top_bottom_half:
        src = _video_data[0][plane];
        src_row_size = _video_data_line_size[0][plane];
        src_offset = view * height * src_row_size;
        break;
    case left_right:
    case left_right_half:
        src = _video_data[0][plane];
        src_row_size = _video_data_line_size[0][plane];
        src_offset = view * dst_row_width;
        break;
    case even_odd_rows:
        src = _video_data[0][plane];
        src_row_size = 2 * _video_data_line_size[0][plane];
        src_offset = view * _video_data_line_size[0][plane];
        break;
    case mono:
        src = _video_data[0][plane];
        src_row_size = _video_data_line_size[0][plane];
        src_offset = 0;
        break;
    case automatic:
        /* cannot happen */
        break;
    }

    size_t dst_offset = 0;
    for (size_t y = 0; y < height; y++)
    {
        std::memcpy(dst + dst_offset, src + src_offset, dst_row_width);
        dst_offset += dst_row_size;
        src_offset += src_row_size;
    }
}

void input::release_video_frame()
{
    _decoders.at(_video_decoders[0])->release_video_frame(_video_streams[0]);
    if (_video_decoders[1] != -1)
    {
        _decoders.at(_video_decoders[1])->release_video_frame(_video_streams[1]);
    }
}

int64_t input::read_audio_data(void **data, size_t size)
{
    if (_audio_buffer.size() < size)
    {
        _audio_buffer.resize(size);
    }
    *data = _audio_buffer.ptr();
    return _decoders.at(_audio_decoder)->read_audio_data(_audio_stream, _audio_buffer.ptr(), size);
}

void input::seek(int64_t dest_pos)
{
    dest_pos += _initial_skip;
    _decoders.at(_video_decoders[0])->seek(dest_pos);
    if (_video_decoders[1] != -1 && _video_decoders[1] != _video_decoders[0])
    {
        _decoders.at(_video_decoders[1])->seek(dest_pos);
    }
    if (_audio_decoder != -1 && _audio_decoder != _video_decoders[0] && _audio_decoder != _video_decoders[1])
    {
        _decoders.at(_audio_decoder)->seek(dest_pos);
    }
}

void input::close()
{
}
