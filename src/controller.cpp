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

#include "config.h"

#include <vector>

#include "exc.h"
#include "dbg.h"
#include "msg.h"
#include "timer.h"
#include "thread.h"

#include "controller.h"
#include "player.h"


// The single dispatch instance
static dispatch *global_dispatch;

// The single player instance
static player *global_player;

// The list of registered controllers
static std::vector<controller *> global_controllers;
static unsigned int global_controllers_version;
static mutex global_controllers_mutex;

// Note about thread safety:
// Only the constructor and destructor are thread safe (by using a lock on the
// global_controllers vector).  The visitation of controllers and the actions
// performed as a result are supposed to happen inside a single thread.

controller::controller() throw ()
{
    global_controllers_mutex.lock();
    global_controllers.push_back(this);
    global_controllers_version++;
    global_controllers_mutex.unlock();
}

controller::~controller()
{
    global_controllers_mutex.lock();
    for (size_t i = 0; i < global_controllers.size(); i++) {
        if (global_controllers[i] == this) {
            global_controllers.erase(global_controllers.begin() + i);
            break;
        }
    }
    global_controllers_version++;
    global_controllers_mutex.unlock();
}

void controller::send_cmd(const command &cmd)
{
    assert(global_dispatch);
    global_dispatch->receive_cmd(cmd);
}

void controller::receive_notification(const notification& /* note */)
{
    /* default: ignore the notification */
}

void controller::process_events()
{
    /* default: do nothing */
}


dispatch::dispatch(bool eq_slave_node, msg::level_t log_level, bool benchmark, int swap_interval) throw () :
    _eq_slave_node(eq_slave_node)
{
    assert(!global_dispatch);
    global_dispatch = this;
    _parameters.set_log_level(log_level);
    msg::set_level(log_level);
    _parameters.set_benchmark(benchmark);
    _parameters.set_swap_interval(swap_interval);
    _playing = false;
    _pausing = false;
}

dispatch::~dispatch()
{
    global_dispatch = NULL;
}

void dispatch::visit_all_controllers(int action, const notification& note) const
{
    std::vector<controller*> visited_controllers;
    visited_controllers.reserve(global_controllers.size());

    // First, try to visit all controllers in one row without extra checks. This
    // only works as long as controllers do not vanish or appear as a result of
    // the function we call. This is the common case.
    unsigned int controllers_version = global_controllers_version;
    for (size_t i = 0; i < global_controllers.size(); i++) {
        controller *c = global_controllers[i];
        if (action == 0) {
            c->process_events();
        } else {
            c->receive_notification(note);
        }
        visited_controllers.push_back(c);
        if (controllers_version != global_controllers_version) {
            break;
        }
    }
    // If some controllers vanished or appeared, we need to redo the loop and
    // check for each controller if it was visited before. This is costly, but
    // it happens seldomly.
    while (controllers_version != global_controllers_version)
    {
        controllers_version = global_controllers_version;
        for (size_t i = 0; i < global_controllers.size(); i++) {
            controller *c = global_controllers[i];
            bool visited = false;
            for (size_t j = 0; j < visited_controllers.size(); j++) {
                if (visited_controllers[j] == c) {
                    visited = true;
                    break;
                }
            }
            if (!visited) {
                if (action == 0) {
                    c->process_events();
                } else {
                    c->receive_notification(note);
                }
                visited_controllers.push_back(c);
                if (controllers_version != global_controllers_version) {
                    break;
                }
            }
        }
    }
}

void dispatch::notify_all(const notification& note)
{
    assert(global_dispatch);
    global_dispatch->visit_all_controllers(1, note);
}

void dispatch::process_all_events()
{
    assert(global_dispatch);
    global_dispatch->visit_all_controllers(0, notification::noop);
}

player* dispatch::get_global_player()
{
    return global_player;
}

void dispatch::set_global_player(player* p)
{
    assert(!p || !global_player);       // there can be at most one global player
    global_player = p;
}

void dispatch::set_playing(bool p)
{
    assert(global_dispatch);
    global_dispatch->_playing = p;
    global_dispatch->notify_all(notification::play);
}

void dispatch::set_pausing(bool p)
{
    assert(global_dispatch);
    global_dispatch->_pausing = p;
    global_dispatch->notify_all(notification::pause);
}

void dispatch::set_position(float pos)
{
    assert(global_dispatch);
    global_dispatch->_position = pos;
    global_dispatch->notify_all(notification::pos);
}

const class parameters& dispatch::parameters()
{
    assert(global_dispatch);
    return global_dispatch->_parameters;
}

const class media_input* dispatch::media_input()
{
    assert(global_dispatch);
    return (global_player ? &(global_player->get_media_input()) : NULL);
}

const class video_output* dispatch::video_output()
{
    assert(global_dispatch);
    return (global_player ? global_player->get_video_output() : NULL);
}

bool dispatch::playing()
{
    assert(global_dispatch);
    return global_dispatch->_playing;
}

bool dispatch::pausing()
{
    assert(global_dispatch);
    return global_dispatch->_pausing;
}

float dispatch::position()
{
    assert(global_dispatch);
    return dispatch::playing() && global_player ? global_player->get_pos() : 0.0f;
}

static float clamp(float x, float lo, float hi)
{
    return std::min(std::max(x, lo), hi);
}

void dispatch::receive_cmd(const command& cmd)
{
    std::istringstream p(cmd.param);

    class parameters& params = global_dispatch->_parameters;
    bool parameters_changed = false;

    switch (cmd.type)
    {
    case command::noop:
        break;
    case command::toggle_play:
        if (playing()) {
            assert(global_player);
            global_player->quit_request();
            /* notify when request is fulfilled */
        } else {
            // TODO;
            assert(0);
            dbg::crash();
        }
        break;
    case command::cycle_video_stream:
        if (global_player && global_player->get_media_input().video_streams() > 1
                && parameters().stereo_layout() != parameters::layout_separate) {
            int s = parameters().video_stream() + 1;
            if (s >= global_player->get_media_input().video_streams())
                s = 0;
            params.set_video_stream(global_player->set_video_stream(s));
            notify_all(notification::video_stream);
        }
        break;
    case command::set_video_stream:
        if (global_player && global_player->get_media_input().video_streams() > 1
                && parameters().stereo_layout() != parameters::layout_separate) {
            params.set_video_stream(global_player->set_video_stream(s11n::load<int>(p)));
            notify_all(notification::video_stream);
        }
        break;
    case command::cycle_audio_stream:
        if (global_player && global_player->get_media_input().audio_streams() > 1) {
            int s = parameters().audio_stream() + 1;
            if (s >= global_player->get_media_input().audio_streams())
                s = 0;
            params.set_audio_stream(global_player->set_audio_stream(s));
            notify_all(notification::audio_stream);
        }
        break;
    case command::set_audio_stream:
        if (global_player && global_player->get_media_input().audio_streams() > 1) {
            params.set_audio_stream(global_player->set_audio_stream(s11n::load<int>(p)));
            notify_all(notification::audio_stream);
        }
        break;
    case command::cycle_subtitle_stream:
        if (global_player && global_player->get_media_input().subtitle_streams() > 0) {
            int s = parameters().subtitle_stream() + 1;
            if (s >= global_player->get_media_input().subtitle_streams())
                s = -1;
            params.set_subtitle_stream(global_player->set_subtitle_stream(s));
            notify_all(notification::subtitle_stream);
        }
        break;
    case command::set_subtitle_stream:
        if (global_player && global_player->get_media_input().subtitle_streams() > 0) {
            params.set_subtitle_stream(global_player->set_subtitle_stream(s11n::load<int>(p)));
            notify_all(notification::subtitle_stream);
        }
        break;
    case command::set_stereo_layout:
        params.set_stereo_layout(static_cast<parameters::stereo_layout_t>(s11n::load<int>(p)));
        if (global_player)
            global_player->set_stereo_layout(params.stereo_layout());
        parameters_changed = true;
        notify_all(notification::stereo_layout);
        break;
    case command::set_stereo_layout_swap:
        params.set_stereo_layout_swap(s11n::load<bool>(p));
        if (global_player)
            global_player->set_stereo_layout_swap(params.stereo_layout_swap());
        parameters_changed = true;
        notify_all(notification::stereo_layout_swap);
        break;
    case command::set_stereo_mode:
        params.set_stereo_mode(static_cast<parameters::stereo_mode_t>(s11n::load<int>(p)));
        parameters_changed = true;
        notify_all(notification::stereo_mode);
        break;
    case command::set_stereo_mode_swap:
        params.set_stereo_mode_swap(s11n::load<bool>(p));
        parameters_changed = true;
        notify_all(notification::stereo_mode_swap);
        break;
    case command::toggle_stereo_mode_swap:
        params.set_stereo_mode_swap(!params.stereo_mode_swap());
        parameters_changed = true;
        notify_all(notification::stereo_mode_swap);
        break;
    case command::toggle_fullscreen:
        {
            bool fs = false;
            if (global_player)
                fs = global_player->set_fullscreen(!params.fullscreen());
            params.set_fullscreen(fs);
        }
        parameters_changed = true;
        notify_all(notification::fullscreen);
        break;
    case command::center:
        if (global_player)
            global_player->center();
        params.set_center(true);
        notify_all(notification::center);
        break;
    case command::toggle_pause:
        if (global_player)
            global_player->set_pause(!global_dispatch->pausing());
        /* notify when request is fulfilled */
        break;
    case command::adjust_contrast:
        params.set_contrast(clamp(params.contrast() + s11n::load<float>(p), -1.0f, +1.0f));
        parameters_changed = true;
        notify_all(notification::contrast);
        break;
    case command::set_contrast:
        params.set_contrast(clamp(s11n::load<float>(p), -1.0f, +1.0f));
        parameters_changed = true;
        notify_all(notification::contrast);
        break;
    case command::adjust_brightness:
        params.set_brightness(clamp(params.brightness() + s11n::load<float>(p), -1.0f, +1.0f));
        parameters_changed = true;
        notify_all(notification::brightness);
        break;
    case command::set_brightness:
        params.set_brightness(clamp(s11n::load<float>(p), -1.0f, +1.0f));
        parameters_changed = true;
        notify_all(notification::brightness);
        break;
    case command::adjust_hue:
        params.set_hue(clamp(params.hue() + s11n::load<float>(p), -1.0f, +1.0f));
        parameters_changed = true;
        notify_all(notification::hue);
        break;
    case command::set_hue:
        params.set_hue(clamp(s11n::load<float>(p), -1.0f, +1.0f));
        parameters_changed = true;
        notify_all(notification::hue);
        break;
    case command::adjust_saturation:
        params.set_saturation(clamp(params.saturation() + s11n::load<float>(p), -1.0f, +1.0f));
        parameters_changed = true;
        notify_all(notification::saturation);
        break;
    case command::set_saturation:
        params.set_saturation(clamp(s11n::load<float>(p), -1.0f, +1.0f));
        parameters_changed = true;
        notify_all(notification::saturation);
        break;
    case command::adjust_parallax:
        params.set_parallax(clamp(params.contrast() + s11n::load<float>(p), -1.0f, +1.0f));
        parameters_changed = true;
        notify_all(notification::parallax);
        break;
    case command::set_parallax:
        params.set_parallax(clamp(s11n::load<float>(p), -1.0f, +1.0f));
        parameters_changed = true;
        notify_all(notification::parallax);
        break;
    case command::set_crosstalk:
        params.set_crosstalk_r(clamp(s11n::load<float>(p), -1.0f, +1.0f));
        params.set_crosstalk_g(clamp(s11n::load<float>(p), -1.0f, +1.0f));
        params.set_crosstalk_b(clamp(s11n::load<float>(p), -1.0f, +1.0f));
        parameters_changed = true;
        notify_all(notification::crosstalk);
        break;
    case command::adjust_ghostbust:
        params.set_ghostbust(clamp(params.ghostbust() + s11n::load<float>(p), 0.0f, 1.0f));
        parameters_changed = true;
        notify_all(notification::ghostbust);
        break;
    case command::set_ghostbust:
        params.set_ghostbust(clamp(s11n::load<float>(p), 0.0f, 1.0f));
        parameters_changed = true;
        notify_all(notification::ghostbust);
        break;
    case command::set_subtitle_encoding:
        params.set_subtitle_encoding(s11n::load<std::string>(p));
        parameters_changed = true;
        notify_all(notification::subtitle_encoding);
        break;
    case command::set_subtitle_font:
        params.set_subtitle_font(s11n::load<std::string>(p));
        parameters_changed = true;
        notify_all(notification::subtitle_font);
        break;
    case command::set_subtitle_size:
        params.set_subtitle_size(s11n::load<int>(p));
        parameters_changed = true;
        notify_all(notification::subtitle_size);
        break;
    case command::set_subtitle_scale:
        params.set_subtitle_scale(std::max(s11n::load<float>(p), 0.0f));
        parameters_changed = true;
        notify_all(notification::subtitle_scale);
        break;
    case command::set_subtitle_color:
        params.set_subtitle_color(s11n::load<uint64_t>(p));
        parameters_changed = true;
        notify_all(notification::subtitle_color);
        break;
    case command::adjust_subtitle_parallax:
        params.set_subtitle_parallax(clamp(params.contrast() + s11n::load<float>(p), -1.0f, +1.0f));
        parameters_changed = true;
        notify_all(notification::subtitle_parallax);
        break;
    case command::set_subtitle_parallax:
        params.set_subtitle_parallax(clamp(s11n::load<float>(p), -1.0f, +1.0f));
        parameters_changed = true;
        notify_all(notification::subtitle_parallax);
        break;
    case command::seek:
        if (global_player)
            global_player->seek(s11n::load<float>(p) * 1e6f);
        /* notify when request is fulfilled */
        break;
    case command::set_pos:
        if (global_player)
            global_player->set_pos(s11n::load<float>(p));
        /* notify when request is fulfilled */
        break;
    case command::set_loop_mode:
        params.set_loop_mode(static_cast<parameters::loop_mode_t>(s11n::load<int>(p)));
        parameters_changed = true;
        notify_all(notification::loop_mode);
        break;
    case command::set_fullscreen_screens:
        params.set_fullscreen_screens(s11n::load<int>(p));
        parameters_changed = true;
        notify_all(notification::fullscreen_screens);
        break;
    case command::set_fullscreen_flip_left:
        params.set_fullscreen_flip_left(s11n::load<bool>(p));
        parameters_changed = true;
        notify_all(notification::fullscreen_flip_left);
        break;
    case command::set_fullscreen_flip_right:
        params.set_fullscreen_flip_right(s11n::load<bool>(p));
        parameters_changed = true;
        notify_all(notification::fullscreen_flip_right);
        break;
    case command::set_fullscreen_flop_left:
        params.set_fullscreen_flop_left(s11n::load<bool>(p));
        parameters_changed = true;
        notify_all(notification::fullscreen_flop_left);
        break;
    case command::set_fullscreen_flop_right:
        params.set_fullscreen_flop_right(s11n::load<bool>(p));
        parameters_changed = true;
        notify_all(notification::fullscreen_flop_right);
        break;
    case command::adjust_zoom:
        params.set_zoom(clamp(params.zoom() + s11n::load<float>(p), 0.0f, 1.0f));
        parameters_changed = true;
        notify_all(notification::zoom);
        break;
    case command::set_zoom:
        params.set_zoom(clamp(s11n::load<float>(p), 0.0f, 1.0f));
        parameters_changed = true;
        notify_all(notification::zoom);
        break;
    case command::set_crop_aspect_ratio:
        {
            float x = s11n::load<float>(p);
            params.set_crop_aspect_ratio((x <= 0.0f ? 0.0f : clamp(x, 1.0f, 2.39f)));
        }
        parameters_changed = true;
        notify_all(notification::crop_aspect_ratio);
        break;
    case command::set_audio_device:
        params.set_audio_device(s11n::load<int>(p));
        parameters_changed = true;
        notify_all(notification::audio_device);
        break;
    case command::adjust_audio_volume:
        params.set_audio_volume(clamp(params.audio_volume() + s11n::load<float>(p), 0.0f, 1.0f));
        parameters_changed = true;
        notify_all(notification::audio_volume);
        break;
    case command::set_audio_volume:
        params.set_audio_volume(clamp(s11n::load<float>(p), 0.0f, 1.0f));
        parameters_changed = true;
        notify_all(notification::audio_volume);
        break;
    case command::toggle_audio_mute:
        params.set_audio_mute(!params.audio_mute());
        parameters_changed = true;
        notify_all(notification::audio_mute);
        break;
    case command::set_audio_delay:
        params.set_audio_delay(s11n::load<int64_t>(p));
        parameters_changed = true;
        notify_all(notification::audio_delay);
        break;
    }

    if (parameters_changed && global_player)
        global_player->trigger_video_output_update();
}

void dispatch::set_video_parameters(const class parameters& p)
{
    class parameters& params = global_dispatch->_parameters;
    params.unset_video_parameters();
    if (p.video_stream_is_set())
        params.set_video_stream(p.video_stream());
    if (p.audio_stream_is_set())
        params.set_audio_stream(p.video_stream());
    if (p.subtitle_stream_is_set())
        params.set_subtitle_stream(p.video_stream());
    if (p.stereo_layout_is_set())
        params.set_stereo_layout(p.stereo_layout());
    if (p.stereo_layout_swap_is_set())
        params.set_stereo_layout_swap(p.stereo_layout_swap());
    if (p.crop_aspect_ratio_is_set())
        params.set_crop_aspect_ratio(p.crop_aspect_ratio());
    if (p.parallax_is_set())
        params.set_parallax(p.parallax());
    if (p.ghostbust_is_set())
        params.set_ghostbust(p.ghostbust());
    if (p.subtitle_parallax_is_set())
        params.set_subtitle_parallax(p.subtitle_parallax());
    notify_all(notification::video_stream);
    notify_all(notification::audio_stream);
    notify_all(notification::subtitle_stream);
    notify_all(notification::stereo_layout);
    notify_all(notification::stereo_layout_swap);
    notify_all(notification::crop_aspect_ratio);
    notify_all(notification::parallax);
    notify_all(notification::ghostbust);
    notify_all(notification::subtitle_parallax);
}
