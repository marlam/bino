/*
 * This file is part of bino, a 3D video player.
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

#ifndef AUDIO_OUTPUT_H
#define AUDIO_OUTPUT_H

#include <cstddef>
#include <stdint.h>

#include "decoder.h"
#include "controller.h"


class audio_output : public controller
{
public:
    audio_output(bool receive_notifications = true) throw ();
    virtual ~audio_output();

    /* Open an audio device for output of audio data with the given specifications.
     * Rate is in samples per second, channels is one of 1 (mono), 2 (stereo), 4 (quad),
     * 6 (5:1), 7 (6:1), or 8 (7:1), and format is one of the formats defined by the decoder. */
    virtual void open(int channels, int rate, enum decoder::audio_sample_format format) = 0;
   
    /* To play audio, do the following:
     * - First, call status() to find out the initial amount of audio data that
     *   is required. The returned time stamp is meaningless on this first call.
     * - Then provide exactly the requested amount of data using the data() function.
     * - Then start audio playback using start(). This function returns the audio start
     *   time, in microseconds.
     * - Regularly call status() to get the current audio time and to be notified when
     *   more data is needed (required_data > 0). Provide the requested data using the
     *   data() function. The time position in the audio stream is the time returned by
     *   status() minus the start time that the start() function returned. You can also
     *   just query the time without asking if more data is needed by passing NULL as
     *   required_data. */
    virtual int64_t status(size_t *required_data) = 0;
    virtual void data(const void *buffer, size_t size) = 0;
    virtual int64_t start() = 0;

    /* Pause/unpause audio playback. */
    virtual void pause() = 0;
    virtual void unpause() = 0;

    /* Stop audio playback, and flush all buffers. */
    virtual void stop() = 0;

    /* Finally, clean up and close the device. */
    virtual void close() = 0;
};

#endif
