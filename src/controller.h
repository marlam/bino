/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010, 2011, 2012
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

#include "media_data.h"

class player;
class media_input;


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
        // Play state
        toggle_play,                    // no parameters
        toggle_pause,                   // no parameters
        seek,                           // float (relative adjustment)
        set_pos,                        // float (absolute position)
        // Per-Session parameters
        set_audio_device,               // int
        set_stereo_mode,                // parameters::stereo_mode
        set_stereo_mode_swap,           // bool
        toggle_stereo_mode_swap,        // no parameters
        set_crosstalk,                  // 3 floats (absolute values)
        set_fullscreen_screens,         // int
        set_fullscreen_flip_left,       // bool
        set_fullscreen_flop_left,       // bool
        set_fullscreen_flip_right,      // bool
        set_fullscreen_flop_right,      // bool
        adjust_contrast,                // float (relative adjustment)
        set_contrast,                   // float (absolute value)
        adjust_brightness,              // float (relative adjustment)
        set_brightness,                 // float (absolute value)
        adjust_hue,                     // float (relative adjustment)
        set_hue,                        // float (absolute value)
        adjust_saturation,              // float (relative adjustment)
        set_saturation,                 // float (absolute value)
        adjust_zoom,                    // float (relative adjustment)
        set_zoom,                       // float (absolute value)
        set_loop_mode,                  // parameters::loop_mode_t
        set_audio_delay,                // float (absolute value)
        set_subtitle_encoding,          // string (encoding name)
        set_subtitle_font,              // string (font name)
        set_subtitle_size,              // int
        set_subtitle_scale,             // float
        set_subtitle_color,             // uint64_t
        // Per-Video parameters
        cycle_video_stream,             // no parameters
        set_video_stream,               // int
        cycle_audio_stream,             // no parameters
        set_audio_stream,               // int
        cycle_subtitle_stream,          // no parameters
        set_subtitle_stream,            // int
        set_stereo_layout,              // video_frame::stereo_layout
        set_stereo_layout_swap,         // bool
        set_crop_aspect_ratio,          // float
        adjust_parallax,                // float (relative adjustment)
        set_parallax,                   // float (absolute value)
        adjust_ghostbust,               // float (relative adjustment)
        set_ghostbust,                  // float (absolute value)
        adjust_subtitle_parallax,       // float (relative adjustment)
        set_subtitle_parallax,          // float (absolute value)
        // Volatile parameters
        toggle_fullscreen,              // no parameters
        center,                         // no parameters
        adjust_audio_volume,            // float (relative adjustment)
        set_audio_volume,               // float (absolute value)
        toggle_audio_mute,              // no parameters
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

    command(enum type t, int64_t p) :
        type(t)
    {
        std::ostringstream oss;
        s11n::save(oss, p);
        param = oss.str();
    }

    command(enum type t, uint64_t p) :
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

// A notification that can be sent to controllers by the dispatch.
// It signals that the corresponding value has changed.

class notification
{
public:
    enum type
    {
        noop,
        // Play state
        play,
        pause,
        pos,
        // Per-Session parameters
        audio_device,
        stereo_mode,
        stereo_mode_swap,
        crosstalk,
        fullscreen_screens,
        fullscreen_flip_left,
        fullscreen_flop_left,
        fullscreen_flip_right,
        fullscreen_flop_right,
        contrast,
        brightness,
        hue,
        saturation,
        zoom,
        loop_mode,
        audio_delay,
        subtitle_encoding,
        subtitle_font,
        subtitle_size,
        subtitle_scale,
        subtitle_color,
        // Per-Video parameters
        video_stream,
        audio_stream,
        subtitle_stream,
        stereo_layout,
        stereo_layout_swap,
        crop_aspect_ratio,
        parallax,
        ghostbust,
        subtitle_parallax,
        // Volatile parameters
        fullscreen,
        center,
        audio_volume,
        audio_mute,
    };

    const enum type type;

public:
    notification(enum type t) : type(t)
    {
    }
};

// The controller interface.

class controller
{
public:
    controller() throw ();
    virtual ~controller();

    /* The controller uses this function to send a command to the player. */
    static void send_cmd(const command& cmd);
    // Convenience wrappers:
    static void send_cmd(enum command::type t) { send_cmd(command(t)); }
    static void send_cmd(enum command::type t, int p) { send_cmd(command(t, p)); }
    static void send_cmd(enum command::type t, float p) { send_cmd(command(t, p)); }
    static void send_cmd(enum command::type t, int64_t p) { send_cmd(command(t, p)); }
    static void send_cmd(enum command::type t, uint64_t p) { send_cmd(command(t, p)); }
    static void send_cmd(enum command::type t, const std::string& p) { send_cmd(command(t, p)); }

    /* The controller receives notifications via this function. The default
     * implementation simply ignores the notification. */
    virtual void receive_notification(const notification& note);

    /* The controller is asked to process events via this function. The default
     * implementation simply does nothing. */
    virtual void process_events();
};

// The dispatch (singleton).

class dispatch
{
private:
    const bool _eq_slave_node;
    // Parameters
    class parameters _parameters;
    // State
    bool _playing;
    bool _pausing;
    float _position;

    void visit_all_controllers(int action, const notification& note) const;
    static void notify_all(const notification& note);

public:
    dispatch(bool eq_slave_node, msg::level_t log_level, bool benchmark, int swap_interval) throw ();
    virtual ~dispatch();

    /* Process events for all controllers */
    static void process_all_events();

    /* Get/set the global player object */
    static player* get_global_player();
    static void set_global_player(player* p);

    /* Interface for the player. TODO: remove this! */
    static void set_playing(bool p);
    static void set_pausing(bool p);
    static void set_position(float pos);

    /* Helper interface for the GUI. TODO: remove this! */
    static void set_video_parameters(const class parameters& p);

    /* Access parameters and state (read-only) */
    static const class parameters& parameters();
    static const class media_input* media_input();      // NULL if no input is opened
    static const class video_output* video_output();    // NULL if not available
    static bool playing();
    static bool pausing();
    static float position();

    /* Receive a command from a controller. */
    static void receive_cmd(const command& cmd);
};

#endif
