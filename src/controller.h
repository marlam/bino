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

#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <string>


class command
{
public:
    enum type
    {
        toggle_play,
        toggle_pause,
        toggle_swap_eyes,
        toggle_fullscreen,
        center,
        adjust_contrast,
        adjust_brightness,
        adjust_hue,
        adjust_saturation,
        seek
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

class notification
{
public:
    enum type
    {
        play,
        pause,
        swap_eyes,
        fullscreen,
        center,
        contrast,
        brightness,
        hue,
        saturation,
        pos
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

class controller
{
public:
    controller(bool receive_notifications = true) throw ();
    ~controller();

    /* A controller can use this function to send a command to the player.
     * The player then reacts on the command, and possibly sends a notification
     * afterwards. */
    void send_cmd(const command &cmd);
    void send_cmd(enum command::type t) { send_cmd(command(t)); }                    // convenience wrapper
    void send_cmd(enum command::type t, float p) { send_cmd(command(t, p)); }        // convenience wrapper

    /* A controller can use this function to receive a notification from the
     * player. This can be used e.g. to update a GUI or on screen display.
     * The default implementation simply ignores the notification. */
    virtual void receive_notification(const notification &note);
};

#endif
