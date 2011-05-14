/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010-2011
 * Martin Lambers <marlam@marlam.de>
 * Gabriele Greco <gabrielegreco@gmail.com>
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

#include "gettext.h"
#define _(string) gettext(string)

#include "audio_output.h"
#include "lib_versions.h"

#include "exc.h"
#include "str.h"
#include "msg.h"
#include "timer.h"
#include "dbg.h"


/* This code is adapted from the alffmpeg.c example available here:
 * http://kcat.strangesoft.net/alffmpeg.c (as of 2010-09-12). */

// These number should fit for most formats; see comments in the alffmpeg.c example.
const size_t audio_output::_num_buffers = 3;
const size_t audio_output::_buffer_size = 20160 * 2;

audio_output::audio_output() : controller(), _initialized(false)
{
}

audio_output::~audio_output()
{
    deinit();
}

void audio_output::init()
{
    if (!_initialized)
    {
        if (!(_device = alcOpenDevice(NULL)))
        {
            throw exc(_("No OpenAL device available."));
        }
        if (!(_context = alcCreateContext(_device, NULL)))
        {
            alcCloseDevice(_device);
            throw exc(_("No OpenAL context available."));
        }
        alcMakeContextCurrent(_context);
        set_openal_versions();
        _buffers.resize(_num_buffers);
        alGenBuffers(_num_buffers, &(_buffers[0]));
        if (alGetError() != AL_NO_ERROR)
        {
            alcMakeContextCurrent(NULL);
            alcDestroyContext(_context);
            alcCloseDevice(_device);
            throw exc(_("Cannot create OpenAL buffers."));
        }
        alGenSources(1, &_source);
        if (alGetError() != AL_NO_ERROR)
        {
            alDeleteBuffers(_num_buffers, &(_buffers[0]));
            alcMakeContextCurrent(NULL);
            alcDestroyContext(_context);
            alcCloseDevice(_device);
            throw exc(_("Cannot create OpenAL source."));
        }
        /* Comment from alffmpeg.c:
         * "Set parameters so mono sources won't distance attenuate" */
        alSourcei(_source, AL_SOURCE_RELATIVE, AL_TRUE);
        alSourcei(_source, AL_ROLLOFF_FACTOR, 0);
        if (alGetError() != AL_NO_ERROR)
        {
            alDeleteSources(1, &_source);
            alDeleteBuffers(_num_buffers, &(_buffers[0]));
            alcMakeContextCurrent(NULL);
            alcDestroyContext(_context);
            alcCloseDevice(_device);
            throw exc(_("Cannot set OpenAL source parameters."));
        }
        _state = 0;
        _initialized = true;
    }
}

void audio_output::deinit()
{
    if (_initialized)
    {
        do
        {
            alGetSourcei(_source, AL_SOURCE_STATE, &_state);
        }
        while (alGetError() == AL_NO_ERROR && _state == AL_PLAYING);
        alDeleteSources(1, &_source);
        alDeleteBuffers(_num_buffers, &(_buffers[0]));
        alcMakeContextCurrent(NULL);
        alcDestroyContext(_context);
        alcCloseDevice(_device);
        _initialized = false;
    }
}

size_t audio_output::required_initial_data_size() const
{
    return _num_buffers * _buffer_size;
}

size_t audio_output::required_update_data_size() const
{
    return _buffer_size;
}

int64_t audio_output::status(bool *need_data)
{
    if (_state == 0)
    {
        if (need_data)
        {
            *need_data = true;
        }
        return std::numeric_limits<int64_t>::min();
    }
    else
    {
        ALint processed = 0;
        alGetSourcei(_source, AL_BUFFERS_PROCESSED, &processed);
        if (processed == 0)
        {
            alGetSourcei(_source, AL_SOURCE_STATE, &_state);
            if (alGetError() != AL_NO_ERROR)
            {
                throw exc(_("Cannot check OpenAL source state."));
            }
            if (_state != AL_PLAYING)
            {
                alSourcePlay(_source);
                if (alGetError() != AL_NO_ERROR)
                {
                    throw exc(_("Cannot restart OpenAL source playback."));
                }
            }
            if (need_data)
            {
                *need_data = false;
            }
        }
        else
        {
            if (need_data)
            {
                *need_data = true;
            }
        }
        ALint offset;
        alGetSourcei(_source, AL_SAMPLE_OFFSET, &offset);
        /* The time inside the current buffer */
        int64_t timestamp = static_cast<int64_t>(offset) * 1000000 / _buffer_rates[0];
        /* Add the time for all past buffers */
        timestamp += _past_time;
        /* This timestamp unfortunately only grows in relatively large steps. This is
         * too imprecise for syncing a video stream with. Therefore, we use an external
         * time source between two timestamp steps. In case this external time runs
         * faster than the audio time, we also need to make sure that we do not report
         * timestamps that run backwards. */
        if (timestamp != _last_timestamp)
        {
            _last_timestamp = timestamp;
            _ext_timer_at_last_timestamp = timer::get_microseconds(timer::monotonic);
            _last_reported_timestamp = std::max(_last_reported_timestamp, timestamp);
            return _last_reported_timestamp;
        }
        else
        {
            _last_reported_timestamp = _last_timestamp + (timer::get_microseconds(timer::monotonic) - _ext_timer_at_last_timestamp);
            return _last_reported_timestamp;
        }
    }
}

ALenum audio_output::get_al_format(const audio_blob &blob)
{
    ALenum format = 0;
    if (blob.sample_format == audio_blob::u8)
    {
        if (blob.channels == 1)
        {
            format = AL_FORMAT_MONO8;
        }
        else if (blob.channels == 2)
        {
            format = AL_FORMAT_STEREO8;
        }
        else if (alIsExtensionPresent("AL_EXT_MCFORMATS"))
        {
            if (blob.channels == 4)
            {
                format = alGetEnumValue("AL_FORMAT_QUAD8");
            }
            else if (blob.channels == 6)
            {
                format = alGetEnumValue("AL_FORMAT_51CHN8");
            }
            else if (blob.channels == 7)
            {
                format = alGetEnumValue("AL_FORMAT_71CHN8");
            }
            else if (blob.channels == 8)
            {
                format = alGetEnumValue("AL_FORMAT_81CHN8");
            }
        }
    }
    else if (blob.sample_format == audio_blob::s16)
    {
        if (blob.channels == 1)
        {
            format = AL_FORMAT_MONO16;
        }
        else if (blob.channels == 2)
        {
            format = AL_FORMAT_STEREO16;
        }
        else if (alIsExtensionPresent("AL_EXT_MCFORMATS"))
        {
            if (blob.channels == 4)
            {
                format = alGetEnumValue("AL_FORMAT_QUAD16");
            }
            else if (blob.channels == 6)
            {
                format = alGetEnumValue("AL_FORMAT_51CHN16");
            }
            else if (blob.channels == 7)
            {
                format = alGetEnumValue("AL_FORMAT_61CHN16");
            }
            else if (blob.channels == 8)
            {
                format = alGetEnumValue("AL_FORMAT_71CHN16");
            }
        }
    }
    else if (blob.sample_format == audio_blob::f32)
    {
        if (alIsExtensionPresent("AL_EXT_float32"))
        {
            if (blob.channels == 1)
            {
                format = alGetEnumValue("AL_FORMAT_MONO_FLOAT32");
            }
            else if (blob.channels == 2)
            {
                format = alGetEnumValue("AL_FORMAT_STEREO_FLOAT32");
            }
            else if (alIsExtensionPresent("AL_EXT_MCFORMATS"))
            {
                if (blob.channels == 4)
                {
                    format = alGetEnumValue("AL_FORMAT_QUAD32");
                }
                else if (blob.channels == 6)
                {
                    format = alGetEnumValue("AL_FORMAT_51CHN32");
                }
                else if (blob.channels == 7)
                {
                    format = alGetEnumValue("AL_FORMAT_61CHN32");
                }
                else if (blob.channels == 8)
                {
                    format = alGetEnumValue("AL_FORMAT_71CHN32");
                }
            }
        }
    }
    else if (blob.sample_format == audio_blob::d64)
    {
        if (alIsExtensionPresent("AL_EXT_double"))
        {
            if (blob.channels == 1)
            {
                format = alGetEnumValue("AL_FORMAT_MONO_DOUBLE_EXT");
            }
            else if (blob.channels == 2)
            {
                format = alGetEnumValue("AL_FORMAT_STEREO_DOUBLE_EXT");
            }
        }
    }
    if (format == 0)
    {
        throw exc(str::asprintf(_("No OpenAL format available for "
                        "audio data format %s."), blob.format_name().c_str()));
    }
    return format;
}

void audio_output::data(const audio_blob &blob)
{
    assert(blob.data);
    ALenum format = get_al_format(blob);
    msg::dbg(std::string("Buffering ") + str::from(blob.size) + " bytes of audio data.");
    if (_state == 0)
    {
        // Initial buffering
        assert(blob.size == _num_buffers * _buffer_size);
        char *data = static_cast<char *>(blob.data);
        for (size_t j = 0; j < _num_buffers; j++)
        {
            _buffer_channels.push_back(blob.channels);
            _buffer_sample_bits.push_back(blob.sample_bits());
            _buffer_rates.push_back(blob.rate);
            alBufferData(_buffers[j], format, data, _buffer_size, blob.rate);
            alSourceQueueBuffers(_source, 1, &(_buffers[j]));
            data += _buffer_size;
        }
        if (alGetError() != AL_NO_ERROR)
        {
            throw exc(_("Cannot buffer initial OpenAL data."));
        }
    }
    else if (blob.size > 0)
    {
        // Replace one buffer
        assert(blob.size == _buffer_size);
        ALuint buf = 0;
        alSourceUnqueueBuffers(_source, 1, &buf);
        assert(buf != 0);
        alBufferData(buf, format, blob.data, _buffer_size, blob.rate);
        alSourceQueueBuffers(_source, 1, &buf);
        if (alGetError() != AL_NO_ERROR)
        {
            throw exc(_("Cannot buffer OpenAL data."));
        }
        // Update the time spent on all past buffers
        int64_t current_buffer_samples = _buffer_size / _buffer_channels[0] * 8 / _buffer_sample_bits[0];
        int64_t current_buffer_time = current_buffer_samples * 1000000 / _buffer_rates[0];
        _past_time += current_buffer_time;
        // Remove current buffer entries
        _buffer_channels.erase(_buffer_channels.begin());
        _buffer_sample_bits.erase(_buffer_sample_bits.begin());
        _buffer_rates.erase(_buffer_rates.begin());
        // Add entries for the new buffer
        _buffer_channels.push_back(blob.channels);
        _buffer_sample_bits.push_back(blob.sample_bits());
        _buffer_rates.push_back(blob.rate);
    }
}

int64_t audio_output::start()
{
    msg::dbg("Starting audio output.");
    assert(_state == 0);
    alSourcePlay(_source);
    alGetSourcei(_source, AL_SOURCE_STATE, &_state);
    if (alGetError() != AL_NO_ERROR)
    {
        throw exc(_("Cannot start OpenAL source playback."));
    }
    _past_time = 0;
    _last_timestamp = 0;
    _ext_timer_at_last_timestamp = timer::get_microseconds(timer::monotonic);
    _last_reported_timestamp = _last_timestamp;
    return _last_timestamp;
}

void audio_output::pause()
{
    alSourcePause(_source);
    if (alGetError() != AL_NO_ERROR)
    {
        throw exc(_("Cannot pause OpenAL source playback."));
    }
}

void audio_output::unpause()
{
    alSourcePlay(_source);
    if (alGetError() != AL_NO_ERROR)
    {
        throw exc(_("Cannot unpause OpenAL source playback."));
    }
}

void audio_output::stop()
{
    alSourceStop(_source);
    if (alGetError() != AL_NO_ERROR)
    {
        throw exc(_("Cannot stop OpenAL source playback."));
    }
    // flush all buffers and reset the state
    ALint processed_buffers;
    alGetSourcei(_source, AL_BUFFERS_PROCESSED, &processed_buffers);
    while (processed_buffers > 0)
    {
        ALuint buf = 0;
        alSourceUnqueueBuffers(_source, 1, &buf);
        if (alGetError() != AL_NO_ERROR)
        {
            throw exc(_("Cannot unqueue OpenAL source buffers."));
        }
        alGetSourcei(_source, AL_BUFFERS_PROCESSED, &processed_buffers);
    }
    _state = 0;
}
