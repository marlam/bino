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

#include "debug.h"
#include "exc.h"
#include "msg.h"
#include "str.h"

#include "input.h"


input::input() throw ()
    : _video_width(-1), _video_height(-1), _video_frame_rate_num(-1), _video_frame_rate_den(-1),
    _audio_rate(-1), _audio_channels(-1), _audio_bits(-1), _duration()
{
}

input::~input()
{
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
    if (_mode == automatic)
    {
        if (video1_decoder != -1)
        {
            _mode = separate;
        }
        else
        {
            if ((tag_value = _decoders.at(_video_decoders[0])->tag_value("StereoscopicLayout")))
            {
                if (std::string(tag_value) == "SideBySideRF")
                {
                    _mode = left_right;
                    _swap_eyes = true;
                }
                else if (std::string(tag_value) == "SideBySideLF")
                {
                    _mode = left_right;
                }
                else if (std::string(tag_value) == "OverUnderRT")
                {
                    _mode = top_bottom;
                    _swap_eyes = true;
                }
                else if (std::string(tag_value) == "OverUnderLT")
                {
                    _mode = top_bottom;
                }
            }
            if (_mode == automatic)
            {
                // these are guesses for untagged video files
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
        }
    }
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

    if (audio_stream != -1)
    {
        _audio_rate = _decoders.at(_audio_decoder)->audio_rate(_audio_stream);
        _audio_channels = _decoders.at(_audio_decoder)->audio_channels(_audio_stream);
        _audio_bits = _decoders.at(_audio_decoder)->audio_bits(_audio_stream);
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

    /*
     * TODO: Once seeking works reliably, skip the initial advertisement in
     * 3dtv.at examples.
    _duration -= _initial_skip;
    try
    {
        _decoders.at(_video_decoders[0])->seek(_initial_skip);
    }
    catch (...)
    {
    }
    */

    msg::dbg("video0=%d,%d video1=%d,%d, audio=%d,%d",
            _video_decoders[0], _video_streams[0],
            _video_decoders[1], _video_streams[1],
            _audio_decoder, _audio_stream);
    msg::inf("input:");
    msg::inf("    video: %dx%d, aspect ratio %g:1, %g fps, %g seconds,",
            video_width(), video_height(), video_aspect_ratio(),
            static_cast<float>(video_frame_rate_numerator()) / static_cast<float>(video_frame_rate_denominator()),
            duration() / 1e6f);
    msg::inf("           stereo mode %s, input eye swap %s",
            (_mode == separate ? "separate-streams" : _mode == top_bottom ? "top-bottom"
             : _mode == left_right ? "left-right" : _mode == even_odd_rows ? "even-odd-rows" : "off"),
            (_swap_eyes ? "on" : "off"));

    if (audio_stream != -1)
    {
        msg::inf("    audio: %d channels, %d bits, %d Hz",
                audio_channels(), audio_bits(), audio_rate());
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

static int get_row_alignment(uint8_t *ptr, size_t line_size)
{
    uintptr_t p = reinterpret_cast<uintptr_t>(ptr);
    int row_alignment = 1;
    if (p % 8 == 0 && line_size % 8 == 0)
    {
        row_alignment = 8;
    }
    else if (p % 4 == 0 && line_size % 4 == 0)
    {
        row_alignment = 4;
    }
    else if (p % 2 == 0 && line_size % 2 == 0)
    {
        row_alignment = 2;
    }
    return row_alignment;
}

void input::get_video_frame(
        uint8_t **l_data, int *l_row_width, int *l_row_alignment,
        uint8_t **r_data, int *r_row_width, int *r_row_alignment)
{
    uint8_t *frame;
    size_t line_size;
    
    _decoders.at(_video_decoders[0])->get_video_frame(_video_streams[0], &frame, &line_size);
    if (_mode == separate)
    {
        uint8_t *frame1;
        size_t line_size1;
        _decoders.at(_video_decoders[1])->get_video_frame(_video_streams[1], &frame1, &line_size1);
        *l_data = frame;
        *l_row_width = video_width();
        *l_row_alignment = get_row_alignment(frame, line_size);
        *r_data = frame1;
        *r_row_width = video_width();
        *r_row_alignment = get_row_alignment(frame1, line_size1);
    }
    else if (_mode == top_bottom || _mode == top_bottom_half)
    {
        *l_data = frame;
        *l_row_width = video_width();
        *l_row_alignment = get_row_alignment(frame, line_size);
        *r_data = *l_data + video_height() * line_size;
        *r_row_width = *l_row_width;
        *r_row_alignment = *l_row_alignment;
    }
    else if (_mode == left_right || _mode == left_right_half)
    {
        *l_data = frame;
        *l_row_width = 2 * video_width();
        *l_row_alignment = get_row_alignment(frame, line_size);
        *r_data = *l_data + 3 * video_width();
        *r_row_width = *l_row_width;
        *r_row_alignment = get_row_alignment(*r_data, line_size);
    }
    else if (_mode == even_odd_rows)
    {
        *l_data = frame;
        *l_row_width = (2 * line_size) / 3;
        *l_row_alignment = get_row_alignment(frame, line_size);
        *r_data = *l_data + line_size;
        *r_row_width = *l_row_width;
        *r_row_alignment = *l_row_alignment;
    }
    else // (_mode == mono)
    {
        *l_data = frame;
        *l_row_width = video_width();
        *l_row_alignment = get_row_alignment(frame, line_size);
        *r_data = *l_data;
        *r_row_width = *l_row_width;
        *r_row_alignment = *l_row_alignment;
    }
    if (_swap_eyes)
    {
        std::swap(*l_data, *r_data);
        std::swap(*l_row_width, *r_row_width);
        std::swap(*l_row_alignment, *r_row_alignment);
    }
}

void input::drop_video_frame()
{
    _decoders.at(_video_decoders[0])->drop_video_frame(_video_streams[0]);
    if (_video_decoders[1] != -1)
    {
        _decoders.at(_video_decoders[1])->drop_video_frame(_video_streams[1]);
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
