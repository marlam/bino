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

#include "exc.h"
#include "str.h"
#include "msg.h"
#include "timer.h"

#include "player.h"


// The single player instance
player *global_player = NULL;

player::player()
    : _input(NULL), _controllers(NULL), _audio_output(NULL), _video_output(NULL), 
    _quit_request(false), _pause_request(false), _seek_request(0)
{
    if (global_player)
    {
        throw exc("cannot create a second player object");
    }
    global_player = this;
}

player::~player()
{
    global_player = NULL;
}

void player::open(input *input,
        std::vector<controller *> *controllers,
        audio_output *audio_output,
        video_output *video_output)
{
    _input = input;
    _controllers = controllers;
    _audio_output = audio_output;
    _video_output = video_output;
}

void player::run()
{
    uint8_t *l_data[3], *r_data[3];
    size_t l_line_size[3], r_line_size[3];
    void *audio_data;
    size_t required_audio_data_size;
    int64_t pause_start = -1;
    // Audio / video timing, relative to a synchronization point.
    // The master time is the audio time, or external time if there is no audio.
    // All times are in microseconds.
    int64_t sync_point_pos;             // input position at last sync point
    int64_t sync_point_time;            // master time at last sync point
    int64_t sync_point_av_offset;       // audio/video offset at last sync point
    int64_t current_pos;                // current input position
    int64_t current_time;               // current master time
    int64_t next_frame_pos;             // presentation time of next video frame

    // Initial buffering
    if (_input->read_video_frame() < 0)
    {
        msg::dbg("empty video input");
        return;
    }
    _input->get_video_frame(_video_output->frame_format(), l_data, l_line_size, r_data, r_line_size);
    _video_output->prepare(l_data, l_line_size, r_data, r_line_size);
    _input->release_video_frame();
    if (_audio_output)
    {
        _audio_output->status(&required_audio_data_size);
        if (_input->read_audio_data(&audio_data, required_audio_data_size) < 0)
        {
            msg::dbg("empty audio input");
            return;
        }
        _audio_output->data(audio_data, required_audio_data_size);
    }

    // Start video and audio output.
    current_pos = 0;
    sync_point_pos = 0;
    sync_point_av_offset = 0;
    _video_output->activate();
    _video_output->process_events();
    if (_audio_output)
    {
        sync_point_time = _audio_output->start();
    }
    else
    {
        sync_point_time = timer::get_microseconds(timer::monotonic);
    }

    // Start buffering next video frame
    next_frame_pos = _input->read_video_frame();
    if (next_frame_pos < 0)
    {
        // This is a single-image input. Put us in pause mode.
        msg::dbg("single-frame video input: going into pause mode");
        _pause_request = true;
    }
    _input->get_video_frame(_video_output->frame_format(), l_data, l_line_size, r_data, r_line_size);
    _video_output->prepare(l_data, l_line_size, r_data, r_line_size);
    _input->release_video_frame();

    // The player loop
    bool eof = false;
    bool previous_frame_dropped = false;
    bool in_pause = false;
    while (!eof && !_quit_request)
    {
        if (_seek_request != 0)
        {
            if (_seek_request < 0 && -_seek_request > current_pos)
            {
                _seek_request = -current_pos;
            }
            if (_input->duration() > 0)
            {
                if (_seek_request > 0 && current_pos + _seek_request >= std::max(_input->duration() - 5000000, static_cast<int64_t>(0)))
                {
                    _seek_request = std::max(_input->duration() - 5000000 - current_pos, static_cast<int64_t>(0));
                }
            }
            _input->seek(current_pos + _seek_request);
            next_frame_pos = _input->read_video_frame();
            eof = (next_frame_pos < 0);
            if (eof)
            {
                msg::wrn("seeked to EOF?!");
            }
            else
            {
                if (_audio_output)
                {
                    _audio_output->stop();
                    _audio_output->status(&required_audio_data_size);
                    int64_t audio_pos = _input->read_audio_data(&audio_data, required_audio_data_size);
                    if (audio_pos >= 0)
                    {
                        _audio_output->data(audio_data, required_audio_data_size);
                        sync_point_time = _audio_output->start();
                        sync_point_pos = audio_pos;
                        sync_point_av_offset = next_frame_pos - audio_pos;
                    }
                }
                else
                {
                    sync_point_pos = next_frame_pos;
                    sync_point_time = timer::get_microseconds(timer::monotonic);
                    sync_point_av_offset = 0;
                }
                notify(notification::pos, current_pos / 1e6f, sync_point_pos / 1e6f);
                current_pos = sync_point_pos;
                _input->get_video_frame(_video_output->frame_format(), l_data, l_line_size, r_data, r_line_size);
                _video_output->prepare(l_data, l_line_size, r_data, r_line_size);
                _input->release_video_frame();
                previous_frame_dropped = false;
            }
            _seek_request = 0;
        }

        if (_pause_request)
        {
            if (!in_pause)
            {
                if (_audio_output)
                {
                    _audio_output->pause();
                }
                else
                {
                    pause_start = timer::get_microseconds(timer::monotonic);
                }
                in_pause = true;
                notify(notification::pause, false, true);
            }
        }
        else
        {
            if (in_pause)
            {
                if (_audio_output)
                {
                    _audio_output->unpause();
                }
                else
                {
                    sync_point_time += timer::get_microseconds(timer::monotonic) - pause_start;
                }
                in_pause = false;
                notify(notification::pause, true, false);
            }

            if (_audio_output)
            {
                // Check if audio needs more data, and get audio time
                current_time = _audio_output->status(&required_audio_data_size);
                // Output requested audio data
                if (required_audio_data_size > 0)
                {
                    eof = eof || (_input->read_audio_data(&audio_data, required_audio_data_size) < 0);
                    _audio_output->data(audio_data, required_audio_data_size);
                }
            }
            else
            {
                // Use our own timer
                current_time = timer::get_microseconds(timer::monotonic);
            }

            int64_t time_to_next_frame = (next_frame_pos - sync_point_pos) - (current_time - sync_point_time) - sync_point_av_offset;
            if (time_to_next_frame <= 0)
            {
                // Output current video frame
                bool drop_next_frame = false;
                if (-time_to_next_frame > _input->video_frame_duration() * 75 / 100)
                {
                    msg::wrn("video: delay %g seconds; dropping next frame", (-time_to_next_frame) / 1e6f);
                    drop_next_frame = true;
                }
                if (!previous_frame_dropped)
                {
                    _video_output->activate();
                    _video_output->process_events();
                }
                current_pos = next_frame_pos;

                // Read next frame
                next_frame_pos = _input->read_video_frame();
                eof = eof || (next_frame_pos < 0);

                // Prepare next video output
                if (drop_next_frame)
                {
                    _input->release_video_frame();
                }
                else
                {
                    _input->get_video_frame(_video_output->frame_format(), l_data, l_line_size, r_data, r_line_size);
                    _video_output->prepare(l_data, l_line_size, r_data, r_line_size);
                    _input->release_video_frame();
                }
                previous_frame_dropped = drop_next_frame;
            }
        }

        // While waiting, allow the video output to react on window system events
        _video_output->process_events();
    }
}

void player::close()
{
}

void player::receive_cmd(const command &cmd)
{
    const struct video_output::state &video_state = _video_output->state();
    bool flag;
    float value;

    switch (cmd.type)
    {
    case command::quit:
        _quit_request = true;
        break;
    case command::toggle_swap_eyes:
        flag = !video_state.swap_eyes;
        notify(notification::swap_eyes, video_state.swap_eyes, flag);
        break;
    case command::toggle_fullscreen:
        flag = !video_state.fullscreen;
        notify(notification::fullscreen, video_state.fullscreen, flag);
        break;
    case command::toggle_pause:
        _pause_request = !_pause_request;
        /* notify when request is fulfilled */
        break;
    case command::adjust_gamma:
        value = std::max(std::min(video_state.gamma + cmd.param, 4.0f), 0.1f);
        notify(notification::gamma, video_state.gamma, value);
        break;
    case command::adjust_contrast:
        value = std::max(std::min(video_state.contrast + cmd.param, 1.0f), -1.0f);
        notify(notification::contrast, video_state.contrast, value);
        break;
    case command::adjust_brightness:
        value = std::max(std::min(video_state.brightness + cmd.param, 1.0f), -1.0f);
        notify(notification::brightness, video_state.brightness, value);
        break;
    case command::adjust_hue:
        value = std::max(std::min(video_state.hue + cmd.param, 1.0f), -1.0f);
        notify(notification::hue, video_state.hue, value);
        break;
    case command::adjust_saturation:
        value = std::max(std::min(video_state.saturation + cmd.param, 1.0f), -1.0f);
        notify(notification::saturation, video_state.saturation, value);
        break;
    case command::seek:
        _seek_request = cmd.param * 1e6f;
        /* notify when request is fulfilled */
        break;
    }
}

void player::notify(const notification &note)
{
    // Notify all controllers (e.g. to update the GUI)
    if (_controllers)
    {
        for (size_t i = 0; i < _controllers->size(); i++)
        {
            _controllers->at(i)->receive_notification(note);
        }
    }
    // Notify the audio output (e.g. to play a sound)
    if (_audio_output)
    {
        _audio_output->receive_notification(note);
    }
    // Notify the video output (e.g. to update the on screen display)
    _video_output->receive_notification(note);
}
