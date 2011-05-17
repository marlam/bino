/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010-2011
 * Martin Lambers <marlam@marlam.de>
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

#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <string>
#include <sstream>
#include <stdint.h>

#include "s11n.h"


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
        noop,                           // no parameters
        toggle_play,                    // no parameters
        toggle_pause,                   // no parameters
        cycle_video_stream,             // no parameters
        set_video_stream,               // int
        cycle_audio_stream,             // no parameters
        set_audio_stream,               // int
        cycle_subtitle_stream,          // no parameters
        set_subtitle_stream,            // int
        set_stereo_layout,              // video_frame::stereo_layout, bool
        set_stereo_mode,                // parameters::stereo_mode, bool
        toggle_stereo_mode_swap,        // no parameters
        toggle_fullscreen,              // no parameters
        center,                         // no parameters
        adjust_contrast,                // float (relative adjustment)
        set_contrast,                   // float (absolute value)
        adjust_brightness,              // float (relative adjustment)
        set_brightness,                 // float (absolute value)
        adjust_hue,                     // float (relative adjustment)
        set_hue,                        // float (absolute value)
        adjust_saturation,              // float (relative adjustment)
        set_saturation,                 // float (absolute value)
        adjust_parallax,                // float (relative adjustment)
        set_parallax,                   // float (absolute value)
        set_crosstalk,                  // 3 floats (absolute values)
        adjust_ghostbust,               // float (relative adjustment)
        set_ghostbust,                  // float (absolute value)
        set_subtitle_encoding,          // string (encoding name)
        set_subtitle_font,              // string (font name)
        set_subtitle_size,              // int
        set_subtitle_scale,             // float
        set_subtitle_color,             // uint64_t
        adjust_subtitle_parallax,       // float (relative adjustment)
        set_subtitle_parallax,          // float (absolute value)
        seek,                           // float (relative adjustment)
        set_pos,                        // float (absolute position)
        set_loop_mode,                  // parameters::loop_mode_t
        set_fullscreen_screens,         // int
        set_fullscreen_flip_left,       // int
        set_fullscreen_flop_left,       // int
        set_fullscreen_flip_right,      // int
        set_fullscreen_flop_right,      // int
    };
    
    type type;
    std::string param;

    command() :
        type(noop)
    {
    }

    command(enum type t) :
        type(t)
    {
    }

    command(enum type t, int p) :
        type(t)
    {
        std::ostringstream oss;
        s11n::save(oss, p);
        param = oss.str();
    }

    command(enum type t, float p) :
        type(t)
    {
        std::ostringstream oss;
        s11n::save(oss, p);
        param = oss.str();
    }

    command(enum type t, const std::string &p) :
        type(t), param(p)
    {
    }
};

// A notification that can be sent to controllers by the player.

class notification
{
public:
    enum type
    {
        play,                   // bool
        pause,                  // bool
        video_stream,           // int
        audio_stream,           // int
        subtitle_stream,        // int
        stereo_layout,          // video_frame::stereo_layout, bool
        stereo_mode,            // parameters::stereo_mode, bool
        stereo_mode_swap,       // bool
        fullscreen,             // bool
        center,                 // bool
        contrast,               // float
        brightness,             // float
        hue,                    // float
        saturation,             // float
        parallax,               // float
        crosstalk,              // 3 floats
        ghostbust,              // float
        subtitle_encoding,      // string
        subtitle_font,          // string
        subtitle_size,          // int
        subtitle_scale,         // float
        subtitle_color,         // uint64_t
        subtitle_parallax,      // float
        pos,                    // float
        loop_mode,              // parameters::loop_mode_t
        fullscreen_screens,     // int
        fullscreen_flip_left,   // int
        fullscreen_flop_left,   // int
        fullscreen_flip_right,  // int
        fullscreen_flop_right,  // int
    };
    
    type type;
    std::string previous;       // previous value of the state indicated by type
    std::string current;        // current value of the state indicated by type

    notification(enum type t) :
        type(t)
    {
    }

    notification(enum type t, bool p, bool c) :
        type(t)
    {
        std::ostringstream ossp;
        s11n::save(ossp, p);
        previous = ossp.str();
        std::ostringstream ossc;
        s11n::save(ossc, c);
        current = ossc.str();
    }

    notification(enum type t, int p, int c) :
        type(t)
    {
        std::ostringstream ossp;
        s11n::save(ossp, p);
        previous = ossp.str();
        std::ostringstream ossc;
        s11n::save(ossc, c);
        current = ossc.str();
    }

    notification(enum type t, float p, float c) :
        type(t)
    {
        std::ostringstream ossp;
        s11n::save(ossp, p);
        previous = ossp.str();
        std::ostringstream ossc;
        s11n::save(ossc, c);
        current = ossc.str();
    }

    notification(enum type t, uint64_t p, uint64_t c) :
        type(t)
    {
        std::ostringstream ossp;
        s11n::save(ossp, p);
        previous = ossp.str();
        std::ostringstream ossc;
        s11n::save(ossc, c);
        current = ossc.str();
    }

    notification(enum type t, const std::string &p, const std::string &c) :
        type(t)
    {
        std::ostringstream ossp;
        s11n::save(ossp, p);
        previous = ossp.str();
        std::ostringstream ossc;
        s11n::save(ossc, c);
        current = ossc.str();
    }
};

// The controller interface.

class controller
{
private:
    static void visit_all_controllers(int action, const notification &note);

public:
    controller() throw ();
    virtual ~controller();

    /* The controller uses this function to send a command to the player. */
    void send_cmd(const command &cmd);
    // Convenience wrappers:
    void send_cmd(enum command::type t) { send_cmd(command(t)); }
    void send_cmd(enum command::type t, int p) { send_cmd(command(t, p)); }
    void send_cmd(enum command::type t, float p) { send_cmd(command(t, p)); }
    void send_cmd(enum command::type t, const std::string &p) { send_cmd(command(t, p)); }

    /* The controller receives notifications via this function. The default
     * implementation simply ignores the notification. */
    virtual void receive_notification(const notification &note);

    /* The controller is asked to process events via this function. The default
     * implementation simply does nothing. */
    virtual void process_events();

    /* The following functions can be used by command dispatchers to handle all
     * controller-related tasks: managing the global player object, and notifying
     * registered controllers. */
    static void *get_global_player();
    static void set_global_player(void *p);
    static void process_all_events();
    static void notify_all(const notification &note);
    // Convenience wrappers:
    static void notify_all(enum notification::type t, bool p, bool c) { notify_all(notification(t, p, c)); }
    static void notify_all(enum notification::type t, int p, int c) { notify_all(notification(t, p, c)); }
    static void notify_all(enum notification::type t, float p, float c) { notify_all(notification(t, p, c)); }
    static void notify_all(enum notification::type t, uint64_t p, uint64_t c) { notify_all(notification(t, p, c)); }
    static void notify_all(enum notification::type t, const std::string &p, const std::string &c) { notify_all(notification(t, p, c)); }
};

#endif
