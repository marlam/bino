/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010, 2011, 2012
 * Martin Lambers <marlam@marlam.de>
 * Alexey Osipov <lion-simba@pridelands.ru>
 * Joe <cuchac@email.cz>
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
#include <unistd.h>

#include "gettext.h"
#define _(string) gettext(string)

#include "exc.h"
#include "str.h"
#include "msg.h"
#include "timer.h"

#include "controller.h"
#include "media_data.h"
#include "media_input.h"
#include "audio_output.h"
#include "video_output_qt.h"
#include "player.h"


player_init_data::player_init_data() :
    dev_request(), urls(), params()
{
}

player_init_data::~player_init_data()
{
}

void player_init_data::save(std::ostream &os) const
{
    s11n::save(os, dev_request);
    s11n::save(os, urls);
    s11n::save(os, params);
}

void player_init_data::load(std::istream &is)
{
    s11n::load(is, dev_request);
    s11n::load(is, urls);
    s11n::load(is, params);
}


player::player(type t) :
    _media_input(NULL), _audio_output(NULL), _video_output(NULL)
{
    if (t == master)
    {
        make_master();
    }
    reset_playstate();
}

player::~player()
{
    if (dispatch::get_global_player() == this)
    {
        dispatch::set_global_player(NULL);
    }
    delete _media_input;
    delete _audio_output;
    delete _video_output;
}

float player::normalize_pos(int64_t pos) const
{
    double pos_min = _start_pos + _media_input->initial_skip();
    double pos_max = pos_min + _media_input->duration();
    double npos = (pos_max > pos_min ? (static_cast<double>(pos) - pos_min) / (pos_max - pos_min) : 0.0f);
    return npos;
}

void player::reset_playstate()
{
    _running = false;
    _first_frame = false;
    _need_frame_now = false;
    _need_frame_soon = false;
    _drop_next_frame = false;
    _previous_frame_dropped = false;
    _in_pause = false;
    _quit_request = false;
    _pause_request = false;
    _seek_request = 0;
    _set_pos_request = -1.0f;
    _video_frame = video_frame();
    _current_subtitle_box = subtitle_box();
    _next_subtitle_box = subtitle_box();
}

void player::stop_playback()
{
    if (_video_output)
    {
        _video_output->exit_fullscreen();
        _video_output->prepare_next_frame(video_frame(), subtitle_box());
        _video_output->activate_next_frame();
        if (_audio_output)
        {
            _audio_output->stop();
        }
    }
    dispatch::set_playing(false);
}

video_output *player::create_video_output()
{
    return new video_output_qt(_swap_interval);
}

void player::destroy_video_output(video_output *vo)
{
    delete vo;
}

audio_output *player::create_audio_output()
{
    return new audio_output();
}

void player::destroy_audio_output(audio_output *ao)
{
    delete ao;
}

void player::make_master()
{
    dispatch::set_global_player(this);
}

void player::open(const player_init_data &init_data)
{
    // Initialize basics
    _benchmark = init_data.params.benchmark();
    _swap_interval = init_data.params.swap_interval();
    reset_playstate();

    // Create media input
    _media_input = new media_input();
    _media_input->open(init_data.urls, init_data.dev_request);
    if (_media_input->video_streams() == 0)
    {
        throw exc(_("No video streams found."));
    }
    if (init_data.params.stereo_layout_is_set() || init_data.params.stereo_layout_swap_is_set())
    {
        if (!_media_input->stereo_layout_is_supported(init_data.params.stereo_layout(), init_data.params.stereo_layout_swap()))
        {
            throw exc(_("Cannot set requested stereo layout: incompatible media."));
        }
        _media_input->set_stereo_layout(init_data.params.stereo_layout(), init_data.params.stereo_layout_swap());
    }
    if (_media_input->video_streams() < init_data.params.video_stream() + 1)
    {
        throw exc(str::asprintf(_("Video stream %d not found."), init_data.params.video_stream() + 1));
    }
    _media_input->select_video_stream(init_data.params.video_stream());
    if (_media_input->audio_streams() > 0 && _media_input->audio_streams() < init_data.params.audio_stream() + 1)
    {
        throw exc(str::asprintf(_("Audio stream %d not found."), init_data.params.audio_stream() + 1));
    }
    if (_media_input->audio_streams() > 0)
    {
        _media_input->select_audio_stream(init_data.params.audio_stream());
    }
    if (_media_input->subtitle_streams() > 0 && _media_input->subtitle_streams() < init_data.params.subtitle_stream() + 1)
    {
        throw exc(str::asprintf(_("Subtitle stream %d not found."), init_data.params.subtitle_stream() + 1));
    }
    if (_media_input->subtitle_streams() > 0 && init_data.params.subtitle_stream() >= 0)
    {
        _media_input->select_subtitle_stream(init_data.params.subtitle_stream());
    }

    // Create audio output
    if (_media_input->audio_streams() > 0 && !_benchmark)
    {
        _audio_output = create_audio_output();
    }
    if (_audio_output)
    {
        _audio_output->init(init_data.params.audio_device());
    }

    // Create video output
    _video_output = create_video_output();
    if (_video_output)
    {
        _video_output->init();
    }

    // Initialize output parameters
    _params = init_data.params;
    if (!init_data.params.stereo_mode_is_set() && !init_data.params.stereo_mode_swap_is_set())
    {
        if (_media_input->video_frame_template().stereo_layout == parameters::layout_mono)
        {
            _params.set_stereo_mode(parameters::mode_mono_left);
        }
        else if (_video_output && _video_output->supports_stereo())
        {
            _params.set_stereo_mode(parameters::mode_stereo);
        }
        else
        {
            _params.set_stereo_mode(parameters::mode_red_cyan_dubois);
        }
        _params.set_stereo_mode_swap(false);
    }

    // Set initial parameters
    if (_video_output)
    {
        _video_output->set_parameters(_params);
        _video_output->set_suitable_size(
                _media_input->video_frame_template().width,
                _media_input->video_frame_template().height,
                _params.crop_aspect_ratio() > 0.0f
                ? _params.crop_aspect_ratio()
                : _media_input->video_frame_template().aspect_ratio,
                _params.stereo_mode());
        if (_params.fullscreen())
        {
            _video_output->enter_fullscreen();
        }
        if (_params.center())
        {
            _video_output->center();
        }
        dispatch::process_all_events();
    }
}

void player::set_current_subtitle_box()
{
    _current_subtitle_box = subtitle_box();
    if (_next_subtitle_box.is_valid()
            && _next_subtitle_box.presentation_start_time < _video_pos + _media_input->video_frame_duration())
    {
        _current_subtitle_box = _next_subtitle_box;
    }
}

int64_t player::step(bool *more_steps, int64_t *seek_to, bool *prep_frame, bool *drop_frame, bool *display_frame)
{
    *more_steps = false;
    *seek_to = -1;
    *prep_frame = false;
    *drop_frame = false;
    *display_frame = false;

    if (_quit_request)
    {
        stop_playback();
        return 0;
    }
    else if (!_running)
    {
        // Read initial data and start output
        _media_input->start_video_frame_read();
        _video_frame = _media_input->finish_video_frame_read();
        if (!_video_frame.is_valid())
        {
            msg::dbg("Empty video input.");
            stop_playback();
            return 0;
        }
        _video_pos = _video_frame.presentation_time;
        if (_media_input->selected_subtitle_stream() >= 0)
        {
            do
            {
                _media_input->start_subtitle_box_read();
                _next_subtitle_box = _media_input->finish_subtitle_box_read();
                if (!_next_subtitle_box.is_valid())
                {
                    msg::dbg("Empty subtitle stream.");
                    stop_playback();
                    return 0;
                }
            }
            while (_next_subtitle_box.presentation_stop_time < _video_pos);
        }
        if (_audio_output)
        {
            _media_input->start_audio_blob_read(_audio_output->required_initial_data_size());
            audio_blob blob = _media_input->finish_audio_blob_read();
            if (!blob.is_valid())
            {
                msg::dbg("Empty audio input.");
                stop_playback();
                return 0;
            }
            _audio_pos = blob.presentation_time;
            _audio_output->data(blob);
            _media_input->start_audio_blob_read(_audio_output->required_update_data_size());
            _master_time_start = _audio_output->start();
            _master_time_pos = _audio_pos;
            _current_pos = _audio_pos;
        }
        else
        {
            _master_time_start = timer::get_microseconds(timer::monotonic);
            _master_time_pos = _video_pos;
            _current_pos = _video_pos;
        }
        _start_pos = _current_pos;
        _fps_mark_time = timer::get_microseconds(timer::monotonic);
        _frames_shown = 0;
        _running = true;
        if (_media_input->initial_skip() > 0)
        {
            _seek_request = _media_input->initial_skip();
        }
        else
        {
            _need_frame_now = false;
            _need_frame_soon = true;
            _first_frame = true;
            *more_steps = true;
            *prep_frame = true;
            set_current_subtitle_box();
            return 0;
        }
    }
    if (_seek_request != 0 || _set_pos_request >= 0.0f)
    {
        if (_set_pos_request >= 0.0f)
        {
            int64_t dest_pos_min = _start_pos + _media_input->initial_skip();
            int64_t dest_pos_max = dest_pos_min + _media_input->duration() - 2000000;
            if (dest_pos_max <= dest_pos_min)
            {
                *seek_to = _current_pos;
            }
            else
            {
                *seek_to = static_cast<double>(_set_pos_request) * dest_pos_max
                    + (1.0 - static_cast<double>(_set_pos_request)) * dest_pos_min;
            }
        }
        else
        {
            if (_current_pos + _seek_request < _start_pos + _media_input->initial_skip())
            {
                _seek_request = _start_pos + _media_input->initial_skip() - _current_pos;
            }
            if (_media_input->duration() > 0)
            {
                if (_seek_request > 0 && _current_pos + _seek_request >= std::max(_start_pos + _media_input->duration() - 2000000, static_cast<int64_t>(0)))
                {
                    _seek_request = std::max(_start_pos + _media_input->duration() - 2000000 - _current_pos, static_cast<int64_t>(0));
                }
            }
            *seek_to = _current_pos + _seek_request;
        }
        _seek_request = 0;
        _set_pos_request = -1.0f;
        _media_input->seek(*seek_to);
        _next_subtitle_box = subtitle_box();
        _current_subtitle_box = subtitle_box();

        _media_input->start_video_frame_read();
        _video_frame = _media_input->finish_video_frame_read();
        if (!_video_frame.is_valid())
        {
            msg::dbg("Seeked to end of video?!");
            stop_playback();
            return 0;
        }
        _video_pos = _video_frame.presentation_time;
        if (_media_input->selected_subtitle_stream() >= 0)
        {
            do
            {
                _media_input->start_subtitle_box_read();
                _next_subtitle_box = _media_input->finish_subtitle_box_read();
                if (!_next_subtitle_box.is_valid())
                {
                    msg::dbg("Seeked to end of subtitle?!");
                    stop_playback();
                    return 0;
                }
            }
            while (_next_subtitle_box.presentation_stop_time < _video_pos);
        }
        if (_audio_output)
        {
            _audio_output->stop();
            _media_input->start_audio_blob_read(_audio_output->required_initial_data_size());
            audio_blob blob = _media_input->finish_audio_blob_read();
            if (!blob.is_valid())
            {
                msg::dbg("Seeked to end of audio?!");
                stop_playback();
                return 0;
            }
            _audio_pos = blob.presentation_time;
            _audio_output->data(blob);
            _media_input->start_audio_blob_read(_audio_output->required_update_data_size());
            _master_time_start = _audio_output->start();
            _master_time_pos = _audio_pos;
            _current_pos = _audio_pos;
        }
        else
        {
            _master_time_start = timer::get_microseconds(timer::monotonic);
            _master_time_pos = _video_pos;
            _current_pos = _video_pos;
        }
        dispatch::set_position(normalize_pos(_current_pos));
        _need_frame_now = false;
        _need_frame_soon = true;
        _previous_frame_dropped = false;
        *more_steps = true;
        *prep_frame = true;
        set_current_subtitle_box();
        return 0;
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
            dispatch::set_pausing(true);
        }
        *more_steps = true;
        return 1000;    // allow some sleep in pause mode
    }
    else if (_need_frame_now)
    {
        _video_frame = _media_input->finish_video_frame_read();
        if (!_video_frame.is_valid())
        {
            if (_first_frame)
            {
                msg::dbg("Single-frame video input: going into pause mode.");
                _pause_request = true;
            }
            else
            {
                msg::dbg("End of video stream.");
                if (_params.loop_mode() == parameters::loop_current)
                {
                    _set_pos_request = 0.0f;
                    *more_steps = true;
                }
                else
                {
                    stop_playback();
                }
                return 0;
            }
        }
        else
        {
            _first_frame = false;
        }
        _video_pos = _video_frame.presentation_time;
        if (_media_input->selected_subtitle_stream() >= 0)
        {
            while (_next_subtitle_box.is_valid()
                    && _next_subtitle_box.presentation_stop_time < _video_pos)
            {
                _next_subtitle_box = _media_input->finish_subtitle_box_read();
                _media_input->start_subtitle_box_read();
                // If the box is invalid, we reached the end of the subtitle stream.
                // Ignore this and let audio/video continue.
            }
        }
        if (!_audio_output)
        {
            _master_time_start += (_video_pos - _master_time_pos);
            _master_time_pos = _video_pos;
            _current_pos = _video_pos;
            dispatch::set_position(normalize_pos(_current_pos));
        }
        _need_frame_now = false;
        _need_frame_soon = true;
        if (_drop_next_frame)
        {
            *drop_frame = true;
        }
        else if (!_pause_request)
        {
            *prep_frame = true;
            set_current_subtitle_box();
        }
        *more_steps = true;
        return 0;
    }
    else if (_need_frame_soon)
    {
        _media_input->start_video_frame_read();
        _need_frame_soon = false;
        *more_steps = true;
        return 0;
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
                _master_time_start += timer::get_microseconds(timer::monotonic) - _pause_start;
            }
            _in_pause = false;
            dispatch::set_pausing(false);
        }

        if (_audio_output)
        {
            // Check if audio needs more data, and get audio time
            bool need_audio_data;
            _master_time_current = _audio_output->status(&need_audio_data) - _master_time_start + _master_time_pos;
            // Output requested audio data
            if (need_audio_data)
            {
                audio_blob blob = _media_input->finish_audio_blob_read();
                if (!blob.is_valid())
                {
                    msg::dbg("End of audio stream.");
                    if (_params.loop_mode() == parameters::loop_current)
                    {
                        _set_pos_request = 0.0f;
                        *more_steps = true;
                    }
                    else
                    {
                        stop_playback();
                    }
                    return 0;
                }
                _audio_pos = blob.presentation_time;
                _master_time_start += (_audio_pos - _master_time_pos);
                _master_time_pos = _audio_pos;
                _audio_output->data(blob);
                _media_input->start_audio_blob_read(_audio_output->required_update_data_size());
                _current_pos = _audio_pos;
                dispatch::set_position(normalize_pos(_current_pos));
            }
        }
        else
        {
            // Use our own timer
            _master_time_current = timer::get_microseconds(timer::monotonic) - _master_time_start
                + _master_time_pos;
        }

        int64_t allowable_sleep = 0;
        if (_master_time_current + _params.audio_delay() >= _video_pos ||
            _benchmark || _media_input->is_device())
        {
            // Output current video frame
            _drop_next_frame = false;
            const int64_t delay = _master_time_current + _params.audio_delay() - _video_pos;
            if ( delay > _media_input->video_frame_duration() * 75 / 100 &&
                 !_benchmark && !_media_input->is_device())
            {
                msg::wrn(_("Video: delay %g seconds/%g frames; dropping next frame."),
                         float(delay) / 1e6f, 
                         float(delay) / float(_media_input->video_frame_duration()));
                _drop_next_frame = true;
            }
            if (!_previous_frame_dropped)
            {
                *display_frame = true;
                if (_benchmark)
                {
                    _frames_shown++;
                    if (_frames_shown == 100)   //show fps each 100 frames
                    {
                        int64_t now = timer::get_microseconds(timer::monotonic);
                        msg::inf(_("FPS: %.2f"), static_cast<float>(_frames_shown) / ((now - _fps_mark_time) / 1e6f));
                        _fps_mark_time = now;
                        _frames_shown = 0;
                    }
                }
            }
            _need_frame_now = true;
            _need_frame_soon = false;
            _previous_frame_dropped = _drop_next_frame;
        }
        else
        {
            allowable_sleep = _video_pos - _master_time_current;
            if (allowable_sleep < 100)
            {
                allowable_sleep = 0;
            }
            else if (allowable_sleep > 1100)
            {
                allowable_sleep = 1100;
            }
            allowable_sleep -= 100;
        }
        *more_steps = true;
        return allowable_sleep;
    }
}

bool player::run_step()
{
    bool more_steps;
    int64_t seek_to;
    bool prep_frame;
    bool drop_frame;
    bool display_frame;
    int64_t allowed_sleep;

    allowed_sleep = step(&more_steps, &seek_to, &prep_frame, &drop_frame, &display_frame);

    if (!more_steps)
    {
        return false;
    }
    if (prep_frame)
    {
        if (_current_subtitle_box.is_valid() && _video_output)
        {
            _master_time_start += _video_output->wait_for_subtitle_renderer();
        }
        _video_output->prepare_next_frame(_video_frame, _current_subtitle_box);
    }
    else if (drop_frame)
    {
    }
    else if (display_frame)
    {
        _video_output->activate_next_frame();
    }

    dispatch::process_all_events();
    if (allowed_sleep > 0)
    {
        usleep(allowed_sleep);
    }

    return true;
}

void player::run()
{
    dispatch::set_playing(true);
    while (run_step());
}

void player::close()
{
    reset_playstate();
    if (_audio_output)
    {
        try { _audio_output->deinit(); } catch (...) {}
        destroy_audio_output(_audio_output);
        _audio_output = NULL;
    }
    if (_video_output)
    {
        try { _video_output->deinit(); } catch (...) {}
        destroy_video_output(_video_output);
        _video_output = NULL;
    }
    if (_media_input)
    {
        try { _media_input->close(); } catch (...) {}
        delete _media_input;
        _media_input = NULL;
    }
}

void player::quit_request()
{
    _quit_request = true;
}

void player::set_pause(bool p)
{
    _pause_request = p;
}

void player::seek(int64_t offset)
{
    _seek_request = offset;
}

void player::set_pos(float pos)
{
    _set_pos_request = pos;
}

int player::set_video_stream(int s)
{
    if (s < 0 || s >= _media_input->video_streams())
        s = 0;
    _media_input->select_video_stream(s);
    _seek_request = -1; // Get position right
    return s;
}

int player::set_audio_stream(int s)
{
    if (s < 0 || s >= _media_input->audio_streams())
        s = 0;
    _media_input->select_audio_stream(s);
    _seek_request = -1; // Get position right
    return s;
}

int player::set_subtitle_stream(int s)
{
    if (s < -1 || s >= _media_input->subtitle_streams())
        s = -1;
    _media_input->select_subtitle_stream(s);
    _seek_request = -1; // Get position right
    return s;
}

void player::set_stereo_layout(parameters::stereo_layout_t stereo_layout)
{
    _media_input->set_stereo_layout(stereo_layout, dispatch::parameters().stereo_layout_swap());
    if (stereo_layout == parameters::layout_separate)
        _seek_request = -1;     // Get position of both streams right
}

void player::set_stereo_layout_swap(bool swap)
{
    _media_input->set_stereo_layout(dispatch::parameters().stereo_layout(), swap);
}

bool player::set_fullscreen(bool fs)
{
    bool r = false;
    if (_video_output) {
        if (fs)
            _video_output->enter_fullscreen();
        else
            _video_output->exit_fullscreen();
        r = fs;
    }
    return r;
}

void player::center()
{
    if (_video_output)
        _video_output->center();
}

void player::trigger_video_output_update()
{
    //TODO: remove this!
    if (_video_output)
        _video_output->set_parameters(dispatch::parameters());
}

float player::get_pos() const
{
    return normalize_pos(_current_pos);
}
