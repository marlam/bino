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

#include <string>

#include "exc.h"
#include "str.h"
#include "msg.h"
#include "timer.h"
#include "dbg.h"

#include "audio_output_openal.h"

/* This code is adapted from the alffmpeg.c example available here:
 * http://kcat.strangesoft.net/alffmpeg.c (as of 2010-09-12). */

static std::vector<std::string> openal_version_vector;

static void set_openal_version_vector()
{
    openal_version_vector.push_back(std::string("OpenAL version ") + static_cast<const char *>(alGetString(AL_VERSION)));
    openal_version_vector.push_back(std::string("OpenAL renderer ") + static_cast<const char *>(alGetString(AL_RENDERER)));
    openal_version_vector.push_back(std::string("OpenAL vendor ") + static_cast<const char *>(alGetString(AL_VENDOR)));
}

const size_t audio_output_openal::_num_buffers = 3;
const size_t audio_output_openal::_buffer_size = 20160;

audio_output_openal::audio_output_openal() throw ()
    : audio_output(false)       // Currently no notifications supported
{
}

audio_output_openal::~audio_output_openal()
{
}

void audio_output_openal::open(int channels, int rate, enum decoder::audio_sample_format sample_format)
{
    _buffers.resize(_num_buffers);
    _data.resize(_buffer_size);

    if (!(_device = alcOpenDevice(NULL)))
    {
        throw exc("No OpenAL device available");
    }
    if (!(_context = alcCreateContext(_device, NULL)))
    {
        alcCloseDevice(_device);
        throw exc("No OpenAL context available");
    }
    alcMakeContextCurrent( _context);
    if (openal_version_vector.size() == 0)
    {
        set_openal_version_vector();
    }
    alGenBuffers(_num_buffers, &(_buffers[0]));
    if (alGetError() != AL_NO_ERROR)
    {
        alcMakeContextCurrent(NULL);
        alcDestroyContext(_context);
        alcCloseDevice(_device);
        throw exc("Cannot create OpenAL buffers");
    }
    alGenSources(1, &_source);
    if (alGetError() != AL_NO_ERROR)
    {
        alDeleteBuffers(_num_buffers, &(_buffers[0]));
        alcMakeContextCurrent(NULL);
        alcDestroyContext(_context);
        alcCloseDevice(_device);
        throw exc("Cannot create OpenAL source");
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
        throw exc("Cannot set OpenAL source parameters");
    }

    _format = 0;
    if (sample_format == decoder::audio_sample_u8)
    {
        if (channels == 1)
        {
            _format = AL_FORMAT_MONO8;
        }
        else if (channels == 2)
        {
            _format = AL_FORMAT_STEREO8;
        }
        else if (alIsExtensionPresent("AL_EXT_MCFORMATS"))
        {
            if (channels == 4)
            {
                _format = alGetEnumValue("AL_FORMAT_QUAD8");
            }
            else if (channels == 6)
            {
                _format = alGetEnumValue("AL_FORMAT_51CHN8");
            }
            else if (channels == 7)
            {
                _format = alGetEnumValue("AL_FORMAT_71CHN8");
            }
            else if (channels == 8)
            {
                _format = alGetEnumValue("AL_FORMAT_81CHN8");
            }
        }
    }
    else if (sample_format == decoder::audio_sample_s16)
    {
        if (channels == 1)
        {
            _format = AL_FORMAT_MONO16;
        }
        else if (channels == 2)
        {
            _format = AL_FORMAT_STEREO16;
        }
        else if (alIsExtensionPresent("AL_EXT_MCFORMATS"))
        {
            if (channels == 4)
            {
                _format = alGetEnumValue("AL_FORMAT_QUAD16");
            }
            else if (channels == 6)
            {
                _format = alGetEnumValue("AL_FORMAT_51CHN16");
            }
            else if (channels == 7)
            {
                _format = alGetEnumValue("AL_FORMAT_61CHN16");
            }
            else if (channels == 8)
            {
                _format = alGetEnumValue("AL_FORMAT_71CHN16");
            }
        }
    }
    else if (sample_format == decoder::audio_sample_f32)
    {
        if (alIsExtensionPresent("AL_EXT_float32"))
        {
            if (channels == 1)
            {
                _format = alGetEnumValue("AL_FORMAT_MONO_FLOAT32");
            }
            else if (channels == 2)
            {
                _format = alGetEnumValue("AL_FORMAT_STEREO_FLOAT32");
            }
            else if (alIsExtensionPresent("AL_EXT_MCFORMATS"))
            {
                if (channels == 4)
                {
                    _format = alGetEnumValue("AL_FORMAT_QUAD32");
                }
                else if (channels == 6)
                {
                    _format = alGetEnumValue("AL_FORMAT_51CHN32");
                }
                else if (channels == 7)
                {
                    _format = alGetEnumValue("AL_FORMAT_61CHN32");
                }
                else if (channels == 8)
                {
                    _format = alGetEnumValue("AL_FORMAT_71CHN32");
                }
            }
        }
    }
    else if (sample_format == decoder::audio_sample_d64)
    {
        if (alIsExtensionPresent("AL_EXT_double"))
        {
            if (channels == 1)
            {
                _format = alGetEnumValue("AL_FORMAT_MONO_DOUBLE_EXT");
            }
            else if (channels == 2)
            {
                _format = alGetEnumValue("AL_FORMAT_STEREO_DOUBLE_EXT");
            }
        }
    }
    if (_format == 0)
    {
        alDeleteSources(1, &_source);
        alDeleteBuffers(_num_buffers, &(_buffers[0]));
        alcMakeContextCurrent(NULL);
        alcDestroyContext(_context);
        alcCloseDevice(_device);
        throw exc(std::string("Cannot set OpenAL format for source with ")
                + str::from(channels) + " channels and sample format "
                + decoder::audio_sample_format_name(sample_format));
    }
    _state = 0;
    _rate = rate;
    _channels = channels;
    _bits = decoder::audio_sample_format_bits(sample_format);
}

int64_t audio_output_openal::status(size_t *required_data)
{
    if (_state == 0)
    {
        if (required_data)
        {
            *required_data = _num_buffers * _buffer_size;
        }
        return -1;
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
                throw exc("Cannot check OpenAL source state");
            }
            if (_state != AL_PLAYING)
            {
                alSourcePlay(_source);
                if (alGetError() != AL_NO_ERROR)
                {
                    throw exc("Cannot restart OpenAL source playback");
                }
            }
            if (required_data)
            {
                *required_data = 0;
            }
        }
        else
        {
            if (required_data)
            {
                *required_data = _buffer_size;
            }
        }
        ALint o;
        alGetSourcei(_source, AL_SAMPLE_OFFSET, &o);
        /* Add the base time to the offset. Each count of _basetime
         * represents one buffer, which is _buffer_size in bytes */
        int64_t offset = o + _basetime * (_buffer_size / _channels * 8 / _bits);
        int64_t timestamp = offset * 1000000 / _rate;
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

void audio_output_openal::data(const void *buffer, size_t size)
{
    msg::dbg(std::string("Buffering ") + str::from(size) + " bytes of audio data");
    if (_state == 0)
    {
        for (size_t j = 0; j < _num_buffers; j++)
        {
            alBufferData(_buffers[j], _format, buffer, _buffer_size, _rate);
            alSourceQueueBuffers(_source, 1, &(_buffers[j]));
            buffer = static_cast<const unsigned char *>(buffer) + _buffer_size;
        }
        if (alGetError() != AL_NO_ERROR)
        {
            throw exc("Cannot buffer initial OpenAL data");
        }
    }
    else if (size > 0)
    {
        ALuint buf = 0;
        alSourceUnqueueBuffers(_source, 1, &buf);
        if (buf != 0)
        {
            alBufferData(buf, _format, buffer, size, _rate);
            alSourceQueueBuffers(_source, 1, &buf);
            _basetime++;
        }
        if (alGetError() != AL_NO_ERROR)
        {
            throw exc("Cannot buffer OpenAL data");
        }
    }
}

int64_t audio_output_openal::start()
{
    msg::dbg("Starting audio output");
    assert(_state == 0);
    alSourcePlay(_source);
    alGetSourcei(_source, AL_SOURCE_STATE, &_state);
    if (alGetError() != AL_NO_ERROR)
    {
        throw exc("Cannot start OpenAL source playback");
    }
    _basetime = 0;
    _last_timestamp = 0;
    _ext_timer_at_last_timestamp = timer::get_microseconds(timer::monotonic);
    _last_reported_timestamp = _last_timestamp;
    return _last_timestamp;
}

void audio_output_openal::pause()
{
    alSourcePause(_source);
    if (alGetError() != AL_NO_ERROR)
    {
        throw exc("Cannot pause OpenAL source playback");
    }
}

void audio_output_openal::unpause()
{
    alSourcePlay(_source);
    if (alGetError() != AL_NO_ERROR)
    {
        throw exc("Cannot unpause OpenAL source playback");
    }
}

void audio_output_openal::stop()
{
    alSourceStop(_source);
    if (alGetError() != AL_NO_ERROR)
    {
        throw exc("Cannot stop OpenAL source playback");
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
            throw exc("Cannot unqueue OpenAL source buffers");
        }
        alGetSourcei(_source, AL_BUFFERS_PROCESSED, &processed_buffers);
    }
    _state = 0;
}

void audio_output_openal::close()
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
}

std::vector<std::string> openal_versions()
{
    if (openal_version_vector.size() == 0)
    {
        ALCdevice *device = alcOpenDevice(NULL);
        if (device)
        {
            ALCcontext *context = alcCreateContext(device, NULL);
            if (context)
            {
                alcMakeContextCurrent(context);
                set_openal_version_vector();
                alcMakeContextCurrent(NULL);
                alcDestroyContext(context);
            }
            alcCloseDevice(device);
        }
    }
    if (openal_version_vector.size() == 0)
    {
        std::vector<std::string> v;
        v.push_back(std::string("OpenAL unknown"));
        return v;
    }
    else
    {
        return openal_version_vector;
    }
}
