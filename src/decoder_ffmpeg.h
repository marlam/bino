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

#ifndef DECODER_FFMPEG_H
#define DECODER_FFMPEG_H

#include "decoder.h"


/* See decoder.h for API documentation */

class decoder_ffmpeg : public decoder
{
private:
    std::string _filename;
    typedef struct internal_stuff stuff_t;
    stuff_t *_stuff;

    bool read();

public:
    decoder_ffmpeg() throw ();
    ~decoder_ffmpeg();

    virtual void open(const std::string &filename);

    virtual int audio_streams() const throw ();
    virtual int video_streams() const throw ();

    virtual void activate_video_stream(int video_stream);
    virtual void activate_audio_stream(int audio_stream);

    virtual int video_width(int video_stream) const throw ();
    virtual int video_height(int video_stream) const throw ();
    virtual int video_aspect_ratio_numerator(int video_stream) const throw ();
    virtual int video_aspect_ratio_denominator(int video_stream) const throw ();
    virtual int video_frame_rate_numerator(int video_stream) const throw ();
    virtual int video_frame_rate_denominator(int video_stream) const throw ();
    virtual int64_t video_duration(int video_stream) const throw ();
    virtual video_frame_format video_preferred_frame_format(int video_stream) const throw ();

    virtual int audio_rate(int audio_stream) const throw ();
    virtual int audio_channels(int audio_stream) const throw ();
    virtual enum audio_sample_format audio_sample_format(int audio_stream) const throw ();
    virtual int64_t audio_duration(int video_stream) const throw ();

    virtual int64_t read_video_frame(int video_stream);
    virtual void get_video_frame(int video_stream, video_frame_format fmt,
            uint8_t *data[3], size_t line_size[3]);
    virtual void release_video_frame(int video_stream);

    virtual int64_t read_audio_data(int audio_stream, void *buffer, size_t size);

    virtual void seek(int64_t pos);

    virtual void close();
};

#endif
