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

#include <vector>

#include "exc.h"
#include "str.h"
#include "msg.h"
#include "timer.h"

#include "decoder_ffmpeg.h"
#include "input.h"
#include "controller.h"
#include "audio_output_openal.h"
#include "video_output_opengl_qt.h"
#include "player.h"


player_init_data::player_init_data()
    : log_level(msg::INF), benchmark(false), filenames(),
    input_mode(input::automatic),
    video_mode(video_output::stereo),
    video_state(),
    video_flags(0)
{
}

player_init_data::~player_init_data()
{
}


// The single player instance
player *global_player = NULL;

// The registered controllers
std::vector<controller *> global_controllers;

player::player(type t)
    : _input(NULL), _audio_output(NULL), _video_output(NULL)
{
    if (t == master)
    {
        make_master();
    }
    reset_playstate();
}

player::~player()
{
    if (global_player == this)
    {
        global_player = NULL;
    }
}

void player::reset_playstate()
{
    _running = false;
    _first_frame = false;
    _need_frame = false;
    _drop_next_frame = false;
    _previous_frame_dropped = false;
    _in_pause = false;
    _quit_request = false;
    _pause_request = false;
    _seek_request = 0;
}

void player::create_decoders(const std::vector<std::string> &filenames)
{
    int video_streams = 0;
    int audio_streams = 0;
    for (size_t i = 0; i < filenames.size(); i++)
    {
        _decoders.push_back(new decoder_ffmpeg);
        _decoders[i]->open(filenames[i].c_str());
        video_streams += _decoders[i]->video_streams();
        audio_streams += _decoders[i]->audio_streams();
    }
    if (video_streams == 0)
    {
        throw exc("no video streams found");
    }
    if (video_streams > 2)
    {
        throw exc("cannot handle more than 2 video streams");
    }
}

void player::create_input(enum input::mode input_mode)
{
    int video_decoder[2] = { -1, -1 };
    int video_stream[2] = { -1, -1 };
    int audio_decoder = -1;
    int audio_stream = -1;
    for (size_t i = 0; i < _decoders.size(); i++)
    {
        for (int j = 0; j < _decoders[i]->video_streams(); j++)
        {
            if (video_decoder[0] != -1 && video_decoder[1] != -1)
            {
                continue;
            }
            else if (video_decoder[0] != -1)
            {
                video_decoder[1] = i;
                video_stream[1] = j;
            }
            else
            {
                video_decoder[0] = i;
                video_stream[0] = j;
            }
        }
        for (int j = 0; j < _decoders[i]->audio_streams(); j++)
        {
            if (audio_decoder != -1)
            {
                continue;
            }
            else
            {
                audio_decoder = i;
                audio_stream = j;
            }
        }
    }
    _input = new input();
    _input->open(_decoders,
            video_decoder[0], video_stream[0],
            video_decoder[1], video_stream[1],
            audio_decoder, audio_stream,
            input_mode);
}

void player::get_input_info(int *w, int *h, float *ar, enum decoder::video_frame_format *fmt)
{
    *w = _input->video_width();
    *h = _input->video_height();
    *ar = _input->video_aspect_ratio();
    *fmt = _input->video_preferred_frame_format();
}

void player::create_audio_output()
{
    if (_input->has_audio() && !_benchmark)
    {
        _audio_output = new audio_output_openal();
        _audio_output->open(_input->audio_channels(), _input->audio_rate(), _input->audio_sample_format());
    }
}

void player::create_video_output()
{
    _video_output = new video_output_opengl_qt();
}

void player::open_video_output(enum video_output::mode video_mode, unsigned int video_flags)
{
    if (video_mode == video_output::automatic)
    {
        if (_input->mode() == input::mono)
        {
            video_mode = video_output::mono_left;
        }
        else if (_video_output->supports_stereo())
        {
            video_mode = video_output::stereo;
        }
        else
        {
            video_mode = video_output::anaglyph_red_cyan_dubois;
        }
    }
    _video_output->open(
            _input->video_preferred_frame_format(),
            _input->video_width(), _input->video_height(), _input->video_aspect_ratio(),
            video_mode, _video_state, video_flags, -1, -1);
    _video_output->process_events();
}

void player::make_master()
{
    if (global_player)
    {
        throw exc("cannot create a second master player");
    }
    global_player = this;
}

void player::run_step(bool *more_steps, int64_t *seek_to, bool *prep_frame, bool *drop_frame, bool *display_frame)
{
    *more_steps = false;
    *seek_to = -1;
    *prep_frame = false;
    *drop_frame = false;
    *display_frame = false;

    if (_quit_request)
    {
        notify(notification::play, true, false);
        return;
    }
    else if (!_running)
    {
        // Read initial data and start output
        if (_input->read_video_frame() < 0)
        {
            msg::dbg("empty video input");
            notify(notification::play, true, false);
            return;
        }
        _current_pos = 0;
        _next_frame_pos = 0;
        _sync_point_pos = 0;
        _sync_point_av_offset = 0;
        if (_audio_output)
        {
            _audio_output->status(&_required_audio_data_size);
            if (_input->read_audio_data(&_audio_data, _required_audio_data_size) < 0)
            {
                msg::dbg("empty audio input");
                notify(notification::play, true, false);
                return;
            }
            _audio_output->data(_audio_data, _required_audio_data_size);
            _sync_point_time = _audio_output->start();
        }
        else
        {
            _sync_point_time = timer::get_microseconds(timer::monotonic);
        }
        _fps_mark_time = _sync_point_time;
        _frames_shown = 0;
        _running = true;
        _need_frame = false;
        _first_frame = true;
        *more_steps = true;
        *prep_frame = true;
        return;
    }
    else if (_seek_request != 0)
    {
        if (_seek_request < 0 && -_seek_request > _current_pos)
        {
            _seek_request = -_current_pos;
        }
        if (_input->duration() > 0)
        {
            if (_seek_request > 0 && _current_pos + _seek_request >= std::max(_input->duration() - 5000000, static_cast<int64_t>(0)))
            {
                _seek_request = std::max(_input->duration() - 5000000 - _current_pos, static_cast<int64_t>(0));
            }
        }
        *seek_to = _current_pos + _seek_request;
        _seek_request = 0;
        _input->seek(*seek_to);
        _next_frame_pos = _input->read_video_frame();
        if (_next_frame_pos < 0)
        {
            msg::wrn("seeked to end of video?!");
            notify(notification::play, true, false);
            return;
        }
        else
        {
            if (_audio_output)
            {
                _audio_output->stop();
                _audio_output->status(&_required_audio_data_size);
                int64_t audio_pos = _input->read_audio_data(&_audio_data, _required_audio_data_size);
                if (audio_pos < 0)
                {
                    msg::wrn("seeked to end of audio?!");
                    notify(notification::play, true, false);
                    return;
                }
                _audio_output->data(_audio_data, _required_audio_data_size);
                _sync_point_time = _audio_output->start();
                _sync_point_pos = audio_pos;
                _sync_point_av_offset = _next_frame_pos - audio_pos;
            }
            else
            {
                _sync_point_pos = _next_frame_pos;
                _sync_point_time = timer::get_microseconds(timer::monotonic);
                _sync_point_av_offset = 0;
            }
            _fps_mark_time = _sync_point_time;
            _frames_shown = 0;
            notify(notification::pos, _current_pos / 1e6f, _sync_point_pos / 1e6f);
            _need_frame = false;
            _current_pos = _sync_point_pos;
            _previous_frame_dropped = false;
            *more_steps = true;
            *prep_frame = true;
            return;
        }
    }
    else if (_pause_request)
    {
        if (!_in_pause)
        {
            if (_audio_output)
            {
                _audio_output->pause();
            }
            else
            {
                _pause_start = timer::get_microseconds(timer::monotonic);
            }
            _in_pause = true;
            notify(notification::pause, false, true);
        }
        *more_steps = true;
        return;
    }
    else if (_need_frame)
    {
        _next_frame_pos = _input->read_video_frame();
        if (_next_frame_pos < 0)
        {
            if (_first_frame)
            {
                msg::dbg("single-frame video input: going into pause mode");
                _pause_request = true;
            }
            else
            {
                msg::dbg("end of video stream");
                notify(notification::play, true, false);
                return;
            }
        }
        else
        {
            _first_frame = false;
        }
        _need_frame = false;
        if (_drop_next_frame)
        {
            *drop_frame = true;
        }
        else if (!_pause_request)
        {
            *prep_frame = true;
        }
        *more_steps = true;
        return;
    }
    else
    {
        if (_in_pause)
        {
            if (_audio_output)
            {
                _audio_output->unpause();
            }
            else
            {
                _sync_point_time += timer::get_microseconds(timer::monotonic) - _pause_start;
                _fps_mark_time = _sync_point_time;
                _frames_shown = 0;
            }
            _in_pause = false;
            notify(notification::pause, true, false);
        }

        if (_audio_output)
        {
            // Check if audio needs more data, and get audio time
            _current_time = _audio_output->status(&_required_audio_data_size);
            // Output requested audio data
            if (_required_audio_data_size > 0)
            {
                if (_input->read_audio_data(&_audio_data, _required_audio_data_size) < 0)
                {
                    msg::dbg("end of audio stream");
                    notify(notification::play, true, false);
                    return;
                }
                _audio_output->data(_audio_data, _required_audio_data_size);
            }
        }
        else
        {
            // Use our own timer
            _current_time = timer::get_microseconds(timer::monotonic);
        }

        int64_t time_to_next_frame = (_next_frame_pos - _sync_point_pos) - (_current_time - _sync_point_time) - _sync_point_av_offset;
        if (time_to_next_frame <= 0 || _benchmark)
        {
            // Output current video frame
            _drop_next_frame = false;
            if (-time_to_next_frame > _input->video_frame_duration() * 75 / 100 && !_benchmark)
            {
                msg::wrn("video: delay %g seconds; dropping next frame", (-time_to_next_frame) / 1e6f);
                _drop_next_frame = true;
            }
            if (!_previous_frame_dropped)
            {
                *display_frame = true;
                if (_benchmark)
                {
                    _frames_shown++;
                    if (_frames_shown % 100 == 0) //show fps each 100 frames
                    {
                        msg::inf("fps: %.2f", 100.0f / ((_current_time - _fps_mark_time) / 1e6));
                        _fps_mark_time = _current_time;
                        _frames_shown = 0;
                    }
                }
            }
            _need_frame = true;
            _current_pos = _next_frame_pos;
            _previous_frame_dropped = _drop_next_frame;
        }
        *more_steps = true;
        return;
    }
}

void player::get_video_frame(enum decoder::video_frame_format fmt)
{
    _input->get_video_frame(fmt, _l_data, _l_line_size, _r_data, _r_line_size);
}

void player::prepare_video_frame(video_output *vo)
{
    vo->prepare(_l_data, _l_line_size, _r_data, _r_line_size);
}

void player::release_video_frame()
{
    _input->release_video_frame();
}

void player::open(const player_init_data &init_data)
{
    msg::set_level(init_data.log_level);
    set_benchmark(init_data.benchmark);
    reset_playstate();
    create_decoders(init_data.filenames);
    create_input(init_data.input_mode);
    create_audio_output();
    create_video_output();
    _video_state = init_data.video_state;
    open_video_output(init_data.video_mode, init_data.video_flags);
}

enum input::mode player::input_mode() const
{
    return _input->mode();
}

enum video_output::mode player::video_mode() const
{
    return _video_output->mode();
}

void player::close()
{
    reset_playstate();
    if (_audio_output)
    {
        try { _audio_output->close(); } catch (...) {}
        delete _audio_output;
        _audio_output = NULL;
    }
    if (_video_output)
    {
        try { _video_output->close(); } catch (...) {}
        delete _video_output;
        _video_output = NULL;
    }
    if (_input)
    {
        try { _input->close(); } catch (...) {}
        delete _input;
        _input = NULL;
    }
    for (size_t i = 0; i < _decoders.size(); i++)
    {
        try { _decoders[i]->close(); } catch (...) {}
        delete _decoders[i];
    }
    _decoders.resize(0);
}

void player::run()
{
    bool more_steps;
    int64_t seek_to;
    bool prep_frame;
    bool drop_frame;
    bool display_frame;

    for (;;)
    {
        run_step(&more_steps, &seek_to, &prep_frame, &drop_frame, &display_frame);
        if (!more_steps)
        {
            break;
        }
        if (prep_frame)
        {
            get_video_frame(_video_output->frame_format());
            prepare_video_frame(_video_output);
            release_video_frame();
        }
        else if (drop_frame)
        {
            release_video_frame();
        }
        else if (display_frame)
        {
            _video_output->activate();
        }
        _video_output->process_events();
    }
}

void player::receive_cmd(const command &cmd)
{
    float oldval;

    switch (cmd.type)
    {
    case command::toggle_play:
        _quit_request = true;
        /* notify when request is fulfilled */
        break;
    case command::toggle_swap_eyes:
        _video_state.swap_eyes = !_video_state.swap_eyes;
        notify(notification::swap_eyes, !_video_state.swap_eyes, _video_state.swap_eyes);
        break;
    case command::toggle_fullscreen:
        _video_state.fullscreen = !_video_state.fullscreen;
        notify(notification::fullscreen, !_video_state.fullscreen, _video_state.fullscreen);
        break;
    case command::center:
        notify(notification::center);
        break;
    case command::toggle_pause:
        _pause_request = !_pause_request;
        /* notify when request is fulfilled */
        break;
    case command::adjust_contrast:
        oldval = _video_state.contrast;
        _video_state.contrast = std::max(std::min(_video_state.contrast + cmd.param, 1.0f), -1.0f);
        notify(notification::contrast, oldval, _video_state.contrast);
        break;
    case command::adjust_brightness:
        oldval = _video_state.brightness;
        _video_state.brightness = std::max(std::min(_video_state.brightness + cmd.param, 1.0f), -1.0f);
        notify(notification::brightness, oldval, _video_state.brightness);
        break;
    case command::adjust_hue:
        oldval = _video_state.hue;
        _video_state.hue = std::max(std::min(_video_state.hue + cmd.param, 1.0f), -1.0f);
        notify(notification::hue, oldval, _video_state.hue);
        break;
    case command::adjust_saturation:
        oldval = _video_state.saturation;
        _video_state.saturation = std::max(std::min(_video_state.saturation + cmd.param, 1.0f), -1.0f);
        notify(notification::saturation, oldval, _video_state.saturation);
        break;
    case command::seek:
        _seek_request = cmd.param * 1e6f;
        /* notify when request is fulfilled */
        break;
    }
}

void player::notify(const notification &note)
{
    // Notify all controllers
    for (size_t i = 0; i < global_controllers.size(); i++)
    {
        global_controllers[i]->receive_notification(note);
    }
}
