/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010, 2011, 2012, 2015
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

#include "base/exc.h"
#include "base/dbg.h"
#include "base/str.h"
#include "base/msg.h"
#include "base/tmr.h"

#include "base/gettext.h"
#define _(string) gettext(string)

#include "audio_output.h"
#include "video_output.h"
#include "media_input.h"
#include "player.h"


extern dispatch* global_dispatch;

player::player()
{
    reset_playstate();
}

player::~player()
{
}

float player::normalize_pos(int64_t pos) const
{
    double pos_min = _start_pos + global_dispatch->get_media_input()->initial_skip();
    double pos_max = pos_min + global_dispatch->get_media_input()->duration();
    if (pos_max > pos_min && pos >= pos_min && pos <= pos_max)
        return (pos - pos_min) / (pos_max - pos_min);
    else
        return 0.0f;
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
    _recently_seeked = false;
    _quit_request = false;
    _pause_request = false;
    _step_request = false;
    _seek_request = 0;
    _set_pos_request = -1.0f;
    _video_frame = video_frame();
    _current_subtitle_box = subtitle_box();
    _next_subtitle_box = subtitle_box();
}

void player::open()
{
    reset_playstate();
}

void player::close()
{
    reset_playstate();
}

void player::set_current_subtitle_box()
{
    _current_subtitle_box = subtitle_box();
    if (_next_subtitle_box.is_valid()
            && _next_subtitle_box.presentation_start_time < _video_pos + global_dispatch->get_media_input()->video_frame_duration())
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
        return 0;
    }
    else if (!_running)
    {
        // Read initial data and start output
        global_dispatch->get_media_input()->start_video_frame_read();
        _video_frame = global_dispatch->get_media_input()->finish_video_frame_read();
        if (!_video_frame.is_valid())
        {
            msg::dbg("Empty video input.");
            return 0;
        }
        _video_pos = _video_frame.presentation_time;
        if (global_dispatch->get_media_input()->selected_subtitle_stream() >= 0)
        {
            do
            {
                global_dispatch->get_media_input()->start_subtitle_box_read();
                _next_subtitle_box = global_dispatch->get_media_input()->finish_subtitle_box_read();
                if (!_next_subtitle_box.is_valid())
                {
                    msg::dbg("Empty subtitle stream.");
                    return 0;
                }
            }
            while (_next_subtitle_box.presentation_stop_time < _video_pos);
        }
        if (global_dispatch->get_audio_output()
                && global_dispatch->get_media_input()->selected_audio_stream() >= 0)
        {
            global_dispatch->get_media_input()->start_audio_blob_read(global_dispatch->get_audio_output()->required_initial_data_size());
            audio_blob blob = global_dispatch->get_media_input()->finish_audio_blob_read();
            if (!blob.is_valid())
            {
                msg::dbg("Empty audio input.");
                return 0;
            }
            _audio_pos = blob.presentation_time;
            global_dispatch->get_audio_output()->data(blob);
            global_dispatch->get_media_input()->start_audio_blob_read(global_dispatch->get_audio_output()->required_update_data_size());
            _master_time_start = global_dispatch->get_audio_output()->start();
            _master_time_pos = _audio_pos;
            _current_pos = _audio_pos;
        }
        else
        {
            _master_time_start = timer::get(timer::monotonic);
            _master_time_pos = _video_pos;
            _current_pos = _video_pos;
        }
        _start_pos = _current_pos;
        _fps_mark_time = timer::get(timer::monotonic);
        _frames_shown = 0;
        _running = true;
        if (global_dispatch->get_media_input()->initial_skip() > 0)
        {
            _seek_request = global_dispatch->get_media_input()->initial_skip();
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
            int64_t dest_pos_min = _start_pos + global_dispatch->get_media_input()->initial_skip();
            int64_t dest_pos_max = dest_pos_min + global_dispatch->get_media_input()->duration() - 2000000;
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
            if (_current_pos + _seek_request < _start_pos + global_dispatch->get_media_input()->initial_skip())
            {
                _seek_request = _start_pos + global_dispatch->get_media_input()->initial_skip() - _current_pos;
            }
            if (global_dispatch->get_media_input()->duration() > 0)
            {
                if (_seek_request > 0 && _current_pos + _seek_request >= std::max(_start_pos + global_dispatch->get_media_input()->duration() - 2000000, static_cast<int64_t>(0)))
                {
                    _seek_request = std::max(_start_pos + global_dispatch->get_media_input()->duration() - 2000000 - _current_pos, static_cast<int64_t>(0));
                }
            }
            *seek_to = _current_pos + _seek_request;
        }
        _seek_request = 0;
        _set_pos_request = -1.0f;
        global_dispatch->get_media_input()->seek(*seek_to);
        _next_subtitle_box = subtitle_box();
        _current_subtitle_box = subtitle_box();

        global_dispatch->get_media_input()->start_video_frame_read();
        _video_frame = global_dispatch->get_media_input()->finish_video_frame_read();
        if (!_video_frame.is_valid())
        {
            msg::dbg("Seeked to end of video?!");
            return 0;
        }
        _video_pos = _video_frame.presentation_time;
        if (global_dispatch->get_media_input()->selected_subtitle_stream() >= 0)
        {
            do
            {
                global_dispatch->get_media_input()->start_subtitle_box_read();
                _next_subtitle_box = global_dispatch->get_media_input()->finish_subtitle_box_read();
                if (!_next_subtitle_box.is_valid())
                {
                    msg::dbg("Seeked to end of subtitle?!");
                    return 0;
                }
            }
            while (_next_subtitle_box.presentation_stop_time < _video_pos);
        }
        if (global_dispatch->get_audio_output()
                && global_dispatch->get_media_input()->selected_audio_stream() >= 0)
        {
            global_dispatch->get_audio_output()->stop();
            global_dispatch->get_media_input()->start_audio_blob_read(global_dispatch->get_audio_output()->required_initial_data_size());
            audio_blob blob = global_dispatch->get_media_input()->finish_audio_blob_read();
            if (!blob.is_valid())
            {
                msg::dbg("Seeked to end of audio?!");
                return 0;
            }
            _audio_pos = blob.presentation_time;
            global_dispatch->get_audio_output()->data(blob);
            global_dispatch->get_media_input()->start_audio_blob_read(global_dispatch->get_audio_output()->required_update_data_size());
            _master_time_start = global_dispatch->get_audio_output()->start();
            _master_time_pos = _audio_pos;
            _current_pos = _audio_pos;
        }
        else
        {
            _master_time_start = timer::get(timer::monotonic);
            _master_time_pos = _video_pos;
            _current_pos = _video_pos;
        }
        global_dispatch->set_position(normalize_pos(_current_pos));
        set_current_subtitle_box();
        _recently_seeked = true;
        *prep_frame = true;
        *more_steps = true;
        return 0;
    }
    else if (_recently_seeked)
    {
        _recently_seeked = false;
        _need_frame_now = true;
        _need_frame_soon = false;
        _previous_frame_dropped = false;
        *display_frame = true;
        *more_steps = true;
        return 0;
    }
    else if (_pause_request)
    {
        if (!_in_pause)
        {
            if (global_dispatch->get_audio_output()
                && global_dispatch->get_media_input()->selected_audio_stream() >= 0)
            {
                global_dispatch->get_audio_output()->pause();
            }
            else
            {
                _pause_start = timer::get(timer::monotonic);
            }
            _in_pause = true;
            global_dispatch->set_pausing(true);
        }
        *more_steps = true;
        return 1000;    // allow some sleep in pause mode
    }
    else if (_need_frame_now)
    {
        _video_frame = global_dispatch->get_media_input()->finish_video_frame_read();
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
                if (dispatch::parameters().loop_mode() == parameters::loop_current)
                {
                    _set_pos_request = 0.0f;
                    *more_steps = true;
                }
                return 0;
            }
        }
        else
        {
            _first_frame = false;
        }
        _video_pos = _video_frame.presentation_time;
        if (global_dispatch->get_media_input()->selected_subtitle_stream() >= 0)
        {
            while (_next_subtitle_box.is_valid()
                    && _next_subtitle_box.presentation_stop_time < _video_pos)
            {
                _next_subtitle_box = global_dispatch->get_media_input()->finish_subtitle_box_read();
                global_dispatch->get_media_input()->start_subtitle_box_read();
                // If the box is invalid, we reached the end of the subtitle stream.
                // Ignore this and let audio/video continue.
            }
        }
        if (!global_dispatch->get_audio_output()
                || global_dispatch->get_media_input()->selected_audio_stream() < 0)
        {
            _master_time_start += (_video_pos - _master_time_pos);
            _master_time_pos = _video_pos;
            _current_pos = _video_pos;
            global_dispatch->set_position(normalize_pos(_current_pos));
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
        global_dispatch->get_media_input()->start_video_frame_read();
        _need_frame_soon = false;
        *more_steps = true;
        return 0;
    }
    else
    {
        if (_in_pause)
        {
            if (global_dispatch->get_audio_output()
                && global_dispatch->get_media_input()->selected_audio_stream() >= 0)
            {
                global_dispatch->get_audio_output()->unpause();
            }
            else
            {
                _master_time_start += timer::get(timer::monotonic) - _pause_start;
            }
            _in_pause = false;
            global_dispatch->set_pausing(false);
        }

        if (global_dispatch->get_audio_output()
                && global_dispatch->get_media_input()->selected_audio_stream() >= 0)
        {
            // Check if audio needs more data, and get audio time
            bool need_audio_data;
            _master_time_current = global_dispatch->get_audio_output()->status(&need_audio_data) - _master_time_start + _master_time_pos;
            // Output requested audio data
            if (need_audio_data)
            {
                audio_blob blob = global_dispatch->get_media_input()->finish_audio_blob_read();
                if (!blob.is_valid())
                {
                    msg::dbg("End of audio stream.");
                    if (dispatch::parameters().loop_mode() == parameters::loop_current)
                    {
                        _set_pos_request = 0.0f;
                        *more_steps = true;
                    }
                    return 0;
                }
                _audio_pos = blob.presentation_time;
                _master_time_start += (_audio_pos - _master_time_pos);
                _master_time_pos = _audio_pos;
                global_dispatch->get_audio_output()->data(blob);
                global_dispatch->get_media_input()->start_audio_blob_read(global_dispatch->get_audio_output()->required_update_data_size());
                _current_pos = _audio_pos;
                global_dispatch->set_position(normalize_pos(_current_pos));
            }
        }
        else
        {
            // Use our own timer
            _master_time_current = timer::get(timer::monotonic) - _master_time_start
                + _master_time_pos;
        }

        int64_t allowable_sleep = 0;
        int64_t next_frame_presentation_time = _master_time_current + dispatch::parameters().audio_delay();
        if (dispatch::video_output())
            next_frame_presentation_time += dispatch::video_output()->time_to_next_frame_presentation();
        if (next_frame_presentation_time >= _video_pos
                || dispatch::parameters().benchmark()
                || global_dispatch->get_media_input()->is_device())
        {
            // Output current video frame
            _drop_next_frame = false;
            int64_t delay = next_frame_presentation_time - _video_pos;
            if (delay > global_dispatch->get_media_input()->video_frame_duration() * 75 / 100
                    && !dispatch::parameters().benchmark()
                    && !global_dispatch->get_media_input()->is_device()
                    && !_step_request)
            {
                msg::wrn(_("Video: delay %g seconds/%g frames; dropping next frame."),
                         float(delay) / 1e6f, 
                         float(delay) / float(global_dispatch->get_media_input()->video_frame_duration()));
                _drop_next_frame = true;
            }
            if (!_previous_frame_dropped)
            {
                *display_frame = true;
                if (_step_request)
                    _pause_request = true;
                if (dispatch::parameters().benchmark())
                {
                    _frames_shown++;
                    if (_frames_shown == 100)   //show fps each 100 frames
                    {
                        int64_t now = timer::get(timer::monotonic);
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
            allowable_sleep = _video_pos - next_frame_presentation_time;
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
        if (_current_subtitle_box.is_valid() && global_dispatch->get_video_output())
        {
            _master_time_start += global_dispatch->get_video_output()->wait_for_subtitle_renderer();
        }
        global_dispatch->get_video_output()->prepare_next_frame(_video_frame, _current_subtitle_box);
    }
    else if (drop_frame)
    {
    }
    else if (display_frame)
    {
        global_dispatch->get_video_output()->activate_next_frame();
    }

    dispatch::process_all_events();
    if (allowed_sleep > 0)
    {
        usleep(allowed_sleep);
    }

    return true;
}

void player::quit_request()
{
    _quit_request = true;
}

void player::set_pause(bool p)
{
    _pause_request = p;
    if (!p)
        _step_request = false;
}

void player::set_step(bool s)
{
    _step_request = s;
    if (s)
        _pause_request = false;
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
    assert(dispatch::media_input())
    assert(s >= 0 && s < dispatch::media_input()->video_streams());
    global_dispatch->get_media_input()->select_video_stream(s);
    if (dispatch::playing())
        _seek_request = -1; // Get position right
    return s;
}

int player::set_audio_stream(int s)
{
    assert(dispatch::media_input())
    assert(s >= 0 && s < dispatch::media_input()->audio_streams());
    global_dispatch->get_media_input()->select_audio_stream(s);
    if (dispatch::playing())
        _seek_request = -1; // Get position right
    return s;
}

int player::set_subtitle_stream(int s)
{
    assert(dispatch::media_input())
    assert(s >= -1 && s < dispatch::media_input()->subtitle_streams());
    global_dispatch->get_media_input()->select_subtitle_stream(s);
    if (dispatch::playing())
        _seek_request = -1; // Get position right
    return s;
}

void player::set_stereo_layout(parameters::stereo_layout_t stereo_layout)
{
    global_dispatch->get_media_input()->set_stereo_layout(stereo_layout, dispatch::parameters().stereo_layout_swap());
    // If the new layout is layout_separate, then seek to synchronize both video streams.
    // If we're pausing, then seek to reload the current frame (or a near frame) and
    // trigger an update of the display.
    // In other cases, we can just continue to read our video and the display will
    // update with the next frame.
    if (stereo_layout == parameters::layout_separate || dispatch::pausing())
        _seek_request = -1;
}

void player::set_stereo_layout_swap(bool swap)
{
    global_dispatch->get_media_input()->set_stereo_layout(dispatch::parameters().stereo_layout(), swap);
}

float player::get_pos() const
{
    return normalize_pos(_current_pos);
}
