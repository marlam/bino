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

#ifndef AUDIO_OUTPUT_OPENAL_H
#define AUDIO_OUTPUT_OPENAL_H

#include <vector>
#include <stdint.h>

#include <AL/alut.h>

#include "audio_output.h"


/* See audio_output.h for API documentation. */

class audio_output_openal : public audio_output
{
private:
    static const size_t _num_buffers;
    static const size_t _buffer_size;
    std::vector<unsigned char> _data;
    std::vector<ALuint> _buffers;
    ALuint _source;
    ALenum _format;
    ALint _state;
    int _rate;
    int _channels;
    int _bits;
    int64_t _basetime;
    int64_t _last_timestamp;
    int64_t _ext_timer_at_last_timestamp;
    int64_t _last_reported_timestamp;

public:
    audio_output_openal() throw ();
    ~audio_output_openal();

    virtual void open(int rate, int channels, int bits);

    virtual int64_t status(size_t *required_data);
    virtual void data(const void *buffer, size_t size);
    virtual int64_t start();

    virtual void pause();
    virtual void unpause();

    virtual void stop();

    virtual void close();
};

#endif
