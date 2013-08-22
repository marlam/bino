/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010, 2011, 2012, 2013
 * Martin Lambers <marlam@marlam.de>
 * Binocle <http://binocle.com> (author: Olivier Letz <oletz@binocle.com>)
 * Frédéric Bour <frederic.bour@lakaban.net>
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
#include <sstream>
#include <cstdio>
#include <cctype>
#include <unistd.h>

#include "gettext.h"
#define _(string) gettext(string)

#include "exc.h"
#include "dbg.h"
#include "msg.h"
#include "str.h"
#include "timer.h"
#include "thread.h"

#include "gui.h"
#include "audio_output.h"
#include "video_output_qt.h"
#include "media_input.h"
#include "player.h"
#if HAVE_LIBEQUALIZER
# include "player_equalizer.h"
#endif
#include "dispatch.h"


// Global objects
dispatch* global_dispatch = NULL;


open_input_data::open_input_data() :
    dev_request(), urls(), params()
{
}

void open_input_data::save(std::ostream &os) const
{
    s11n::save(os, dev_request);
    s11n::save(os, urls);
    s11n::save(os, params);
}

void open_input_data::load(std::istream &is)
{
    s11n::load(is, dev_request);
    s11n::load(is, urls);
    s11n::load(is, params);
}


controller::controller() throw ()
{
    assert(global_dispatch);
    global_dispatch->register_controller(this);
}

controller::~controller()
{
    assert(global_dispatch);
    global_dispatch->deregister_controller(this);
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

bool controller::allow_early_quit()
{
    /* default: allow to quit when there is nothing to do */
    return true;
}


dispatch::dispatch(int* argc, char** argv,
        bool equalizer, bool equalizer_3d, bool equalizer_slave_node,
        bool gui, bool have_display, msg::level_t log_level,
        bool benchmark, int swap_interval) throw () :
    _argc(argc), _argv(argv),
    _eq(equalizer), _eq_3d(equalizer_3d), _eq_slave_node(equalizer_slave_node),
    _gui_mode(gui), _have_display(have_display),
    _gui(NULL), _audio_output(NULL), _video_output(NULL), _media_input(NULL), _player(NULL),
    _controllers_version(0),
    _playing(false), _pausing(false), _position(0.0f)
{
    assert(!global_dispatch);
    global_dispatch = this;
    _parameters.set_log_level(log_level);
    msg::set_level(log_level);
    _parameters.set_benchmark(benchmark);
    _parameters.set_swap_interval(swap_interval);
}

dispatch::~dispatch()
{
    deinit();
    global_dispatch = NULL;
}

void dispatch::register_controller(controller* c)
{
    // Note about thread safety:
    // Only the registering and deregistering a controller are thread safe.
    // The visitation of controllers and the actions performed as a result
    // are supposed to happen inside a single thread.
    _controllers_mutex.lock();
    _controllers.push_back(c);
    _controllers_version++;
    _controllers_mutex.unlock();
}

void dispatch::deregister_controller(controller* c)
{
    _controllers_mutex.lock();
    for (size_t i = 0; i < _controllers.size(); i++) {
        if (_controllers[i] == c) {
            _controllers.erase(_controllers.begin() + i);
            break;
        }
    }
    _controllers_version++;
    _controllers_mutex.unlock();
}

void dispatch::init(const open_input_data& input_data)
{
    if (_eq) {
        if (!_eq_slave_node && !_parameters.benchmark())
            _audio_output = new class audio_output;
    } else if (!_have_display) {
        throw exc(_("Cannot connect to X server."));
    } else if (_gui_mode) {
        _gui = new gui();
        if (!_parameters.benchmark())
            _audio_output = new class audio_output;
        _video_output = new video_output_qt(_gui->container_widget());
        if (input_data.urls.size() > 0)
            _gui->open(input_data);
    } else {
        if (!_parameters.benchmark())
            _audio_output = new class audio_output;
        _video_output = new video_output_qt();
    }
    if ((_eq && !_eq_slave_node) || !_gui_mode) {
        if (input_data.urls.size() == 0) {
            if (early_quit_is_allowed())
                throw exc(_("No video to play."));
        } else {
            std::ostringstream v;
            s11n::save(v, input_data);
            controller::send_cmd(command::open, v.str());
            controller::send_cmd(command::toggle_play);
        }
    }
}

void dispatch::deinit()
{
    force_stop();
    delete _player;
    _player = NULL;
    delete _media_input;
    _media_input = NULL;
    delete _video_output;
    _video_output = NULL;
    delete _audio_output;
    _audio_output = NULL;
    delete _gui;
    _gui = NULL;
}

void dispatch::step()
{
    assert(global_dispatch);
    if (global_dispatch->_playing) {
        if (!global_dispatch->_player->run_step())
            global_dispatch->stop_player();
    } else {
        usleep(1000);
    }
}

bool dispatch::early_quit_is_allowed() const
{
    for (size_t i = 0; i < _controllers.size(); i++)
        if (!_controllers[i]->allow_early_quit())
            return false;
    return true;
}

void dispatch::visit_all_controllers(int action, const notification& note) const
{
    std::vector<controller*> visited_controllers;
    visited_controllers.reserve(_controllers.size());

    // First, try to visit all controllers in one row without extra checks. This
    // only works as long as controllers do not vanish or appear as a result of
    // the function we call. This is the common case.
    unsigned int controllers_version = _controllers_version;
    for (size_t i = 0; i < _controllers.size(); i++) {
        controller *c = _controllers[i];
        if (action == 0) {
            c->process_events();
        } else {
            c->receive_notification(note);
        }
        visited_controllers.push_back(c);
        if (controllers_version != _controllers_version) {
            break;
        }
    }
    // If some controllers vanished or appeared, we need to redo the loop and
    // check for each controller if it was visited before. This is costly, but
    // it happens seldomly.
    while (controllers_version != _controllers_version)
    {
        controllers_version = _controllers_version;
        for (size_t i = 0; i < _controllers.size(); i++) {
            controller *c = _controllers[i];
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
                if (controllers_version != _controllers_version) {
                    break;
                }
            }
        }
    }
}

void dispatch::notify_all(const notification& note)
{
    visit_all_controllers(1, note);
}

void dispatch::process_all_events()
{
    assert(global_dispatch);
    global_dispatch->visit_all_controllers(0, notification::noop);
}

class audio_output* dispatch::get_audio_output()
{
    return _audio_output;
}

class video_output* dispatch::get_video_output()
{
    return _video_output;
}

class media_input* dispatch::get_media_input()
{
    return _media_input;
}

class open_input_data* dispatch::get_input_data()
{
    return &_input_data;
}

void dispatch::set_playing(bool p)
{
    _playing = p;
    notify_all(notification::play);
}

void dispatch::set_pausing(bool p)
{
    _pausing = p;
    notify_all(notification::pause);
}

void dispatch::set_position(float pos)
{
    _position = pos;
    notify_all(notification::pos);
}

const class parameters& dispatch::parameters()
{
    assert(global_dispatch);
    return global_dispatch->_parameters;
}

const class media_input* dispatch::media_input()
{
    assert(global_dispatch);
    return global_dispatch->_media_input;
}

const class video_output* dispatch::video_output()
{
    assert(global_dispatch);
    return global_dispatch->_video_output;
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
    float p = 0.0f;
    if (dispatch::playing()) {
        assert(global_dispatch->_player);
        p = global_dispatch->_player->get_pos();
    }
    return p;
}

static float clamp(float x, float lo, float hi)
{
    return std::min(std::max(x, lo), hi);
}

void dispatch::stop_player()
{
    bool early_quit = !_gui_mode && early_quit_is_allowed();
    force_stop(!early_quit);
    notify_all(notification::play);
    if (early_quit)
        notify_all(notification::quit);
}

void dispatch::force_stop(bool reopen_media_input)
{
    if (_player) {
        _player->close();
        if (!_eq)
            delete _player;     // The equalizer code will delete the player object at the appropriate time
        _player = NULL;
    }
    if (_media_input) {
        // close, reopen, and reinitialize media input
        _media_input->close();
        if (reopen_media_input) {
            _media_input->open(_input_data.urls, _input_data.dev_request);
            _media_input->set_stereo_layout(_parameters.stereo_layout(), _parameters.stereo_layout_swap());
            _media_input->select_video_stream(_parameters.video_stream());
            if (_media_input->audio_streams() > 0)
                _media_input->select_audio_stream(_parameters.audio_stream());
            if (_media_input->subtitle_streams() > 0 && _parameters.subtitle_stream() >= 0)
                _media_input->select_subtitle_stream(_parameters.subtitle_stream());
        } else {
            delete _media_input;
            _media_input = NULL;
        }
    }
    if (_video_output) {
        _video_output->deinit();
    }
    if (_audio_output) {
        _audio_output->deinit();
    }
    _playing = false;
    _pausing = false;
}

void dispatch::receive_cmd(const command& cmd)
{
    std::istringstream p(cmd.param);

    switch (cmd.type)
    {
    case command::noop:
        break;
    case command::quit:
        force_stop(false);
        notify_all(notification::quit);
        break;
    // Play state
    case command::open:
        force_stop(false);
        notify_all(notification::play);
        s11n::load(p, _input_data);
        // Create media input
        try {
            _media_input = new class media_input;
            _media_input->open(_input_data.urls, _input_data.dev_request);
            if (_media_input->video_streams() == 0) {
                throw exc(_("No video streams found."));
            }
        }
        catch (exc& e) {
            delete _media_input;
            _media_input = NULL;
            throw e;
        }
        catch (std::exception& e) {
            delete _media_input;
            _media_input = NULL;
            throw e;
        }
        _parameters.unset_video_parameters();
        // Initialize media input and associated parameters
        if (_input_data.params.stereo_layout_is_set() || _input_data.params.stereo_layout_swap_is_set()) {
            if (!_media_input->stereo_layout_is_supported(_input_data.params.stereo_layout(), _input_data.params.stereo_layout_swap())) {
                throw exc(_("Cannot set requested stereo layout: incompatible media."));
            }
            _media_input->set_stereo_layout(_input_data.params.stereo_layout(),
                    _input_data.params.stereo_layout_swap());
            _parameters.set_stereo_layout(_input_data.params.stereo_layout());
            _parameters.set_stereo_layout_swap(_input_data.params.stereo_layout_swap());
        } else {
            _media_input->set_stereo_layout(_media_input->video_frame_template().stereo_layout,
                    _media_input->video_frame_template().stereo_layout_swap);
            _parameters.set_stereo_layout(_media_input->video_frame_template().stereo_layout);
            _parameters.set_stereo_layout_swap(_media_input->video_frame_template().stereo_layout_swap);
        }
        notify_all(notification::stereo_layout);
        notify_all(notification::stereo_layout_swap);
        if (_media_input->video_streams() < _input_data.params.video_stream() + 1) {
            throw exc(str::asprintf(_("Video stream %d not found."), _input_data.params.video_stream() + 1));
        }
        _media_input->select_video_stream(_input_data.params.video_stream());
        _parameters.set_video_stream(_input_data.params.video_stream());
        notify_all(notification::video_stream);
        if (_media_input->audio_streams() > 0 && _media_input->audio_streams() < _input_data.params.audio_stream() + 1) {
            throw exc(str::asprintf(_("Audio stream %d not found."), _input_data.params.audio_stream() + 1));
        }
        if (_media_input->audio_streams() > 0) {
            _media_input->select_audio_stream(_input_data.params.audio_stream());
            _parameters.set_audio_stream(_input_data.params.audio_stream());
        }
        notify_all(notification::audio_stream);
        if (_media_input->subtitle_streams() > 0 && _media_input->subtitle_streams() < _input_data.params.subtitle_stream() + 1) {
            throw exc(str::asprintf(_("Subtitle stream %d not found."), _input_data.params.subtitle_stream() + 1));
        }
        if (_media_input->subtitle_streams() > 0 && _input_data.params.subtitle_stream() >= 0) {
            _media_input->select_subtitle_stream(_input_data.params.subtitle_stream());
            _parameters.set_subtitle_stream(_input_data.params.subtitle_stream());
        }
        notify_all(notification::subtitle_stream);
        // Initialize remaining parameters
        if (_input_data.params.crop_aspect_ratio_is_set())
            _parameters.set_crop_aspect_ratio(_input_data.params.crop_aspect_ratio());
        notify_all(notification::crop_aspect_ratio);
        if (_input_data.params.source_aspect_ratio_is_set())
            _parameters.set_source_aspect_ratio(_input_data.params.source_aspect_ratio());
        notify_all(notification::source_aspect_ratio);
        if (_input_data.params.parallax_is_set())
            _parameters.set_parallax(_input_data.params.parallax());
        notify_all(notification::parallax);
        if (_input_data.params.ghostbust_is_set())
            _parameters.set_ghostbust(_input_data.params.ghostbust());
        notify_all(notification::ghostbust);
        if (_input_data.params.subtitle_parallax_is_set())
            _parameters.set_subtitle_parallax(_input_data.params.subtitle_parallax());
        notify_all(notification::subtitle_parallax);
        if (!_parameters.stereo_mode_is_set()) {
            if (_media_input->video_frame_template().stereo_layout == parameters::layout_mono)
                _parameters.set_stereo_mode(parameters::mode_mono_left);
            else if (_video_output && _video_output->supports_stereo())
                _parameters.set_stereo_mode(parameters::mode_stereo);
            else
                _parameters.set_stereo_mode(parameters::mode_red_cyan_dubois);
            _parameters.set_stereo_mode_swap(false);
        }
        notify_all(notification::stereo_mode);
        notify_all(notification::stereo_mode_swap);
        notify_all(notification::open);
        break;
    case command::close:
        force_stop(false);
        notify_all(notification::play);
        notify_all(notification::open);
        break;
    case command::toggle_play:
        if (playing()) {
            assert(_player);
            _player->quit_request();
            /* notify when request is fulfilled */
        } else if (_media_input) {
            // Initialize audio output
            if (_media_input->audio_streams() > 0 && _audio_output) {
                _audio_output->deinit();
                _audio_output->init(_parameters.audio_device());
            }
            // Initialize video output
            if (_video_output) {
                _video_output->deinit();
                _video_output->init();
            }
            // Set initial parameters
            if (_video_output) {
                _video_output->set_suitable_size(
                        _media_input->video_frame_template().width,
                        _media_input->video_frame_template().height,
                        _parameters.crop_aspect_ratio() > 0.0f
                        ? _parameters.crop_aspect_ratio()
                        : _media_input->video_frame_template().aspect_ratio,
                        _parameters.stereo_mode());
                if (_parameters.fullscreen())
                    _video_output->enter_fullscreen();
                if (_parameters.center() && !_gui_mode)
                    _video_output->center();
            }
            if (_eq) {
#if HAVE_LIBEQUALIZER
                _player = new class player_equalizer(_argc, _argv, !_eq_3d);
#else
                throw exc(_("This version of Bino was compiled without support for Equalizer."));
#endif
            } else {
                _player = new player;
            }
            _player->open();
            _playing = true;
            notify_all(notification::play);
        }
        break;
    case command::toggle_pause:
        if (_player)
            _player->set_pause(!pausing());
        /* notify when request is fulfilled */
        break;
    case command::step:
        if (_player)
            _player->set_step(true);
        break;
    case command::seek:
        if (_player)
            _player->seek(s11n::load<float>(p) * 1e6f);
        /* notify when request is fulfilled */
        break;
    case command::set_pos:
        if (_player)
            _player->set_pos(s11n::load<float>(p));
        /* notify when request is fulfilled */
        break;
    // Per-Session parameters
    case command::set_audio_device:
        _parameters.set_audio_device(s11n::load<int>(p));
        notify_all(notification::audio_device);
        break;
    case command::set_quality:
        _parameters.set_quality(s11n::load<int>(p));
        notify_all(notification::quality);
        break;
    case command::set_stereo_mode:
        _parameters.set_stereo_mode(static_cast<parameters::stereo_mode_t>(s11n::load<int>(p)));
        notify_all(notification::stereo_mode);
        break;
    case command::set_stereo_mode_swap:
        _parameters.set_stereo_mode_swap(s11n::load<bool>(p));
        notify_all(notification::stereo_mode_swap);
        break;
    case command::toggle_stereo_mode_swap:
        _parameters.set_stereo_mode_swap(!_parameters.stereo_mode_swap());
        notify_all(notification::stereo_mode_swap);
        break;
    case command::set_crosstalk:
        _parameters.set_crosstalk_r(clamp(s11n::load<float>(p), -1.0f, +1.0f));
        _parameters.set_crosstalk_g(clamp(s11n::load<float>(p), -1.0f, +1.0f));
        _parameters.set_crosstalk_b(clamp(s11n::load<float>(p), -1.0f, +1.0f));
        notify_all(notification::crosstalk);
        break;
    case command::set_fullscreen_screens:
        _parameters.set_fullscreen_screens(s11n::load<int>(p));
        notify_all(notification::fullscreen_screens);
        break;
    case command::set_fullscreen_flip_left:
        _parameters.set_fullscreen_flip_left(s11n::load<bool>(p));
        notify_all(notification::fullscreen_flip_left);
        break;
    case command::set_fullscreen_flip_right:
        _parameters.set_fullscreen_flip_right(s11n::load<bool>(p));
        notify_all(notification::fullscreen_flip_right);
        break;
    case command::set_fullscreen_flop_left:
        _parameters.set_fullscreen_flop_left(s11n::load<bool>(p));
        notify_all(notification::fullscreen_flop_left);
        break;
    case command::set_fullscreen_flop_right:
        _parameters.set_fullscreen_flop_right(s11n::load<bool>(p));
        notify_all(notification::fullscreen_flop_right);
        break;
    case command::set_fullscreen_inhibit_screensaver:
        _parameters.set_fullscreen_inhibit_screensaver(s11n::load<bool>(p));
        notify_all(notification::fullscreen_inhibit_screensaver);
        break;
    case command::set_fullscreen_3d_ready_sync:
        _parameters.set_fullscreen_3d_ready_sync(s11n::load<bool>(p));
        notify_all(notification::fullscreen_3d_ready_sync);
        break;
    case command::adjust_contrast:
        _parameters.set_contrast(clamp(_parameters.contrast() + s11n::load<float>(p), -1.0f, +1.0f));
        notify_all(notification::contrast);
        break;
    case command::set_contrast:
        _parameters.set_contrast(clamp(s11n::load<float>(p), -1.0f, +1.0f));
        notify_all(notification::contrast);
        break;
    case command::adjust_brightness:
        _parameters.set_brightness(clamp(_parameters.brightness() + s11n::load<float>(p), -1.0f, +1.0f));
        notify_all(notification::brightness);
        break;
    case command::set_brightness:
        _parameters.set_brightness(clamp(s11n::load<float>(p), -1.0f, +1.0f));
        notify_all(notification::brightness);
        break;
    case command::adjust_hue:
        _parameters.set_hue(clamp(_parameters.hue() + s11n::load<float>(p), -1.0f, +1.0f));
        notify_all(notification::hue);
        break;
    case command::set_hue:
        _parameters.set_hue(clamp(s11n::load<float>(p), -1.0f, +1.0f));
        notify_all(notification::hue);
        break;
    case command::adjust_saturation:
        _parameters.set_saturation(clamp(_parameters.saturation() + s11n::load<float>(p), -1.0f, +1.0f));
        notify_all(notification::saturation);
        break;
    case command::set_saturation:
        _parameters.set_saturation(clamp(s11n::load<float>(p), -1.0f, +1.0f));
        notify_all(notification::saturation);
        break;
    case command::adjust_zoom:
        _parameters.set_zoom(clamp(_parameters.zoom() + s11n::load<float>(p), 0.0f, 1.0f));
        notify_all(notification::zoom);
        break;
    case command::set_zoom:
        _parameters.set_zoom(clamp(s11n::load<float>(p), 0.0f, 1.0f));
        notify_all(notification::zoom);
        break;
    case command::set_loop_mode:
        _parameters.set_loop_mode(static_cast<parameters::loop_mode_t>(s11n::load<int>(p)));
        notify_all(notification::loop_mode);
        break;
    case command::set_audio_delay:
        _parameters.set_audio_delay(s11n::load<int64_t>(p));
        notify_all(notification::audio_delay);
        break;
    case command::set_subtitle_encoding:
        _parameters.set_subtitle_encoding(s11n::load<std::string>(p));
        notify_all(notification::subtitle_encoding);
        break;
    case command::set_subtitle_font:
        _parameters.set_subtitle_font(s11n::load<std::string>(p));
        notify_all(notification::subtitle_font);
        break;
    case command::set_subtitle_size:
        _parameters.set_subtitle_size(s11n::load<int>(p));
        notify_all(notification::subtitle_size);
        break;
    case command::set_subtitle_scale:
        _parameters.set_subtitle_scale(s11n::load<float>(p));
        notify_all(notification::subtitle_scale);
        break;
    case command::set_subtitle_color:
        _parameters.set_subtitle_color(s11n::load<uint64_t>(p));
        notify_all(notification::subtitle_color);
        break;
    case command::set_subtitle_shadow:
        _parameters.set_subtitle_shadow(s11n::load<int>(p));
        notify_all(notification::subtitle_shadow);
        break;
#if HAVE_LIBXNVCTRL
    case command::set_sdi_output_format:
        _parameters.set_sdi_output_format(s11n::load<int>(p));
        notify_all(notification::sdi_output_format);
        break;
    case command::set_sdi_output_left_stereo_mode:
        _parameters.set_sdi_output_left_stereo_mode(static_cast<parameters::stereo_mode_t>(s11n::load<int>(p)));
        notify_all(notification::sdi_output_left_stereo_mode);
        break;
    case command::set_sdi_output_right_stereo_mode:
        _parameters.set_sdi_output_right_stereo_mode(static_cast<parameters::stereo_mode_t>(s11n::load<int>(p)));
        notify_all(notification::sdi_output_right_stereo_mode);
        break;
#endif // HAVE_LIBXNVCTRL
    // Per-Video parameters
    case command::cycle_video_stream:
        if (_media_input && _media_input->video_streams() > 1
                && parameters().stereo_layout() != parameters::layout_separate) {
            int s = parameters().video_stream() + 1;
            if (s >= _media_input->video_streams())
                s = 0;
            s = _player->set_video_stream(s);
            _parameters.set_video_stream(s);
            notify_all(notification::video_stream);
        }
        break;
    case command::set_video_stream:
        if (_media_input && _media_input->video_streams() > 1
                && parameters().stereo_layout() != parameters::layout_separate) {
            int s = s11n::load<int>(p);
            s = _player->set_video_stream(s);
            _parameters.set_video_stream(s);
            notify_all(notification::video_stream);
        }
        break;
    case command::cycle_audio_stream:
        if (_media_input && _media_input->audio_streams() > 1) {
            int s = parameters().audio_stream() + 1;
            if (s >= _media_input->audio_streams())
                s = 0;
            s = _player->set_audio_stream(s);
            _parameters.set_audio_stream(s);
            notify_all(notification::audio_stream);
        }
        break;
    case command::set_audio_stream:
        if (_media_input && _media_input->audio_streams() > 1) {
            int s = s11n::load<int>(p);
            s = _player->set_audio_stream(s);
            _parameters.set_audio_stream(s);
            notify_all(notification::audio_stream);
        }
        break;
    case command::cycle_subtitle_stream:
        if (_media_input && _media_input->subtitle_streams() > 0) {
            int s = parameters().subtitle_stream() + 1;
            if (s >= _media_input->subtitle_streams())
                s = -1;
            s = _player->set_subtitle_stream(s);
            _parameters.set_subtitle_stream(s);
            notify_all(notification::subtitle_stream);
        }
        break;
    case command::set_subtitle_stream:
        if (_media_input && _media_input->subtitle_streams() > 0) {
            int s = s11n::load<int>(p);
            s = _player->set_subtitle_stream(s);
            _parameters.set_subtitle_stream(s);
            notify_all(notification::subtitle_stream);
        }
        break;
    case command::set_stereo_layout:
        _parameters.set_stereo_layout(static_cast<parameters::stereo_layout_t>(s11n::load<int>(p)));
        if (_player)
            _player->set_stereo_layout(_parameters.stereo_layout());
        notify_all(notification::stereo_layout);
        break;
    case command::set_stereo_layout_swap:
        _parameters.set_stereo_layout_swap(s11n::load<bool>(p));
        if (_player)
            _player->set_stereo_layout_swap(_parameters.stereo_layout_swap());
        notify_all(notification::stereo_layout_swap);
        break;
    case command::set_crop_aspect_ratio:
        {
            float x = s11n::load<float>(p);
            _parameters.set_crop_aspect_ratio((x <= 0.0f ? 0.0f : clamp(x, 1.0f, 2.39f)));
        }
        notify_all(notification::crop_aspect_ratio);
        break;
    case command::set_source_aspect_ratio:
        {
            float x = s11n::load<float>(p);
            _parameters.set_source_aspect_ratio((x <= 0.0f ? 0.0f : clamp(x, 1.0f, 2.39f)));
        }
        notify_all(notification::source_aspect_ratio);
        break;
    case command::adjust_parallax:
        _parameters.set_parallax(clamp(_parameters.parallax() + s11n::load<float>(p), -1.0f, +1.0f));
        notify_all(notification::parallax);
        break;
    case command::set_parallax:
        _parameters.set_parallax(clamp(s11n::load<float>(p), -1.0f, +1.0f));
        notify_all(notification::parallax);
        break;
    case command::adjust_ghostbust:
        _parameters.set_ghostbust(clamp(_parameters.ghostbust() + s11n::load<float>(p), 0.0f, 1.0f));
        notify_all(notification::ghostbust);
        break;
    case command::set_ghostbust:
        _parameters.set_ghostbust(clamp(s11n::load<float>(p), 0.0f, 1.0f));
        notify_all(notification::ghostbust);
        break;
    case command::adjust_subtitle_parallax:
        _parameters.set_subtitle_parallax(clamp(_parameters.subtitle_parallax() + s11n::load<float>(p), -1.0f, +1.0f));
        notify_all(notification::subtitle_parallax);
        break;
    case command::set_subtitle_parallax:
        _parameters.set_subtitle_parallax(clamp(s11n::load<float>(p), -1.0f, +1.0f));
        notify_all(notification::subtitle_parallax);
        break;
    // Volatile parameters
    case command::toggle_fullscreen:
        {
            if (playing()) {
                bool fs = false;
                if (_video_output) {
                    if (_parameters.fullscreen()) {
                    _video_output->exit_fullscreen();
                    fs = false;
                    } else {
                        _video_output->enter_fullscreen();
                        fs = true;
                    }
                }
                _parameters.set_fullscreen(fs);
            } else {
                _parameters.set_fullscreen(!_parameters.fullscreen());
            }
            notify_all(notification::fullscreen);
        }
        break;
    case command::center:
        if (_video_output)
            _video_output->center();
        _parameters.set_center(true);
        notify_all(notification::center);
        break;
    case command::adjust_audio_volume:
        _parameters.set_audio_volume(clamp(_parameters.audio_volume() + s11n::load<float>(p), 0.0f, 1.0f));
        notify_all(notification::audio_volume);
        break;
    case command::set_audio_volume:
        _parameters.set_audio_volume(clamp(s11n::load<float>(p), 0.0f, 1.0f));
        notify_all(notification::audio_volume);
        break;
    case command::toggle_audio_mute:
        _parameters.set_audio_mute(!_parameters.audio_mute());
        notify_all(notification::audio_mute);
        break;
    case command::update_display_pos:
        notify_all(notification::display_pos);
        break;
    }
}

std::string dispatch::save_state() const
{
    std::ostringstream oss;
    s11n::save(oss, _input_data);
    s11n::save(oss, _parameters);
    s11n::save(oss, _playing);
    s11n::save(oss, _pausing);
    s11n::save(oss, _position);
    return oss.str();
}

void dispatch::load_state(const std::string& s)
{
    std::istringstream iss(s);
    s11n::load(iss, _input_data);
    s11n::load(iss, _parameters);
    s11n::load(iss, _playing);
    s11n::load(iss, _pausing);
    s11n::load(iss, _position);
}

static bool parse_bool(const std::string& s, bool* x)
{
    if (s == "1" || s == "on" || s == "true") {
        *x = true;
        return true;
    } else if (s == "0" || s == "off" || s == "false") {
        *x = false;
        return true;
    } else {
        return false;
    }
}

static bool parse_aspect_ratio(const std::string& s, float* x)
{
    std::vector<std::string> tokens = str::tokens(s, ":");
    float xn, xd;
    if (tokens.size() == 2 && str::to(tokens[0], &xn) && str::to(tokens[1], &xd)) {
        *x = std::min(std::max(xn / xd, 1.0f), 2.39f);
        return true;
    } else if (tokens.size() == 1 && str::to(s, x)) {
        *x = std::min(std::max(*x, 1.0f), 2.39f);
        return true;
    } else {
        return false;
    }
}

static bool parse_open_input_data(const std::vector<std::string>& tokens, open_input_data* oid)
{
    for (size_t i = 1; i < tokens.size(); i++) {
        if (tokens[i].length() > 2 && tokens[i].substr(0, 2) == "--") {
            std::vector<std::string> subtokens = str::tokens(tokens[i].substr(2), "=");
            if (subtokens.size() == 2 && subtokens[0] == "device-type") {
                if (subtokens[1] == "none") {
                    oid->dev_request.device = device_request::no_device;
                    continue;
                } else if (subtokens[1] == "default") {
                    oid->dev_request.device = device_request::sys_default;
                    continue;
                } else if (subtokens[1] == "firewire") {
                    oid->dev_request.device = device_request::firewire;
                    continue;
                } else if (subtokens[1] == "x11") {
                    oid->dev_request.device = device_request::x11;
                    continue;
                }
            } else if (subtokens.size() == 2 && subtokens[0] == "device-frame-size") {
                std::vector<std::string> wh = str::tokens(subtokens[1], "x");
                if (wh.size() == 2
                        && str::to(wh[0], &oid->dev_request.width) && oid->dev_request.width >= 0
                        && str::to(wh[1], &oid->dev_request.height) && oid->dev_request.height >= 0)
                    continue;
            } else if (subtokens.size() == 2 && subtokens[0] == "device-frame-rate") {
                std::vector<std::string> nd = str::tokens(subtokens[1], "/");
                if (nd.size() == 2
                        && str::to(nd[0], &oid->dev_request.frame_rate_num) && oid->dev_request.frame_rate_num >= 0
                        && str::to(nd[1], &oid->dev_request.frame_rate_den) && oid->dev_request.frame_rate_den >= 0)
                    continue;
            } else if (subtokens.size() == 2 && subtokens[0] == "device-format") {
                if (subtokens[1] == "default") {
                    oid->dev_request.request_mjpeg = false;
                    continue;
                } else if (subtokens[1] == "mjpeg") {
                    oid->dev_request.request_mjpeg = true;
                    continue;
                }
            }
            return false;
        } else {
            // URLs use percent-encoding; see http://en.wikipedia.org/wiki/Percent_encoding
            std::string url;
            for (size_t j = 0; j < tokens[i].size(); j++) {
                if (tokens[i][j] == '%') {
                    if (j + 2 < tokens[i].size()) {
                        char h0 = tokens[i][j + 1];
                        char h1 = tokens[i][j + 2];
                        int k = 0;
                        if (h0 >= '0' && h0 <= '9') {
                            k = (h0 - '0') << 4;
                        } else if (h0 >= 'a' && h0 <= 'f') {
                            k = (h0 - 'a' + 10) << 4;
                        } else if (h0 >= 'A' && h0 <= 'F') {
                            k = (h0 - 'A' + 10) << 4;
                        } else {
                            return false;
                        }
                        if (h1 >= '0' && h1 <= '9') {
                            k += (h1 - '0');
                        } else if (h1 >= 'a' && h1 <= 'f') {
                            k += (h1 - 'a' + 10);
                        } else if (h1 >= 'A' && h1 <= 'F') {
                            k += (h1 - 'A' + 10);
                        } else {
                            return false;
                        }
                        if (k <= 127 && !iscntrl(static_cast<unsigned char>(k))) {
                            url.push_back(k);
                            j += 2;
                        } else {
                            return false;
                        }
                    } else {
                        return false;
                    }
                } else {
                    url.push_back(tokens[i][j]);
                }
            }
            oid->urls.push_back(url);
        }
    }
    return true;
}

bool dispatch::parse_command(const std::string& s, command* c)
{
    std::vector<std::string> tokens = str::tokens(s, " \t\r");
    bool ok = true;
    union {
        bool b;
        int i;
        float f;
        uint64_t c;
    } p;
    float p_crosstalk[3];
    parameters::stereo_layout_t p_stereo_layout;
    parameters::stereo_mode_t p_stereo_mode;
    open_input_data p_oid;

    /* Please keep this in the same order as commands are declared in dispatch.h */

    if (tokens.empty() || tokens[0][0] == '#') {
        // a comment
        *c = command(command::noop);
    } else if (tokens.size() == 1 && tokens[0] == "quit") {
        *c = command(command::quit);
    } else if (tokens.size() > 1 && tokens[0] == "open"
            && parse_open_input_data(tokens, &p_oid)) {
        std::ostringstream v;
        s11n::save(v, p_oid);
        *c = command(command::open, v.str());
    } else if (tokens.size() == 1 && tokens[0] == "close") {
        *c = command(command::close);
    } else if (tokens.size() == 1 && tokens[0] == "toggle-play") {
        *c = command(command::toggle_play);
    } else if (tokens.size() == 1 && tokens[0] == "play") {     // extra command, mainly for LIRC
        if (!dispatch::playing())
            *c = command(command::toggle_play);
        else if (dispatch::pausing())
            *c = command(command::toggle_pause);
        else
            *c = command(command::noop);
    } else if (tokens.size() == 1 && tokens[0] == "stop") {     // extra command, mainly for LIRC
        if (dispatch::playing())
            *c = command(command::toggle_play);
        else
            *c = command(command::noop);
    } else if (tokens.size() == 1 && tokens[0] == "toggle-pause") {
        *c = command(command::toggle_pause);
    } else if (tokens.size() == 1 && tokens[0] == "pause") {    // extra command, mainly for LIRC
        if (dispatch::playing() && !dispatch::pausing())
            *c = command(command::toggle_pause);
        else
            *c = command(command::noop);
    } else if (tokens.size() == 1 && tokens[0] == "step") {
        *c = command(command::step);
    } else if (tokens.size() == 2 && tokens[0] == "seek"
            && str::to(tokens[1], &p.f)) {
        *c = command(command::seek, p.f);
    } else if (tokens.size() == 2 && tokens[0] == "set-pos"
            && str::to(tokens[1], &p.f) && p.f >= 0.0f && p.f <= 1.0f) {
        *c = command(command::set_pos, p.f);
    } else if (tokens.size() == 2 && tokens[0] == "set-audio-device"
            && str::to(tokens[1], &p.i)) {
        *c = command(command::set_audio_device, p.i);
    } else if (tokens.size() == 2 && tokens[0] == "set-quality"
            && str::to(tokens[1], &p.i)) {
        *c = command(command::set_quality, p.i);
    } else if (tokens.size() == 2 && tokens[0] == "set-stereo-mode"
            && parameters::parse_stereo_mode(tokens[1], &p_stereo_mode)) {
        *c = command(command::set_stereo_mode, static_cast<int>(p_stereo_mode));
    } else if (tokens.size() == 2 && tokens[0] == "set-stereo-mode-swap"
            && parse_bool(tokens[1], &p.b)) {
        *c = command(command::set_stereo_mode_swap, p.b);
    } else if (tokens.size() == 1 && tokens[0] == "toggle-stereo-mode-swap") {
        *c = command(command::toggle_stereo_mode_swap);
    } else if (tokens.size() == 4 && tokens[0] == "set-crosstalk"
            && str::to(tokens[1], p_crosstalk + 0) && p_crosstalk[0] >= 0.0f && p_crosstalk[0] <= 1.0f
            && str::to(tokens[2], p_crosstalk + 1) && p_crosstalk[1] >= 0.0f && p_crosstalk[1] <= 1.0f
            && str::to(tokens[3], p_crosstalk + 2) && p_crosstalk[2] >= 0.0f && p_crosstalk[2] <= 1.0f) {
        std::ostringstream v;
        s11n::save(v, p_crosstalk[0]);
        s11n::save(v, p_crosstalk[1]);
        s11n::save(v, p_crosstalk[2]);
        *c = command(command::set_crosstalk, v.str());
    } else if (tokens.size() == 2 && tokens[0] == "set-fullscreen-screens"
            && str::to(tokens[1], &p.i)) {
        *c = command(command::set_fullscreen_screens, p.i);
    } else if (tokens.size() == 2 && tokens[0] == "set-fullscreen-flip-left"
            && parse_bool(tokens[1], &p.b)) {
        *c = command(command::set_fullscreen_flip_left, p.b);
    } else if (tokens.size() == 2 && tokens[0] == "set-fullscreen-flop-left"
            && parse_bool(tokens[1], &p.b)) {
        *c = command(command::set_fullscreen_flop_left, p.b);
    } else if (tokens.size() == 2 && tokens[0] == "set-fullscreen-flip-right"
            && parse_bool(tokens[1], &p.b)) {
        *c = command(command::set_fullscreen_flip_right, p.b);
    } else if (tokens.size() == 2 && tokens[0] == "set-fullscreen-flop-right"
            && parse_bool(tokens[1], &p.b)) {
        *c = command(command::set_fullscreen_flop_right, p.b);
    } else if (tokens.size() == 2 && tokens[0] == "set-fullscreen-inhibit-screensaver"
            && parse_bool(tokens[1], &p.b)) {
        *c = command(command::set_fullscreen_inhibit_screensaver, p.b);
    } else if (tokens.size() == 2 && tokens[0] == "set-fullscreen-3dr-sync"
            && parse_bool(tokens[1], &p.b)) {
        *c = command(command::set_fullscreen_3d_ready_sync, p.b);
    } else if (tokens.size() == 2 && tokens[0] == "set-contrast"
            && str::to(tokens[1], &p.f)) {
        *c = command(command::set_contrast, p.f);
    } else if (tokens.size() == 2 && tokens[0] == "adjust-contrast"
            && str::to(tokens[1], &p.f)) {
        *c = command(command::adjust_contrast, p.f);
    } else if (tokens.size() == 2 && tokens[0] == "set-brightness"
            && str::to(tokens[1], &p.f)) {
        *c = command(command::set_brightness, p.f);
    } else if (tokens.size() == 2 && tokens[0] == "adjust-brightness"
            && str::to(tokens[1], &p.f)) {
        *c = command(command::adjust_brightness, p.f);
    } else if (tokens.size() == 2 && tokens[0] == "set-hue"
            && str::to(tokens[1], &p.f)) {
        *c = command(command::set_hue, p.f);
    } else if (tokens.size() == 2 && tokens[0] == "adjust-hue"
            && str::to(tokens[1], &p.f)) {
        *c = command(command::adjust_hue, p.f);
    } else if (tokens.size() == 2 && tokens[0] == "set-saturation"
            && str::to(tokens[1], &p.f)) {
        *c = command(command::set_saturation, p.f);
    } else if (tokens.size() == 2 && tokens[0] == "adjust-saturation"
            && str::to(tokens[1], &p.f)) {
        *c = command(command::adjust_saturation, p.f);
    } else if (tokens.size() == 2 && tokens[0] == "set-zoom"
            && str::to(tokens[1], &p.f)) {
        *c = command(command::set_zoom, p.f);
    } else if (tokens.size() == 2 && tokens[0] == "adjust-zoom"
            && str::to(tokens[1], &p.f)) {
        *c = command(command::adjust_zoom, p.f);
    } else if (tokens.size() == 2 && tokens[0] == "set-loop-mode"
            && (tokens[1] == "off" || tokens[1] == "current")) {
        parameters::loop_mode_t l = (tokens[1] == "off"
                ? parameters::no_loop : parameters::loop_current);
        *c = command(command::set_loop_mode, static_cast<int>(l));
    } else if (tokens.size() == 2 && tokens[0] == "set-audio-delay"
            && str::to(tokens[1], &p.i)) {
        *c = command(command::set_audio_delay, static_cast<int64_t>(p.i * 1000));
    } else if ((tokens.size() == 1 || tokens.size() == 2) && tokens[0] == "set-subtitle-encoding") {
        std::ostringstream v;
        s11n::save(v, tokens.size() > 1 ? tokens[1] : std::string(""));
        *c = command(command::set_subtitle_encoding, v.str());
    } else if (tokens.size() >= 1 && tokens[0] == "set-subtitle-font") {
        // A font name can contain spaces and therefore span multiple tokens.
        std::ostringstream v;
        s11n::save(v, tokens.size() > 1 ? str::trim(str::trim(s).substr(17)) : std::string(""));
        *c = command(command::set_subtitle_font, v.str());
    } else if (tokens.size() == 2 && tokens[0] == "set-subtitle-size"
            && str::to(tokens[1], &p.i)) {
        *c = command(command::set_subtitle_size, p.i);
    } else if (tokens.size() == 2 && tokens[0] == "set-subtitle-color"
            && str::to(tokens[1], &p.c)) {
        *c = command(command::set_subtitle_color, p.c);
    } else if (tokens.size() == 2 && tokens[0] == "set-subtitle-shadow"
            && str::to(tokens[1], &p.i)) {
        *c = command(command::set_subtitle_shadow, p.i);
    } else if (tokens.size() == 2 && tokens[0] == "set-video-stream"
            && str::to(tokens[1], &p.i) && p.i >= 0) {
        *c = command(command::set_video_stream, p.i);
    } else if (tokens.size() == 1 && tokens[0] == "cycle-video-stream") {
        *c = command(command::cycle_video_stream);
    } else if (tokens.size() == 2 && tokens[0] == "set-audio-stream"
            && str::to(tokens[1], &p.i) && p.i >= 0) {
        *c = command(command::set_audio_stream, p.i);
    } else if (tokens.size() == 1 && tokens[0] == "cycle-audio-stream") {
        *c = command(command::cycle_audio_stream);
    } else if (tokens.size() == 2 && tokens[0] == "set-subtitle-stream"
            && str::to(tokens[1], &p.i) && p.i >= -1) {
        *c = command(command::set_subtitle_stream, p.i);
    } else if (tokens.size() == 1 && tokens[0] == "cycle-subtitle-stream") {
        *c = command(command::cycle_subtitle_stream);
    } else if (tokens.size() == 2 && tokens[0] == "set-stereo-layout"
            && parameters::parse_stereo_layout(tokens[1], &p_stereo_layout)) {
        *c = command(command::set_stereo_layout, static_cast<int>(p_stereo_layout));
    } else if (tokens.size() == 2 && tokens[0] == "set-stereo-layout-swap"
            && parse_bool(tokens[1], &p.b)) {
        *c = command(command::set_stereo_layout_swap, p.b);
    } else if (tokens.size() == 2 && tokens[0] == "set-crop-aspect-ratio"
            && parse_aspect_ratio(tokens[1], &p.f)) {
        *c = command(command::set_crop_aspect_ratio, p.f);
    } else if (tokens.size() == 2 && tokens[0] == "set-source-aspect-ratio"
            && parse_aspect_ratio(tokens[1], &p.f)) {
        *c = command(command::set_source_aspect_ratio, p.f);
    } else if (tokens.size() == 2 && tokens[0] == "set-parallax"
            && str::to(tokens[1], &p.f)) {
        *c = command(command::set_parallax, p.f);
    } else if (tokens.size() == 2 && tokens[0] == "adjust-parallax"
            && str::to(tokens[1], &p.f)) {
        *c = command(command::adjust_parallax, p.f);
    } else if (tokens.size() == 2 && tokens[0] == "set-ghostbust"
            && str::to(tokens[1], &p.f)) {
        *c = command(command::set_ghostbust, p.f);
    } else if (tokens.size() == 2 && tokens[0] == "adjust-ghostbust"
            && str::to(tokens[1], &p.f)) {
        *c = command(command::adjust_ghostbust, p.f);
    } else if (tokens.size() == 2 && tokens[0] == "set-subtitle-parallax"
            && str::to(tokens[1], &p.f)) {
        *c = command(command::set_subtitle_parallax, p.f);
    } else if (tokens.size() == 2 && tokens[0] == "adjust-subtitle-parallax"
            && str::to(tokens[1], &p.f)) {
        *c = command(command::adjust_subtitle_parallax, p.f);
    } else if (tokens.size() == 1 && tokens[0] == "toggle-fullscreen") {
        *c = command(command::toggle_fullscreen);
    } else if (tokens.size() == 1 && tokens[0] == "center") {
        *c = command(command::center);
    } else if (tokens.size() == 2 && tokens[0] == "set-audio-volume"
            && str::to(tokens[1], &p.f)) {
        *c = command(command::set_audio_volume, p.f);
    } else if (tokens.size() == 2 && tokens[0] == "adjust-audio-volume"
            && str::to(tokens[1], &p.f)) {
        *c = command(command::adjust_audio_volume, p.f);
    } else if (tokens.size() == 1 && tokens[0] == "toggle-audio-mute") {
        *c = command(command::toggle_audio_mute);
    } else {
        ok = false;
    }
    return ok;
}
