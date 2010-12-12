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

#ifndef PLAYER_H
#define PLAYER_H

#include <vector>
#include <string>

#include "msg.h"

#include "input.h"
#include "controller.h"
#include "audio_output.h"
#include "video_output.h"


class player_init_data
{
public:
    msg::level_t log_level;
    bool benchmark;
    std::vector<std::string> filenames;
    enum input::mode input_mode;
    enum video_output::mode video_mode;
    video_output_state video_state;
    unsigned int video_flags;

public:
    player_init_data();
    ~player_init_data();
};

class player
{
public:
    enum type
    {
        master, slave
    };

private:
    std::vector<decoder *> _decoders;
    input *_input;
    std::vector<controller *> _controllers;
    audio_output *_audio_output;
    video_output *_video_output;
    video_output_state _video_state;
    bool _benchmark;

    bool _running;
    bool _first_frame;
    bool _need_frame;
    bool _drop_next_frame;
    bool _previous_frame_dropped;
    bool _in_pause;

    bool _quit_request;
    bool _pause_request;
    int64_t _seek_request;

    uint8_t *_l_data[3], *_r_data[3];
    size_t _l_line_size[3], _r_line_size[3];
    void *_audio_data;
    size_t _required_audio_data_size;
    int64_t _pause_start;
    // Audio / video timing, relative to a synchronization point.
    // The master time is the audio time, or external time if there is no audio.
    // All times are in microseconds.
    int64_t _sync_point_pos;             // input position at last sync point
    int64_t _sync_point_time;            // master time at last sync point
    int64_t _sync_point_av_offset;       // audio/video offset at last sync point
    int64_t _current_pos;                // current input position
    int64_t _current_time;               // current master time
    int64_t _next_frame_pos;             // presentation time of next video frame

    int _frames_shown;                   // frames shown since last _sync_point_time update
    int64_t _fps_mark_time;              // time when _frames_shown was reset to zero

protected:
    void set_benchmark(bool benchmark)
    {
        _benchmark = benchmark;
    }
    void reset_playstate();
    void create_decoders(const std::vector<std::string> &filenames);
    void create_input(enum input::mode input_mode);
    void get_input_info(int *w, int *h, float *ar, enum decoder::video_frame_format *fmt);
    void create_audio_output();
    void create_video_output();
    void set_video_output(video_output *vo)
    {
        _video_output = vo;
    }
    video_output_state &video_state() { return _video_state; }
    void open_video_output(enum video_output::mode video_mode, unsigned int video_flags);
    void make_master();
    void run_step(bool *more_steps, int64_t *seek_to, bool *prep_frame, bool *drop_frame, bool *display_frame);
    void seek(int64_t seek_to);
    void get_video_frame(enum decoder::video_frame_format fmt);
    void prepare_video_frame(video_output *vo);
    void release_video_frame();
    input *get_input() { return _input; }
    video_output *get_video_output() { return _video_output; }

    void notify(const notification &note);
    void notify(enum notification::type t, bool p, bool c) { notify(notification(t, p, c)); }
    void notify(enum notification::type t, float p, float c) { notify(notification(t, p, c)); }

public:
    /* Constructor/destructor.
     * Only a single player instance can exist. The constructor throws an
     * exception if and only if it detects that this instance already exists. */
    player(type t = master);
    virtual ~player();

    /* Open a player. */
    virtual void open(const player_init_data &init_data);

    /* Get modes. */
    enum input::mode input_mode() const;
    enum video_output::mode video_mode() const;

    /* Run the player. It will take care of all interaction. This function
     * returns when the user quits the player. */
    virtual void run();

    /* Close the player and clean up. */
    virtual void close();

    /* Receive a command from a controller. */
    virtual void receive_cmd(const command &cmd);
};

#endif
