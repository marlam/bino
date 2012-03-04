/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010, 2011, 2012
 * Martin Lambers <marlam@marlam.de>
 * Gabriele Greco <gabrielegreco@gmail.com>
 * Frédéric Devernay <Frederic.Devernay@inrialpes.fr>
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

#include <vector>
#include <string>

#if !defined(__APPLE__) || defined(HAVE_AL_AL_H)
#  include <AL/al.h>
#  include <AL/alc.h>
#else
#  include <OpenAL/al.h>
#  include <OpenAL/alc.h>
#endif

#include "dispatch.h"


class audio_output : public controller
{
private:
    // Static configuration
    static const size_t _num_buffers;   // Number of audio buffers
    static const size_t _buffer_size;   // Size of each audio buffer

    // OpenAL things
    std::vector<std::string> _devices;  // List of known OpenAL devices
    bool _initialized;                  // Was this initialized?
    ALCdevice *_device;                 // Audio device        
    ALCcontext *_context;               // Audio context associated with device
    std::vector<ALuint> _buffers;       // Buffer handles
    ALuint _source;                     // Audio source
    ALint _state;                       // State of audio source

    // Properties of the audio data in the current buffers
    std::vector<int64_t> _buffer_channels;      // Number of channels
    std::vector<int64_t> _buffer_sample_bits;   // Number of sample bits
    std::vector<int64_t> _buffer_rates;         // Sample rate in Hz

    // Time management
    int64_t _past_time;                 // Time that represents all finished buffers
    int64_t _last_timestamp;            // 
    int64_t _ext_timer_at_last_timestamp;
    int64_t _last_reported_timestamp;

    // Get an OpenAL source format for the audio data in blob (or throw an exception)
    ALenum get_al_format(const audio_blob &blob);

    // Set source parameters
    void set_source_parameters();

public:
    audio_output();
    ~audio_output();
    
    /* How many OpenAL devices are available? */
    int devices() const;
    /* Return the name of OpenAL device i. */
    const std::string &device_name(int i) const;

    /* Initialize the audio device i for output. If i is < 0, the default device
     * will be used. Throw an exception if this fails. */
    void init(int i = -1);
    /* Deinitialize the audio device. */
    void deinit();

    /* To play audio, do the following:
     * - First, call required_initial_data_size() to find out the initial amount
     *   of audio data that is required.
     * - Then provide exactly this amount of data using the data() function.
     * - Then start audio playback using start(). This function returns the audio
     *   start time, in microseconds.
     * - Regularly call status() to get the current audio time and to be notified
     *   when more data is needed. Provide the amount of data given by
     *   required_update_data_size() using the data() function. The time position
     *   in the audio stream is the time returned by status() minus the start time
     *   that the start() function returned. You can also just query the time
     *   without asking if more data is needed by passing NULL as need_data. */
    size_t required_initial_data_size() const;
    size_t required_update_data_size() const;
    int64_t status(bool *need_data);
    void data(const audio_blob &blob);
    int64_t start();

    /* Pause/unpause audio playback. */
    void pause();
    void unpause();

    /* Stop audio playback, and flush all buffers. */
    void stop();

    /* Receive a notification from the dispatch. */
    virtual void receive_notification(const notification& note);
};

#endif
