/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010-2011
 * Martin Lambers <marlam@marlam.de>
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

#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <string>


/* A controller can send commands to the player (e.g. "pause", "seek",
 * "adjust colors", ...). The player then reacts on this command, and sends
 * a notification to all controllers afterwards. The controllers may react
 * on the notification or ignore it.
 *
 * For example, the video output controller may notice that the user pressed the
 * 'p' key to pause the video. So it sends the "pause" command to the player.
 * The player updates its state accordingly, and notifies all controllers that
 * the video is now paused. The video output could use this notification to display
 * a pause symbol on screen, and the audio output controller may play a pause jingle
 * (however, in the case of pause, both currently simply ignore the notification).
 */

// A command that can be sent to the player by a controller.

class command
{
public:
    enum type
    {
        toggle_play,
        toggle_pause,
        toggle_stereo_mode_swap,
        toggle_fullscreen,
        center,
        adjust_contrast,
        adjust_brightness,
        adjust_hue,
        adjust_saturation,
        seek,
        set_pos,
        adjust_parallax,
        set_parallax,
        adjust_ghostbust,
        set_ghostbust
    };
    
    type type;
    float param;

    command(enum type t)
    {
        type = t;
    }

    command(enum type t, float p)
    {
        type = t;
        param = p;
    }
};

// A notification that can be sent to controllers by the player.

class notification
{
public:
    enum type
    {
        play,
        pause,
        stereo_mode_swap,
        fullscreen,
        center,
        contrast,
        brightness,
        hue,
        saturation,
        pos,
        parallax,
        ghostbust
    };
    
    type type;
    union
    {
        bool flag;
        float value;
    } previous;
    union
    {
        bool flag;
        float value;
    } current;

    notification(enum type t)
    {
        type = t;
    }

    notification(enum type t, bool p, bool c)
    {
        type = t;
        previous.flag = p;
        current.flag = c;
    }

    notification(enum type t, float p, float c)
    {
        type = t;
        previous.value = p;
        current.value = c;
    }
};

// The controller interface.

class controller
{
public:
    /* A controller usually receives notifications, but may choose not to, e.g. when it
     * never will react on any notification anyway. */
    controller(bool receive_notifications = true) throw ();
    virtual ~controller();

    /* Send a command to the player. */
    void send_cmd(const command &cmd);
    void send_cmd(enum command::type t) { send_cmd(command(t)); }                    // convenience wrapper
    void send_cmd(enum command::type t, float p) { send_cmd(command(t, p)); }        // convenience wrapper

    /* Receive notifications via this function. The default implementation
     * simply ignores the notification. */
    virtual void receive_notification(const notification &note);
};

#endif
