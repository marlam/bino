/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010, 2011, 2012
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
        if (input_data.urls.size() == 0)
            throw exc(_("No video to play."));
        std::ostringstream v;
        s11n::save(v, input_data);
        controller::send_cmd(command::open, v.str());
        controller::send_cmd(command::toggle_play);
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
        bool more = global_dispatch->_player->run_step();
        if (!more) {
            global_dispatch->_playing = false;
            global_dispatch->_pausing = false;
            global_dispatch->notify_all(notification::play);            
            if (!global_dispatch->_gui_mode) {
                global_dispatch->notify_all(notification::quit);
            } else {
                global_dispatch->force_stop();
            }
        }
    }
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

class player* dispatch::get_player()
{
    return _player;
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

void dispatch::force_stop()
{
    if (_player) {
        _player->close();
        delete _player;
        _player = NULL;
    }
    if (_media_input) {
        _media_input->close();
        _media_input->open(_input_data.urls, _input_data.dev_request);
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
    // Play state
    case command::open:
        force_stop();
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
        break;
    case command::close:
        force_stop();
        notify_all(notification::close);
        break;
    case command::toggle_play:
        if (playing()) {
            assert(_player);
            _player->quit_request();
            /* notify when request is fulfilled */
        } else if (_media_input) {
            // Initialize media input
            if (_input_data.params.stereo_layout_is_set() || _input_data.params.stereo_layout_swap_is_set()) {
                if (!_media_input->stereo_layout_is_supported(_input_data.params.stereo_layout(), _input_data.params.stereo_layout_swap())) {
                    throw exc(_("Cannot set requested stereo layout: incompatible media."));
                }
                _media_input->set_stereo_layout(_input_data.params.stereo_layout(), _input_data.params.stereo_layout_swap());
            }
            if (_media_input->video_streams() < _input_data.params.video_stream() + 1) {
                throw exc(str::asprintf(_("Video stream %d not found."), _input_data.params.video_stream() + 1));
            }
            _media_input->select_video_stream(_input_data.params.video_stream());
            if (_media_input->audio_streams() > 0 && _media_input->audio_streams() < _input_data.params.audio_stream() + 1) {
                throw exc(str::asprintf(_("Audio stream %d not found."), _input_data.params.audio_stream() + 1));
            }
            if (_media_input->audio_streams() > 0) {
                _media_input->select_audio_stream(_input_data.params.audio_stream());
            }
            if (_media_input->subtitle_streams() > 0 && _media_input->subtitle_streams() < _input_data.params.subtitle_stream() + 1) {
                throw exc(str::asprintf(_("Subtitle stream %d not found."), _input_data.params.subtitle_stream() + 1));
            }
            if (_media_input->subtitle_streams() > 0 && _input_data.params.subtitle_stream() >= 0) {
                _media_input->select_subtitle_stream(_input_data.params.subtitle_stream());
            }
            // Initialize parameters
            _parameters.unset_video_parameters();
            if (_input_data.params.video_stream_is_set())
                _parameters.set_video_stream(_input_data.params.video_stream());
            if (_input_data.params.audio_stream_is_set())
                _parameters.set_audio_stream(_input_data.params.video_stream());
            if (_input_data.params.subtitle_stream_is_set())
                _parameters.set_subtitle_stream(_input_data.params.video_stream());
            if (_input_data.params.stereo_layout_is_set())
                _parameters.set_stereo_layout(_input_data.params.stereo_layout());
            if (_input_data.params.stereo_layout_swap_is_set())
                _parameters.set_stereo_layout_swap(_input_data.params.stereo_layout_swap());
            if (!_parameters.stereo_layout_is_set() && !_parameters.stereo_layout_swap_is_set()) {
                _parameters.set_stereo_layout(_media_input->video_frame_template().stereo_layout);
                _parameters.set_stereo_layout_swap(_media_input->video_frame_template().stereo_layout_swap);
            }
            if (_input_data.params.crop_aspect_ratio_is_set())
                _parameters.set_crop_aspect_ratio(_input_data.params.crop_aspect_ratio());
            if (_input_data.params.parallax_is_set())
                _parameters.set_parallax(_input_data.params.parallax());
            if (_input_data.params.ghostbust_is_set())
                _parameters.set_ghostbust(_input_data.params.ghostbust());
            if (_input_data.params.subtitle_parallax_is_set())
                _parameters.set_subtitle_parallax(_input_data.params.subtitle_parallax());
            notify_all(notification::video_stream);
            notify_all(notification::audio_stream);
            notify_all(notification::subtitle_stream);
            notify_all(notification::stereo_layout);
            notify_all(notification::stereo_layout_swap);
            notify_all(notification::crop_aspect_ratio);
            notify_all(notification::parallax);
            notify_all(notification::ghostbust);
            notify_all(notification::subtitle_parallax);
            if (!_parameters.stereo_mode_is_set()) {
                if (_media_input->video_frame_template().stereo_layout == parameters::layout_mono)
                    _parameters.set_stereo_mode(parameters::mode_mono_left);
                else if (_video_output && _video_output->supports_stereo())
                    _parameters.set_stereo_mode(parameters::mode_stereo);
                else
                    _parameters.set_stereo_mode(parameters::mode_red_cyan_dubois);
                _parameters.set_stereo_mode_swap(false);
                notify_all(notification::stereo_mode);
                notify_all(notification::stereo_mode_swap);
            }
            notify_all(notification::open);
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
        _parameters.set_subtitle_scale(std::max(s11n::load<float>(p), 0.0f));
        notify_all(notification::subtitle_scale);
        break;
    case command::set_subtitle_color:
        _parameters.set_subtitle_color(s11n::load<uint64_t>(p));
        notify_all(notification::subtitle_color);
        break;
    // Per-Video parameters
    case command::cycle_video_stream:
        if (_player && _media_input->video_streams() > 1
                && parameters().stereo_layout() != parameters::layout_separate) {
            int s = parameters().video_stream() + 1;
            if (s >= _media_input->video_streams())
                s = 0;
            _parameters.set_video_stream(_player->set_video_stream(s));
            notify_all(notification::video_stream);
        }
        break;
    case command::set_video_stream:
        if (_player && _media_input->video_streams() > 1
                && parameters().stereo_layout() != parameters::layout_separate) {
            _parameters.set_video_stream(_player->set_video_stream(s11n::load<int>(p)));
            notify_all(notification::video_stream);
        }
        break;
    case command::cycle_audio_stream:
        if (_player && _media_input->audio_streams() > 1) {
            int s = parameters().audio_stream() + 1;
            if (s >= _media_input->audio_streams())
                s = 0;
            _parameters.set_audio_stream(_player->set_audio_stream(s));
            notify_all(notification::audio_stream);
        }
        break;
    case command::set_audio_stream:
        if (_player && _media_input->audio_streams() > 1) {
            _parameters.set_audio_stream(_player->set_audio_stream(s11n::load<int>(p)));
            notify_all(notification::audio_stream);
        }
        break;
    case command::cycle_subtitle_stream:
        if (_player && _media_input->subtitle_streams() > 0) {
            int s = parameters().subtitle_stream() + 1;
            if (s >= _media_input->subtitle_streams())
                s = -1;
            _parameters.set_subtitle_stream(_player->set_subtitle_stream(s));
            notify_all(notification::subtitle_stream);
        }
        break;
    case command::set_subtitle_stream:
        if (_player && _media_input->subtitle_streams() > 0) {
            _parameters.set_subtitle_stream(_player->set_subtitle_stream(s11n::load<int>(p)));
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
    case command::adjust_parallax:
        _parameters.set_parallax(clamp(_parameters.contrast() + s11n::load<float>(p), -1.0f, +1.0f));
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
        _parameters.set_subtitle_parallax(clamp(_parameters.contrast() + s11n::load<float>(p), -1.0f, +1.0f));
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
