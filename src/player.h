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

#include "input.h"
#include "controller.h"
#include "audio_output.h"
#include "video_output.h"


class player
{
private:
    input *_input;
    std::vector<controller *> *_controllers;
    audio_output *_audio_output;
    video_output *_video_output;

    bool _quit_request;
    bool _pause_request;
    int64_t _seek_request;

    void notify(const notification &note);
    void notify(enum notification::type t, bool p, bool c) { notify(notification(t, p, c)); }
    void notify(enum notification::type t, float p, float c) { notify(notification(t, p, c)); }

public:

    /* Constructor/destructor.
     * Only a single player instance can exist. The constructor throws an
     * exception if and only if it detects that this instance already exists. */
    player();
    ~player();

    /* Open a player. It combines the input, a bunch of controllers, optionally an
     * audio output, and a video output. */
    void open(input *input,
            std::vector<controller *> *controllers,
            audio_output *audio_output, // may be NULL
            video_output *video_output);

    /* Run the player. It will take care of all interaction. This function
     * returns when the user quits the player. */
    void run();

    /* Close the player and clean up. */
    void close();

    /* Receive a command from a controller. */
    void receive_cmd(const command &cmd);
};

#endif
