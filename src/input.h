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

#ifndef INPUT_H
#define INPUT_H

#include <vector>

#include "decoder.h"

#include "blob.h"

class input
{
public:
    enum mode
    {
        mono,                   /* 1 video source: center view */
        separate,               /* 2 video sources: left and right view independent */
        top_bottom,             /* 1 video source: left view top, right view bottom, both with full size */
        top_bottom_half,        /* 1 video source: left view top, right view bottom, both with half size */
        left_right,             /* 1 video source: left view left, right view right, both with full size */
        left_right_half,        /* 1 video source: left view left, right view right, both with half size */
        even_odd_rows,          /* 1 video source: left view even lines, right view odd lines */
        automatic               /* derive mode from metadata or guess */
    };

private:
    std::vector<decoder *> _decoders;
    int _video_decoders[2], _video_streams[2];
    int _audio_decoder, _audio_stream;
    mode _mode;
    bool _swap_eyes;
    int64_t _initial_skip;
    int _video_width;
    int _video_height;
    float _video_aspect_ratio;
    int _video_frame_rate_num;
    int _video_frame_rate_den;
    int _audio_rate;
    int _audio_channels;
    int _audio_bits;
    int64_t _duration;
    blob _audio_buffer;

public:
    input() throw ();
    ~input();

    void open(std::vector<decoder *> decoders,
            int video0_decoder, int video0_stream,
            int video1_decoder, int video1_stream,
            int audio_decoder, int audio_stream,
            mode mode);

    mode mode() const throw ()
    {
        return _mode;
    }

    int video_width() const throw ()
    {
        return _video_width;
    }

    int video_height() const throw ()
    {
        return _video_height;
    }

    float video_aspect_ratio() const throw ()
    {
        return _video_aspect_ratio;
    }

    int video_frame_rate_numerator() const throw ()
    {
        return _video_frame_rate_num;
    }

    int video_frame_rate_denominator() const throw ()
    {
        return _video_frame_rate_den;
    }

    int64_t video_frame_duration() const throw ()
    {
        return static_cast<int64_t>(_video_frame_rate_den) * 1000000 / _video_frame_rate_num;
    }

    int audio_rate() const throw ()
    {
        return _audio_rate;
    }

    int audio_channels() const throw ()
    {
        return _audio_channels;
    }

    int audio_bits() const throw ()
    {
        return _audio_bits;
    }

    int64_t duration() const throw ()
    {
        return _duration;
    }

    int64_t read_video_frame();
    void get_video_frame(
            uint8_t **l_data, int *l_row_width, int *l_row_alignment,
            uint8_t **r_data, int *r_row_width, int *r_row_alignment);
    void drop_video_frame();

    int64_t read_audio_data(void **data, size_t size);

    void seek(int64_t dest_pos);

    void close();
};

#endif
