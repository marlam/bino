/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010, 2011, 2012
 * Martin Lambers <marlam@marlam.de>
 * Joe <cuchac@email.cz>
 * Binocle <http://binocle.com> (author: Olivier Letz <oletz@binocle.com>)
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

#ifndef DISPATCH_H
#define DISPATCH_H

#include <string>
#include <sstream>
#include <stdint.h>

#include "s11n.h"
#include "thread.h"

#include "media_data.h"

class gui;
class audio_output;
class video_output;
class media_input;
class player;


/* The open_init_data class contains everything that is needed to open
 * a media input. */

class open_input_data : public serializable
{
public:
    device_request dev_request;    // Request for input device settings
    std::vector<std::string> urls; // Input media objects
    parameters params;             // Initial per-video output parameters
                                   // (other parameter fields are ignored)
public:
    open_input_data();

    // Serialization
    void save(std::ostream &os) const;
    void load(std::istream &is);
};


/* A controller can send commands to the dispatch (e.g. "pause", "seek",
 * "adjust colors", ...). The dispatch then reacts on this command, and sends
 * a notification to all controllers afterwards. The controllers may react
 * on the notification or ignore it.
 *
 * For example, the video output controller may notice that the user pressed the
 * 'p' key to pause the video. So it sends the "pause" command to the dispatch.
 * The dipatch updates its state accordingly, and notifies all controllers that
 * the video is now paused. The video output could use this notification to display
 * a pause symbol on screen, and the audio output controller may play a pause jingle
 * (however, in the case of pause, both currently simply ignore the notification).
 */

// A command that can be sent to the dispatch by a controller.

class command
{
public:
    enum type
    {
        noop,                           // no parameters
        quit,                           // no parameters
        // Play state
        open,                           // open_input_data
        close,                          // no parameters
        toggle_play,                    // no parameters
        toggle_pause,                   // no parameters
        step,                           // no parameters
        seek,                           // float (relative adjustment)
        set_pos,                        // float (absolute position)
        // Per-Session parameters
        set_audio_device,               // int
        set_quality,                    // int
        set_stereo_mode,                // parameters::stereo_mode
        set_stereo_mode_swap,           // bool
        toggle_stereo_mode_swap,        // no parameters
        set_crosstalk,                  // 3 floats (absolute values)
        set_fullscreen_screens,         // int
        set_fullscreen_flip_left,       // bool
        set_fullscreen_flop_left,       // bool
        set_fullscreen_flip_right,      // bool
        set_fullscreen_flop_right,      // bool
        set_fullscreen_inhibit_screensaver,     // bool
        set_fullscreen_3d_ready_sync,   // bool
        set_contrast,                   // float (absolute value)
        adjust_contrast,                // float (relative adjustment)
        set_brightness,                 // float (absolute value)
        adjust_brightness,              // float (relative adjustment)
        set_hue,                        // float (absolute value)
        adjust_hue,                     // float (relative adjustment)
        set_saturation,                 // float (absolute value)
        adjust_saturation,              // float (relative adjustment)
        set_zoom,                       // float (absolute value)
        adjust_zoom,                    // float (relative adjustment)
        set_loop_mode,                  // parameters::loop_mode_t
        set_audio_delay,                // float (absolute value)
        set_subtitle_encoding,          // string (encoding name)
        set_subtitle_font,              // string (font name)
        set_subtitle_size,              // int
        set_subtitle_scale,             // float
        set_subtitle_color,             // uint64_t
        set_subtitle_shadow,            // int
#if HAVE_LIBXNVCTRL
        set_sdi_output_format,          // int
        set_sdi_output_left_stereo_mode,  // parameters::stereo_mode_t
        set_sdi_output_right_stereo_mode, // parameters::stereo_mode_t
#endif // HAVE_LIBXNVCTRL
        // Per-Video parameters
        set_video_stream,               // int
        cycle_video_stream,             // no parameters
        set_audio_stream,               // int
        cycle_audio_stream,             // no parameters
        set_subtitle_stream,            // int
        cycle_subtitle_stream,          // no parameters
        set_stereo_layout,              // video_frame::stereo_layout
        set_stereo_layout_swap,         // bool
        set_crop_aspect_ratio,          // float
        set_parallax,                   // float (absolute value)
        adjust_parallax,                // float (relative adjustment)
        set_ghostbust,                  // float (absolute value)
        adjust_ghostbust,               // float (relative adjustment)
        set_subtitle_parallax,          // float (absolute value)
        adjust_subtitle_parallax,       // float (relative adjustment)
        // Volatile parameters
        toggle_fullscreen,              // no parameters
        center,                         // no parameters
        set_audio_volume,               // float (absolute value)
        adjust_audio_volume,            // float (relative adjustment)
        toggle_audio_mute,              // no parameters
        update_display_pos,             // no parameters
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
        quit,
        // Play state
        open,
        play,
        pause,
        pos,
        // Per-Session parameters
        audio_device,
        quality,
        stereo_mode,
        stereo_mode_swap,
        crosstalk,
        fullscreen_screens,
        fullscreen_flip_left,
        fullscreen_flop_left,
        fullscreen_flip_right,
        fullscreen_flop_right,
        fullscreen_inhibit_screensaver,
        fullscreen_3d_ready_sync,
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
        subtitle_shadow,
#if HAVE_LIBXNVCTRL
        sdi_output_format,
        sdi_output_left_stereo_mode,
        sdi_output_right_stereo_mode,
#endif // HAVE_LIBXNVCTRL
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
        display_pos,
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

    /* The controller can prevent Bino from exiting when there is no video
     * to play. In this case, the following function should return 'false'.
     * This is intended to be used for controllers that might send another
     * 'open' command in the future. */
    virtual bool allow_early_quit();
};

// The dispatch (singleton).

class dispatch
{
private:
    int* _argc;
    char** _argv;
    const bool _eq;
    const bool _eq_3d;
    const bool _eq_slave_node;
    const bool _gui_mode;
    const bool _have_display;
    // Objects
    class gui* _gui;
    class audio_output* _audio_output;
    class video_output* _video_output;
    class media_input* _media_input;
    class player* _player;
    std::vector<controller*> _controllers;
    unsigned int _controllers_version;
    mutex _controllers_mutex;
    // Parameters
    open_input_data _input_data;
    class parameters _parameters;
    // State
    bool _playing;
    bool _pausing;
    float _position;

    void stop_player();
    void force_stop(bool reopen_media_input = true);

    bool early_quit_is_allowed() const;
    void visit_all_controllers(int action, const notification& note) const;
    void notify_all(const notification& note);

public:
    dispatch(int* argc, char** argv,
            bool equalizer, bool equalizer_3d, bool equalizer_slave_node,
            bool gui, bool have_display, msg::level_t log_level,
            bool benchmark, int swap_interval) throw ();
    virtual ~dispatch();

    void register_controller(controller* c);
    void deregister_controller(controller* c);

    void init(const open_input_data& input_data);
    void deinit();

    static void step();

    /* Process events for all controllers */
    static void process_all_events();

    /* Access parameters and state (read-only) */
    static const class parameters& parameters();
    static const class audio_output* audio_output();    // NULL if not available
    static const class video_output* video_output();    // NULL if not available
    static const class media_input* media_input();      // NULL if no input is opened
    static bool playing();
    static bool pausing();
    static float position();

    /* Receive a command from a controller. */
    void receive_cmd(const command& cmd);

    /* Interface for the player. TODO: remove this! */
    class audio_output* get_audio_output(); // NULL if not available
    class video_output* get_video_output(); // NULL if not available
    class media_input* get_media_input();   // NULL if not available
    void set_playing(bool p);
    void set_pausing(bool p);
    void set_position(float pos);

    /* Interface for Equalizer. */
    class open_input_data* get_input_data();
    std::string save_state() const;
    void load_state(const std::string& s);
    void stop_eq_player() { stop_player(); }

    /* Helper function for text-based controllers: parse a command from a string.
     * Return false if this fails, otherwise store the command in c. */
    static bool parse_command(const std::string& s, command* c);
};

#endif
