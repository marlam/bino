/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010, 2011, 2012, 2013, 2015, 2018
 * Martin Lambers <marlam@marlam.de>
 * Joe <joe@wpj.cz>
 * D. Matz <bandregent@yahoo.de>
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

#include <limits>
#include <cstring>
#include <cmath>

#include "media_data.h"

#include "base/str.h"
#include "base/msg.h"
#include "base/dbg.h"

#include "base/gettext.h"
#define _(string) gettext(string)

#if HAVE_LIBXNVCTRL
# include <NVCtrl/NVCtrl.h>
#endif // HAVE_LIBXNVCTRL


device_request::device_request() :
    device(no_device),
    width(0),
    height(0),
    frame_rate_num(0),
    frame_rate_den(0),
    request_mjpeg(false)
{
}

void device_request::save(std::ostream &os) const
{
    s11n::save(os, static_cast<int>(device));
    s11n::save(os, width);
    s11n::save(os, height);
    s11n::save(os, frame_rate_num);
    s11n::save(os, frame_rate_den);
    s11n::save(os, request_mjpeg);
}

void device_request::load(std::istream &is)
{
    int x;
    s11n::load(is, x);
    device = static_cast<device_t>(x);
    s11n::load(is, width);
    s11n::load(is, height);
    s11n::load(is, frame_rate_num);
    s11n::load(is, frame_rate_den);
    s11n::load(is, request_mjpeg);
}


parameters::parameters()
{
    // Invariant parameters
    unset_log_level();
    unset_benchmark();
    unset_swap_interval();
    // Per-Session parameters
    unset_audio_device();
    unset_quality();
    unset_stereo_mode();
    unset_stereo_mode_swap();
    unset_crosstalk_r();
    unset_crosstalk_g();
    unset_crosstalk_b();
    unset_fullscreen_screens();
    unset_fullscreen_flip_left();
    unset_fullscreen_flop_left();
    unset_fullscreen_flip_right();
    unset_fullscreen_flop_right();
    unset_fullscreen_inhibit_screensaver();
    unset_fullscreen_3d_ready_sync();
    unset_contrast();
    unset_brightness();
    unset_hue();
    unset_saturation();
    unset_zoom();
    unset_loop_mode();
    unset_audio_delay();
    unset_subtitle_encoding();
    unset_subtitle_font();
    unset_subtitle_size();
    unset_subtitle_scale();
    unset_subtitle_color();
    unset_subtitle_shadow();
#if HAVE_LIBXNVCTRL
    unset_sdi_output_format();
    unset_sdi_output_left_stereo_mode();
    unset_sdi_output_right_stereo_mode();
#endif // HAVE_LIBXNVCTRL
    // Per-Video parameters
    unset_video_stream();
    unset_audio_stream();
    unset_subtitle_stream();
    unset_stereo_layout();
    unset_stereo_layout_swap();
    unset_crop_aspect_ratio();
    unset_source_aspect_ratio();
    unset_parallax();
    unset_ghostbust();
    unset_subtitle_parallax();
    unset_vertical_pixel_shift_left();
    unset_vertical_pixel_shift_right();
    // Volatile parameters
    unset_fullscreen();
    unset_center();
    unset_audio_volume();
    unset_audio_mute();
}

// Invariant parameters
const msg::level_t parameters::_log_level_default = msg::INF;
const bool parameters::_benchmark_default = false;
const int parameters::_swap_interval_default = 1;
// Per-Session parameter defaults
const int parameters::_audio_device_default = -1;
const int parameters::_quality_default = 4;
const parameters::stereo_mode_t parameters::_stereo_mode_default = mode_mono_left;
const bool parameters::_stereo_mode_swap_default = false;
const float parameters::_crosstalk_r_default = 0.0f;
const float parameters::_crosstalk_g_default = 0.0f;
const float parameters::_crosstalk_b_default = 0.0f;
const int parameters::_fullscreen_screens_default = 0;
const bool parameters::_fullscreen_flip_left_default = false;
const bool parameters::_fullscreen_flop_left_default = false;
const bool parameters::_fullscreen_flip_right_default = false;
const bool parameters::_fullscreen_flop_right_default = false;
const bool parameters::_fullscreen_inhibit_screensaver_default = true;
const bool parameters::_fullscreen_3d_ready_sync_default = false;
const float parameters::_contrast_default = 0.0f;
const float parameters::_brightness_default = 0.0f;
const float parameters::_hue_default = 0.0f;
const float parameters::_saturation_default = 0.0f;
const float parameters::_zoom_default = 0.0f;
const parameters::loop_mode_t parameters::_loop_mode_default = no_loop;
const int64_t parameters::_audio_delay_default = 0;
const std::string parameters::_subtitle_encoding_default = "";
const std::string parameters::_subtitle_font_default = "";
const int parameters::_subtitle_size_default = -1;
const float parameters::_subtitle_scale_default = -1.0f;
const uint64_t parameters::_subtitle_color_default = std::numeric_limits<uint64_t>::max();
const int parameters::_subtitle_shadow_default = -1;
#if HAVE_LIBXNVCTRL
const int parameters::_sdi_output_format_default = NV_CTRL_GVIO_VIDEO_FORMAT_1080P_25_00_SMPTE274;
const parameters::stereo_mode_t parameters::_sdi_output_left_stereo_mode_default = mode_mono_left;
const parameters::stereo_mode_t parameters::_sdi_output_right_stereo_mode_default = mode_mono_right;
#endif // HAVE_LIBXNVCTRL
// Per-Video parameter defaults
const int parameters::_video_stream_default = 0;
const int parameters::_audio_stream_default = 0;
const int parameters::_subtitle_stream_default = -1;
const parameters::stereo_layout_t parameters::_stereo_layout_default = layout_mono;
const bool parameters::_stereo_layout_swap_default = false;
const float parameters::_crop_aspect_ratio_default = 0.0f;
const float parameters::_source_aspect_ratio_default = 0.0f;
const float parameters::_parallax_default = 0.0f;
const float parameters::_ghostbust_default = 0.0f;
const float parameters::_subtitle_parallax_default = 0.0f;
const float parameters::_vertical_pixel_shift_left_default = 0.0f;
const float parameters::_vertical_pixel_shift_right_default = 0.0f;
// Volatile parameter defaults
const bool parameters::_fullscreen_default = false;
const bool parameters::_center_default = false;
const float parameters::_audio_volume_default = 1.0f;
const bool parameters::_audio_mute_default = false;

std::string parameters::stereo_layout_to_string(stereo_layout_t stereo_layout, bool stereo_layout_swap)
{
    std::string s;
    switch (stereo_layout) {
    case layout_mono:
        s = "mono";
        break;
    case layout_separate:
        s = stereo_layout_swap ? "separate-right-left" : "separate-left-right";
        break;
    case layout_alternating:
        s = stereo_layout_swap ? "alternating-right-left" : "alternating-left-right";
        break;
    case layout_top_bottom:
        s = stereo_layout_swap ? "bottom-top" : "top-bottom";
        break;
    case layout_top_bottom_half:
        s = stereo_layout_swap ? "bottom-top-half" : "top-bottom-half";
        break;
    case layout_left_right:
        s = stereo_layout_swap ? "right-left" : "left-right";
        break;
    case layout_left_right_half:
        s = stereo_layout_swap ? "right-left-half" : "left-right-half";
        break;
    case layout_even_odd_rows:
        s = stereo_layout_swap ? "odd-even-rows" : "even-odd-rows";
        break;
    }
    return s;
}

void parameters::stereo_layout_from_string(const std::string &s, stereo_layout_t &stereo_layout, bool &stereo_layout_swap)
{
    if (s == "mono") {
        stereo_layout = layout_mono;
        stereo_layout_swap = false;
    } else if (s == "separate-right-left") {
        stereo_layout = layout_separate;
        stereo_layout_swap = true;
    } else if (s == "separate-left-right") {
        stereo_layout = layout_separate;
        stereo_layout_swap = false;
    } else if (s == "alternating-right-left") {
        stereo_layout = layout_alternating;
        stereo_layout_swap = true;
    } else if (s == "alternating-left-right") {
        stereo_layout = layout_alternating;
        stereo_layout_swap = false;
    } else if (s == "bottom-top") {
        stereo_layout = layout_top_bottom;
        stereo_layout_swap = true;
    } else if (s == "top-bottom") {
        stereo_layout = layout_top_bottom;
        stereo_layout_swap = false;
    } else if (s == "bottom-top-half") {
        stereo_layout = layout_top_bottom_half;
        stereo_layout_swap = true;
    } else if (s == "top-bottom-half") {
        stereo_layout = layout_top_bottom_half;
        stereo_layout_swap = false;
    } else if (s == "right-left") {
        stereo_layout = layout_left_right;
        stereo_layout_swap = true;
    } else if (s == "left-right") {
        stereo_layout = layout_left_right;
        stereo_layout_swap = false;
    } else if (s == "right-left-half") {
        stereo_layout = layout_left_right_half;
        stereo_layout_swap = true;
    } else if (s == "left-right-half") {
        stereo_layout = layout_left_right_half;
        stereo_layout_swap = false;
    } else if (s == "odd-even-rows") {
        stereo_layout = layout_even_odd_rows;
        stereo_layout_swap = true;
    } else if (s == "even-odd-rows") {
        stereo_layout = layout_even_odd_rows;
        stereo_layout_swap = false;
    } else {
        // safe fallback
        stereo_layout = layout_mono;
        stereo_layout_swap = false;
    }
}

bool parameters::parse_stereo_layout(const std::string& s, stereo_layout_t* stereo_layout)
{
    bool ok = true;
    if (s == "mono") {
        *stereo_layout = layout_mono;
    } else if (s == "separate-left-right") {
        *stereo_layout = layout_separate;
    } else if (s == "alternating-left-right") {
        *stereo_layout = layout_alternating;
    } else if (s == "top-bottom") {
        *stereo_layout = layout_top_bottom;
    } else if (s == "top-bottom-half") {
        *stereo_layout = layout_top_bottom_half;
    } else if (s == "left-right") {
        *stereo_layout = layout_left_right;
    } else if (s == "left-right-half") {
        *stereo_layout = layout_left_right_half;
    } else if (s == "even-odd-rows") {
        *stereo_layout = layout_even_odd_rows;
    } else {
        ok = false;
    }
    return ok;
}

std::string parameters::stereo_mode_to_string(stereo_mode_t stereo_mode, bool stereo_mode_swap)
{
    std::string s;
    switch (stereo_mode) {
    case mode_stereo:
        s = "stereo";
        break;
    case mode_alternating:
        s = "alternating";
        break;
    case mode_mono_left:
        s = "mono-left";
        break;
    case mode_mono_right:
        s = "mono-right";
        break;
    case mode_top_bottom:
        s = "top-bottom";
        break;
    case mode_top_bottom_half:
        s = "top-bottom-half";
        break;
    case mode_left_right:
        s = "left-right";
        break;
    case mode_left_right_half:
        s = "left-right-half";
        break;
    case mode_even_odd_rows:
        s = "even-odd-rows";
        break;
    case mode_even_odd_columns:
        s = "even-odd-columns";
        break;
    case mode_checkerboard:
        s = "checkerboard";
        break;
    case mode_hdmi_frame_pack:
        s = "hdmi-frame-pack";
        break;
    case mode_red_cyan_monochrome:
        s = "red-cyan-monochrome";
        break;
    case mode_red_cyan_half_color:
        s = "red-cyan-half-color";
        break;
    case mode_red_cyan_full_color:
        s = "red-cyan-full-color";
        break;
    case mode_red_cyan_dubois:
        s = "red-cyan-dubois";
        break;
    case mode_green_magenta_monochrome:
        s = "green-magenta-monochrome";
        break;
    case mode_green_magenta_half_color:
        s = "green-magenta-half-color";
        break;
    case mode_green_magenta_full_color:
        s = "green-magenta-full-color";
        break;
    case mode_green_magenta_dubois:
        s = "green-magenta-dubois";
        break;
    case mode_amber_blue_monochrome:
        s = "amber-blue-monochrome";
        break;
    case mode_amber_blue_half_color:
        s = "amber-blue-half-color";
        break;
    case mode_amber_blue_dubois:
        s = "amber-blue-dubois";
        break;
    case mode_amber_blue_full_color:
        s = "amber-blue-full-color";
        break;
    case mode_red_green_monochrome:
        s = "red-green-monochrome";
        break;
    case mode_red_blue_monochrome:
        s = "red-blue-monochrome";
        break;
    }
    if (stereo_mode_swap) {
        s += "-swap";
    }
    return s;
}

void parameters::stereo_mode_from_string(const std::string &s, stereo_mode_t &stereo_mode, bool &stereo_mode_swap)
{
    size_t x = s.find_last_of("-");
    std::string t;
    if (x != std::string::npos && s.substr(x) == "-swap") {
        t = s.substr(0, x);
        stereo_mode_swap = true;
    } else {
        t = s;
        stereo_mode_swap = false;
    }
    if (!parse_stereo_mode(t, &stereo_mode)) {
        // safe fallback
        stereo_mode = mode_mono_left;
    }
}

bool parameters::parse_stereo_mode(const std::string& s, stereo_mode_t* stereo_mode)
{
    bool ok = true;
    if (s == "stereo") {
        *stereo_mode = mode_stereo;
    } else if (s == "alternating") {
        *stereo_mode = mode_alternating;
    } else if (s == "mono-left") {
        *stereo_mode = mode_mono_left;
    } else if (s == "mono-right") {
        *stereo_mode = mode_mono_right;
    } else if (s == "top-bottom") {
        *stereo_mode = mode_top_bottom;
    } else if (s == "top-bottom-half") {
        *stereo_mode = mode_top_bottom_half;
    } else if (s == "left-right") {
        *stereo_mode = mode_left_right;
    } else if (s == "left-right-half") {
        *stereo_mode = mode_left_right_half;
    } else if (s == "even-odd-rows") {
        *stereo_mode = mode_even_odd_rows;
    } else if (s == "even-odd-columns") {
        *stereo_mode = mode_even_odd_columns;
    } else if (s == "checkerboard") {
        *stereo_mode = mode_checkerboard;
    } else if (s == "hdmi-frame-pack") {
        *stereo_mode = mode_hdmi_frame_pack;
    } else if (s == "red-cyan-monochrome") {
        *stereo_mode = mode_red_cyan_monochrome;
    } else if (s == "red-cyan-half-color") {
        *stereo_mode = mode_red_cyan_half_color;
    } else if (s == "red-cyan-full-color") {
        *stereo_mode = mode_red_cyan_full_color;
    } else if (s == "red-cyan-dubois") {
        *stereo_mode = mode_red_cyan_dubois;
    } else if (s == "green-magenta-monochrome") {
        *stereo_mode = mode_green_magenta_monochrome;
    } else if (s == "green-magenta-half-color") {
        *stereo_mode = mode_green_magenta_half_color;
    } else if (s == "green-magenta-full-color") {
        *stereo_mode = mode_green_magenta_full_color;
    } else if (s == "green-magenta-dubois") {
        *stereo_mode = mode_green_magenta_dubois;
    } else if (s == "amber-blue-monochrome") {
        *stereo_mode = mode_amber_blue_monochrome;
    } else if (s == "amber-blue-half-color") {
        *stereo_mode = mode_amber_blue_half_color;
    } else if (s == "amber-blue-full-color") {
        *stereo_mode = mode_amber_blue_full_color;
    } else if (s == "amber-blue-dubois") {
        *stereo_mode = mode_amber_blue_dubois;
    } else if (s == "red-green-monochrome") {
        *stereo_mode = mode_red_green_monochrome;
    } else if (s == "red-blue-monochrome") {
        *stereo_mode = mode_red_blue_monochrome;
    } else {
        ok = false;
    }
    return ok;
}

std::string parameters::loop_mode_to_string(loop_mode_t loop_mode)
{
    if (loop_mode == loop_current) {
        return "loop-current";
    } else {
        return "no-loop";
    }
}

parameters::loop_mode_t parameters::loop_mode_from_string(const std::string &s)
{
    if (s == "loop-current") {
        return loop_current;
    } else {
        return no_loop;
    }
}

void parameters::save(std::ostream &os) const
{
    // Invariant parameters
    s11n::save(os, static_cast<int>(_log_level));
    s11n::save(os, _log_level_set);
    s11n::save(os, _benchmark);
    s11n::save(os, _benchmark_set);
    s11n::save(os, _swap_interval);
    s11n::save(os, _swap_interval_set);
    // Per-Session parameters
    s11n::save(os, _audio_device);
    s11n::save(os, _audio_device_set);
    s11n::save(os, _quality);
    s11n::save(os, _quality_set);
    s11n::save(os, static_cast<int>(_stereo_mode));
    s11n::save(os, _stereo_mode_set);
    s11n::save(os, _stereo_mode_swap);
    s11n::save(os, _stereo_mode_swap_set);
    s11n::save(os, _crosstalk_r);
    s11n::save(os, _crosstalk_r_set);
    s11n::save(os, _crosstalk_g);
    s11n::save(os, _crosstalk_g_set);
    s11n::save(os, _crosstalk_b);
    s11n::save(os, _crosstalk_b_set);
    s11n::save(os, _fullscreen_screens);
    s11n::save(os, _fullscreen_screens_set);
    s11n::save(os, _fullscreen_flip_left);
    s11n::save(os, _fullscreen_flip_left_set);
    s11n::save(os, _fullscreen_flop_left);
    s11n::save(os, _fullscreen_flop_left_set);
    s11n::save(os, _fullscreen_flip_right);
    s11n::save(os, _fullscreen_flip_right_set);
    s11n::save(os, _fullscreen_flop_right);
    s11n::save(os, _fullscreen_flop_right_set);
    s11n::save(os, _fullscreen_inhibit_screensaver);
    s11n::save(os, _fullscreen_inhibit_screensaver_set);
    s11n::save(os, _fullscreen_3d_ready_sync);
    s11n::save(os, _fullscreen_3d_ready_sync_set);
    s11n::save(os, _contrast);
    s11n::save(os, _contrast_set);
    s11n::save(os, _brightness);
    s11n::save(os, _brightness_set);
    s11n::save(os, _hue);
    s11n::save(os, _hue_set);
    s11n::save(os, _saturation);
    s11n::save(os, _saturation_set);
    s11n::save(os, _zoom);
    s11n::save(os, _zoom_set);
    s11n::save(os, static_cast<int>(_loop_mode));
    s11n::save(os, _loop_mode_set);
    s11n::save(os, _audio_delay);
    s11n::save(os, _audio_delay_set);
    s11n::save(os, _subtitle_encoding);
    s11n::save(os, _subtitle_encoding_set);
    s11n::save(os, _subtitle_font);
    s11n::save(os, _subtitle_font_set);
    s11n::save(os, _subtitle_size);
    s11n::save(os, _subtitle_size_set);
    s11n::save(os, _subtitle_scale);
    s11n::save(os, _subtitle_scale_set);
    s11n::save(os, _subtitle_color);
    s11n::save(os, _subtitle_color_set);
    s11n::save(os, _subtitle_shadow);
    s11n::save(os, _subtitle_shadow_set);
#if HAVE_LIBXNVCTRL
    s11n::save(os, _sdi_output_format);
    s11n::save(os, _sdi_output_format_set);
    s11n::save(os, static_cast<int>(_sdi_output_left_stereo_mode));
    s11n::save(os, _sdi_output_left_stereo_mode_set);
    s11n::save(os, static_cast<int>(_sdi_output_right_stereo_mode));
    s11n::save(os, _sdi_output_right_stereo_mode_set);
#endif // HAVE_LIBXNVCTRL
    // Per-Video parameters
    s11n::save(os, _video_stream);
    s11n::save(os, _video_stream_set);
    s11n::save(os, _audio_stream);
    s11n::save(os, _audio_stream_set);
    s11n::save(os, _subtitle_stream);
    s11n::save(os, _subtitle_stream_set);
    s11n::save(os, static_cast<int>(_stereo_layout));
    s11n::save(os, _stereo_layout_set);
    s11n::save(os, _stereo_layout_swap);
    s11n::save(os, _stereo_layout_swap_set);
    s11n::save(os, _crop_aspect_ratio);
    s11n::save(os, _crop_aspect_ratio_set);
    s11n::save(os, _source_aspect_ratio);
    s11n::save(os, _source_aspect_ratio_set);
    s11n::save(os, _parallax);
    s11n::save(os, _parallax_set);
    s11n::save(os, _ghostbust);
    s11n::save(os, _ghostbust_set);
    s11n::save(os, _subtitle_parallax);
    s11n::save(os, _subtitle_parallax_set);
    s11n::save(os, _vertical_pixel_shift_left);
    s11n::save(os, _vertical_pixel_shift_left_set);
    s11n::save(os, _vertical_pixel_shift_right);
    s11n::save(os, _vertical_pixel_shift_right_set);
    // Volatile parameters
    s11n::save(os, _fullscreen);
    s11n::save(os, _fullscreen_set);
    s11n::save(os, _center);
    s11n::save(os, _center_set);
    s11n::save(os, _audio_volume);
    s11n::save(os, _audio_volume_set);
    s11n::save(os, _audio_mute);
    s11n::save(os, _audio_mute_set);
}

void parameters::load(std::istream &is)
{
    int x;
    // Invariant parameters
    s11n::load(is, x); _log_level = static_cast<msg::level_t>(x);
    s11n::load(is, _log_level_set);
    s11n::load(is, _benchmark);
    s11n::load(is, _benchmark_set);
    s11n::load(is, _swap_interval);
    s11n::load(is, _swap_interval_set);
    // Per-Session parameters
    s11n::load(is, _audio_device);
    s11n::load(is, _audio_device_set);
    s11n::load(is, _quality);
    s11n::load(is, _quality_set);
    s11n::load(is, x); _stereo_mode = static_cast<stereo_mode_t>(x);
    s11n::load(is, _stereo_mode_set);
    s11n::load(is, _stereo_mode_swap);
    s11n::load(is, _stereo_mode_swap_set);
    s11n::load(is, _crosstalk_r);
    s11n::load(is, _crosstalk_r_set);
    s11n::load(is, _crosstalk_g);
    s11n::load(is, _crosstalk_g_set);
    s11n::load(is, _crosstalk_b);
    s11n::load(is, _crosstalk_b_set);
    s11n::load(is, _fullscreen_screens);
    s11n::load(is, _fullscreen_screens_set);
    s11n::load(is, _fullscreen_flip_left);
    s11n::load(is, _fullscreen_flip_left_set);
    s11n::load(is, _fullscreen_flop_left);
    s11n::load(is, _fullscreen_flop_left_set);
    s11n::load(is, _fullscreen_flip_right);
    s11n::load(is, _fullscreen_flip_right_set);
    s11n::load(is, _fullscreen_flop_right);
    s11n::load(is, _fullscreen_flop_right_set);
    s11n::load(is, _fullscreen_inhibit_screensaver);
    s11n::load(is, _fullscreen_inhibit_screensaver_set);
    s11n::load(is, _fullscreen_3d_ready_sync);
    s11n::load(is, _fullscreen_3d_ready_sync_set);
    s11n::load(is, _contrast);
    s11n::load(is, _contrast_set);
    s11n::load(is, _brightness);
    s11n::load(is, _brightness_set);
    s11n::load(is, _hue);
    s11n::load(is, _hue_set);
    s11n::load(is, _saturation);
    s11n::load(is, _saturation_set);
    s11n::load(is, _zoom);
    s11n::load(is, _zoom_set);
    s11n::load(is, x); _loop_mode = static_cast<loop_mode_t>(x);
    s11n::load(is, _loop_mode_set);
    s11n::load(is, _audio_delay);
    s11n::load(is, _audio_delay_set);
    s11n::load(is, _subtitle_encoding);
    s11n::load(is, _subtitle_encoding_set);
    s11n::load(is, _subtitle_font);
    s11n::load(is, _subtitle_font_set);
    s11n::load(is, _subtitle_size);
    s11n::load(is, _subtitle_size_set);
    s11n::load(is, _subtitle_scale);
    s11n::load(is, _subtitle_scale_set);
    s11n::load(is, _subtitle_color);
    s11n::load(is, _subtitle_color_set);
    s11n::load(is, _subtitle_shadow);
    s11n::load(is, _subtitle_shadow_set);
#if HAVE_LIBXNVCTRL
    s11n::load(is, _sdi_output_format);
    s11n::load(is, _sdi_output_format_set);
    s11n::load(is, x); _sdi_output_left_stereo_mode = static_cast<stereo_mode_t>(x);
    s11n::load(is, _sdi_output_left_stereo_mode_set);
    s11n::load(is, x); _sdi_output_right_stereo_mode = static_cast<stereo_mode_t>(x);
    s11n::load(is, _sdi_output_right_stereo_mode_set);
#endif // HAVE_LIBXNVCTRL
    // Per-Video parameters
    s11n::load(is, _video_stream);
    s11n::load(is, _video_stream_set);
    s11n::load(is, _audio_stream);
    s11n::load(is, _audio_stream_set);
    s11n::load(is, _subtitle_stream);
    s11n::load(is, _subtitle_stream_set);
    s11n::load(is, x); _stereo_layout = static_cast<stereo_layout_t>(x);
    s11n::load(is, _stereo_layout_set);
    s11n::load(is, _stereo_layout_swap);
    s11n::load(is, _stereo_layout_swap_set);
    s11n::load(is, _crop_aspect_ratio);
    s11n::load(is, _crop_aspect_ratio_set);
    s11n::load(is, _source_aspect_ratio);
    s11n::load(is, _source_aspect_ratio_set);
    s11n::load(is, _parallax);
    s11n::load(is, _parallax_set);
    s11n::load(is, _ghostbust);
    s11n::load(is, _ghostbust_set);
    s11n::load(is, _subtitle_parallax);
    s11n::load(is, _subtitle_parallax_set);
    s11n::load(is, _vertical_pixel_shift_left);
    s11n::load(is, _vertical_pixel_shift_left_set);
    s11n::load(is, _vertical_pixel_shift_right);
    s11n::load(is, _vertical_pixel_shift_right_set);
    // Volatile parameters
    s11n::load(is, _fullscreen);
    s11n::load(is, _fullscreen_set);
    s11n::load(is, _center);
    s11n::load(is, _center_set);
    s11n::load(is, _audio_volume);
    s11n::load(is, _audio_volume_set);
    s11n::load(is, _audio_mute);
    s11n::load(is, _audio_mute_set);
}

std::string parameters::save_session_parameters() const
{
    std::stringstream oss;
    if (!audio_device_is_default())
        s11n::save(oss, "audio_device", audio_device());
    if (!quality_is_default())
        s11n::save(oss, "quality", quality());
    if (!stereo_mode_is_default() || !stereo_mode_swap_is_default())
        s11n::save(oss, "stereo_mode", stereo_mode_to_string(stereo_mode(), stereo_mode_swap()));
    if (!crosstalk_r_is_default())
        s11n::save(oss, "crosstalk_r", crosstalk_r());
    if (!crosstalk_g_is_default())
        s11n::save(oss, "crosstalk_g", crosstalk_g());
    if (!crosstalk_b_is_default())
        s11n::save(oss, "crosstalk_b", crosstalk_b());
    if (!fullscreen_screens_is_default())
        s11n::save(oss, "fullscreen_screens", fullscreen_screens());
    if (!fullscreen_flip_left_is_default())
        s11n::save(oss, "fullscreen_flip_left", fullscreen_flip_left());
    if (!fullscreen_flop_left_is_default())
        s11n::save(oss, "fullscreen_flop_left", fullscreen_flop_left());
    if (!fullscreen_flip_right_is_default())
        s11n::save(oss, "fullscreen_flip_right", fullscreen_flip_right());
    if (!fullscreen_flop_right_is_default())
        s11n::save(oss, "fullscreen_flop_right", fullscreen_flop_right());
    if (!fullscreen_inhibit_screensaver_is_default())
        s11n::save(oss, "fullscreen_inhibit_screensaver", fullscreen_inhibit_screensaver());
    if (!fullscreen_3d_ready_sync_is_default())
        s11n::save(oss, "fullscreen_3d_ready_sync", fullscreen_3d_ready_sync());
    if (!contrast_is_default())
        s11n::save(oss, "contrast", contrast());
    if (!brightness_is_default())
        s11n::save(oss, "brightness", brightness());
    if (!hue_is_default())
        s11n::save(oss, "hue", hue());
    if (!saturation_is_default())
        s11n::save(oss, "saturation", saturation());
    if (!zoom_is_default())
        s11n::save(oss, "zoom", zoom());
    if (!loop_mode_is_default())
        s11n::save(oss, "loop_mode", loop_mode_to_string(loop_mode()));
    if (!audio_delay_is_default())
        s11n::save(oss, "audio_delay", audio_delay());
    if (!subtitle_encoding_is_default())
        s11n::save(oss, "subtitle_encoding", _subtitle_encoding);
    if (!subtitle_font_is_default())
        s11n::save(oss, "subtitle_font", _subtitle_font);
    if (!subtitle_size_is_default())
        s11n::save(oss, "subtitle_size", _subtitle_size);
    if (!subtitle_scale_is_default())
        s11n::save(oss, "subtitle_scale", _subtitle_scale);
    if (!subtitle_color_is_default())
        s11n::save(oss, "subtitle_color", _subtitle_color);
    if (!subtitle_shadow_is_default())
        s11n::save(oss, "subtitle_shadow", _subtitle_shadow);
#if HAVE_LIBXNVCTRL
    if (!sdi_output_format_is_default())
        s11n::save(oss, "sdi_output_format", sdi_output_format());
    if (!sdi_output_left_stereo_mode_is_default())
        s11n::save(oss, "sdi_output_left_stereo_mode", stereo_mode_to_string(sdi_output_left_stereo_mode(), false));
    if (!sdi_output_right_stereo_mode_is_default())
        s11n::save(oss, "sdi_output_right_stereo_mode", stereo_mode_to_string(sdi_output_right_stereo_mode(), false));
#endif // HAVE_LIBXNVCTRL
    return oss.str();
}

void parameters::load_session_parameters(const std::string &s)
{
    std::istringstream iss(s);
    std::string name, value;
    while (iss.good()) {
        s11n::load(iss, name, value);
        if (name == "audio_device") {
            s11n::load(value, _audio_device);
            _audio_device_set = true;
        } else if (name == "quality") {
            s11n::load(value, _quality);
            _quality_set = true;
        } else if (name == "stereo_mode") {
            std::string s;
            s11n::load(value, s);
            stereo_mode_from_string(s, _stereo_mode, _stereo_mode_swap);
            _stereo_mode_set = true;
            _stereo_mode_swap_set = true;
        } else if (name == "crosstalk_r") {
            s11n::load(value, _crosstalk_r);
            _crosstalk_r_set = true;
        } else if (name == "crosstalk_g") {
            s11n::load(value, _crosstalk_g);
            _crosstalk_g_set = true;
        } else if (name == "crosstalk_b") {
            s11n::load(value, _crosstalk_b);
            _crosstalk_b_set = true;
        } else if (name == "fullscreen_screens") {
            s11n::load(value, _fullscreen_screens);
            _fullscreen_screens_set = true;
        } else if (name == "fullscreen_flip_left") {
            s11n::load(value, _fullscreen_flip_left);
            _fullscreen_flip_left_set = true;
        } else if (name == "fullscreen_flop_left") {
            s11n::load(value, _fullscreen_flop_left);
            _fullscreen_flop_left_set = true;
        } else if (name == "fullscreen_flip_right") {
            s11n::load(value, _fullscreen_flip_right);
            _fullscreen_flip_right_set = true;
        } else if (name == "fullscreen_flop_right") {
            s11n::load(value, _fullscreen_flop_right);
            _fullscreen_flop_right_set = true;
        } else if (name == "fullscreen_inhibit_screensaver") {
            s11n::load(value, _fullscreen_inhibit_screensaver);
            _fullscreen_inhibit_screensaver_set = true;
        } else if (name == "fullscreen_3d_ready_sync") {
            s11n::load(value, _fullscreen_3d_ready_sync);
            _fullscreen_3d_ready_sync_set = true;
        } else if (name == "contrast") {
            s11n::load(value, _contrast);
            _contrast_set = true;
        } else if (name == "brightness") {
            s11n::load(value, _brightness);
            _brightness_set = true;
        } else if (name == "hue") {
            s11n::load(value, _hue);
            _hue_set = true;
        } else if (name == "saturation") {
            s11n::load(value, _saturation);
            _saturation_set = true;
        } else if (name == "zoom") {
            s11n::load(value, _zoom);
            _zoom_set = true;
        } else if (name == "loop_mode") {
            std::string s;
            s11n::load(value, s);
            _loop_mode = loop_mode_from_string(s);
            _loop_mode_set = true;
        } else if (name == "audio_delay") {
            s11n::load(value, _audio_delay);
            _audio_delay_set = true;
        } else if (name == "subtitle_encoding") {
            s11n::load(value, _subtitle_encoding);
            _subtitle_encoding_set = true;
        } else if (name == "subtitle_font") {
            s11n::load(value, _subtitle_font);
            _subtitle_font_set = true;
        } else if (name == "subtitle_size") {
            s11n::load(value, _subtitle_size);
            _subtitle_size_set = true;
        } else if (name == "subtitle_scale") {
            s11n::load(value, _subtitle_scale);
            _subtitle_scale_set = true;
        } else if (name == "subtitle_color") {
            s11n::load(value, _subtitle_color);
            _subtitle_color_set = true;
        } else if (name == "subtitle_shadow") {
            s11n::load(value, _subtitle_shadow);
            _subtitle_shadow_set = true;
#if HAVE_LIBXNVCTRL
        } else if (name == "sdi_output_format") {
            s11n::load(value, _sdi_output_format);
            _sdi_output_format_set = true;
        } else if (name == "sdi_output_left_stereo_mode") {
            std::string s;
            bool swap_value;
            s11n::load(value, s);
            stereo_mode_from_string(s, _sdi_output_left_stereo_mode, swap_value);
            _sdi_output_left_stereo_mode_set = true;
        } else if (name == "sdi_output_right_stereo_mode") {
            std::string s;
            bool swap_value;
            s11n::load(value, s);
            stereo_mode_from_string(s, _sdi_output_right_stereo_mode, swap_value);
            _sdi_output_right_stereo_mode_set = true;
#endif // HAVE_LIBXNVCTRL
        }
    }
}

void parameters::unset_video_parameters()
{
    unset_video_stream();
    unset_audio_stream();
    unset_subtitle_stream();
    unset_stereo_layout();
    unset_stereo_layout_swap();
    unset_crop_aspect_ratio();
    unset_source_aspect_ratio();
    unset_parallax();
    unset_ghostbust();
    unset_subtitle_parallax();
    unset_vertical_pixel_shift_left();
    unset_vertical_pixel_shift_right();
}

std::string parameters::save_video_parameters() const
{
    std::stringstream oss;
    if (!video_stream_is_default())
        s11n::save(oss, "video_stream", _video_stream);
    if (!audio_stream_is_default())
        s11n::save(oss, "audio_stream", _audio_stream);
    if (!subtitle_stream_is_default())
        s11n::save(oss, "subtitle_stream", _subtitle_stream);
    if (!stereo_layout_is_default() || !stereo_layout_swap_is_default())
        s11n::save(oss, "stereo_layout", stereo_layout_to_string(stereo_layout(), stereo_layout_swap()));
    if (!crop_aspect_ratio_is_default())
        s11n::save(oss, "crop_aspect_ratio", _crop_aspect_ratio);
    if (!source_aspect_ratio_is_default())
        s11n::save(oss, "source_aspect_ratio", _source_aspect_ratio);
    if (!parallax_is_default())
        s11n::save(oss, "parallax", _parallax);
    if (!ghostbust_is_default())
        s11n::save(oss, "ghostbust", _ghostbust);
    if (!subtitle_parallax_is_default())
        s11n::save(oss, "subtitle_parallax", _subtitle_parallax);
    if (!vertical_pixel_shift_left_is_default())
        s11n::save(oss, "vertical_pixel_shift_left", _vertical_pixel_shift_left);
    if (!vertical_pixel_shift_right_is_default())
        s11n::save(oss, "vertical_pixel_shift_right", _vertical_pixel_shift_right);
    return oss.str();
}

void parameters::load_video_parameters(const std::string &s)
{
    std::istringstream iss(s);
    std::string name, value;
    while (iss.good()) {
        s11n::load(iss, name, value);
        if (name == "video_stream") {
            s11n::load(value, _video_stream);
            _video_stream_set = true;
        } else if (name == "audio_stream") {
            s11n::load(value, _audio_stream);
            _audio_stream_set = true;
        } else if (name == "subtitle_stream") {
            s11n::load(value, _subtitle_stream);
            _subtitle_stream_set = true;
        } else if (name == "stereo_layout") {
            std::string s;
            s11n::load(value, s);
            stereo_layout_from_string(s, _stereo_layout, _stereo_layout_swap);
            _stereo_layout_set = true;
            _stereo_layout_swap_set = true;
        } else if (name == "crop_aspect_ratio") {
            s11n::load(value, _crop_aspect_ratio);
            _crop_aspect_ratio_set = true;
        } else if (name == "source_aspect_ratio") {
            s11n::load(value, _source_aspect_ratio);
            _source_aspect_ratio_set = true;
        } else if (name == "parallax") {
            s11n::load(value, _parallax);
            _parallax_set = true;
        } else if (name == "ghostbust") {
            s11n::load(value, _ghostbust);
            _ghostbust_set = true;
        } else if (name == "subtitle_parallax") {
            s11n::load(value, _subtitle_parallax);
            _subtitle_parallax_set = true;
        } else if (name == "vertical_pixel_shift_left") {
            s11n::load(value, _vertical_pixel_shift_left);
            _vertical_pixel_shift_left_set = true;
        } else if (name == "vertical_pixel_shift_right") {
            s11n::load(value, _vertical_pixel_shift_right);
            _vertical_pixel_shift_right_set = true;
        }
    }
}


video_frame::video_frame() :
    raw_width(-1),
    raw_height(-1),
    raw_aspect_ratio(0.0f),
    width(-1),
    height(-1),
    aspect_ratio(0.0f),
    layout(bgra32),
    color_space(srgb),
    value_range(u8_full),
    chroma_location(center),
    stereo_layout(parameters::layout_mono),
    stereo_layout_swap(false),
    presentation_time(std::numeric_limits<int64_t>::min())
{
    for (int i = 0; i < 2; i++)
    {
        for (int p = 0; p < 3; p++)
        {
            data[i][p] = NULL;
            line_size[i][p] = 0;
        }
    }
}

void video_frame::set_view_dimensions()
{
    width = raw_width;
    height = raw_height;
    aspect_ratio = raw_aspect_ratio;
    if (stereo_layout == parameters::layout_left_right)
    {
        width /= 2;
        aspect_ratio /= 2.0f;
    }
    else if (stereo_layout == parameters::layout_left_right_half)
    {
        width /= 2;
    }
    else if (stereo_layout == parameters::layout_top_bottom)
    {
        height /= 2;
        aspect_ratio *= 2.0f;
    }
    else if (stereo_layout == parameters::layout_top_bottom_half)
    {
        height /= 2;
    }
    else if (stereo_layout == parameters::layout_even_odd_rows)
    {
        height /= 2;
        //aspect_ratio *= 2.0f;
        // The only video files I know of which use row-alternating format (those from stereopia.com)
        // do not want this adjustment of aspect ratio.
    }
}

std::string video_frame::format_name() const
{
    std::string name = str::asprintf("%dx%d-%.3g:1-", raw_width, raw_height, raw_aspect_ratio);
    switch (layout)
    {
    case bgra32:
        name += "bgra32";
        break;
    case yuv444p:
        name += "yuv444p";
        break;
    case yuv422p:
        name += "yuv422p";
        break;
    case yuv420p:
        name += "yuv420p";
        break;
    }
    switch (color_space)
    {
    case srgb:
        name += "-srgb";
        break;
    case yuv601:
        name += "-601";
        break;
    case yuv709:
        name += "-709";
        break;
    }
    if (layout != bgra32)
    {
        switch (value_range)
        {
        case u8_full:
            name += "-jpeg";
            break;
        case u8_mpeg:
            name += "-mpeg";
            break;
        case u10_full:
            name += "-jpeg10";
            break;
        case u10_mpeg:
            name += "-mpeg10";
            break;
        }
    }
    if (layout == yuv422p || layout == yuv420p)
    {
        switch (chroma_location)
        {
        case center:
            name += "-c";
            break;
        case left:
            name += "-l";
            break;
        case topleft:
            name += "-tl";
            break;
        }
    }
    return name;
}

std::string video_frame::format_info() const
{
    /* TRANSLATORS: This is a very short string describing the video size and aspect ratio. */
    return str::asprintf(_("%dx%d, %.3g:1"), raw_width, raw_height, aspect_ratio);
}

static int next_multiple_of_4(int x)
{
    return (x / 4 + (x % 4 == 0 ? 0 : 1)) * 4;
}

void video_frame::copy_plane(int view, int plane, void *buf) const
{
    char *dst = reinterpret_cast<char *>(buf);
    const char *src = NULL;
    size_t src_offset = 0;
    size_t src_row_size = 0;
    size_t dst_row_width = 0;
    size_t dst_row_size = 0;
    size_t lines = 0;
    size_t type_size = (value_range == u8_full || value_range == u8_mpeg) ? 1 : 2;

    switch (layout)
    {
    case bgra32:
        dst_row_width = width * 4;
        dst_row_size = dst_row_width * type_size;
        lines = height;
        break;

    case yuv444p:
        dst_row_width = width;
        dst_row_size = next_multiple_of_4(dst_row_width * type_size);
        lines = height;
        break;

    case yuv422p:
        if (plane == 0)
        {
            dst_row_width = width;
            dst_row_size = next_multiple_of_4(dst_row_width * type_size);
            lines = height;
        }
        else
        {
            dst_row_width = width / 2;
            dst_row_size = next_multiple_of_4(dst_row_width * type_size);
            lines = height;
        }
        break;

    case yuv420p:
        if (plane == 0)
        {
            dst_row_width = width;
            dst_row_size = next_multiple_of_4(dst_row_width * type_size);
            lines = height;
        }
        else
        {
            dst_row_width = width / 2;
            dst_row_size = next_multiple_of_4(dst_row_width * type_size);
            lines = height / 2;
        }
        break;
    }

    if (stereo_layout_swap)
    {
        view = (view == 0 ? 1 : 0);
    }
    switch (stereo_layout)
    {
    case parameters::layout_mono:
        src = static_cast<const char *>(data[0][plane]);
        src_row_size = line_size[0][plane];
        src_offset = 0;
        break;
    case parameters::layout_separate:
    case parameters::layout_alternating:
        src = static_cast<const char *>(data[view][plane]);
        src_row_size = line_size[view][plane];
        src_offset = 0;
        break;
    case parameters::layout_top_bottom:
    case parameters::layout_top_bottom_half:
        src = static_cast<const char *>(data[0][plane]);
        src_row_size = line_size[0][plane];
        src_offset = view * lines * src_row_size;
        break;
    case parameters::layout_left_right:
    case parameters::layout_left_right_half:
        src = static_cast<const char *>(data[0][plane]);
        src_row_size = line_size[0][plane];
        src_offset = view * dst_row_width;
        break;
    case parameters::layout_even_odd_rows:
        src = static_cast<const char *>(data[0][plane]);
        src_row_size = 2 * line_size[0][plane];
        src_offset = view * line_size[0][plane];
        break;
    }

    assert(src);
    if (src_row_size == dst_row_size)
    {
        std::memcpy(dst, src + src_offset, lines * src_row_size);
    }
    else
    {
        size_t dst_offset = 0;
        for (size_t y = 0; y < lines; y++)
        {
            std::memcpy(dst + dst_offset, src + src_offset, dst_row_width * type_size);
            dst_offset += dst_row_size;
            src_offset += src_row_size;
        }
    }
}

audio_blob::audio_blob() :
    language(),
    channels(-1),
    rate(-1),
    sample_format(u8),
    data(NULL),
    size(0),
    presentation_time(std::numeric_limits<int64_t>::min())
{
}

std::string audio_blob::format_info() const
{
    /* TRANSLATORS: This is a very short string describing the audio language, channels, frequency, and bits. */
    return str::asprintf(_("%s, %d ch., %g kHz, %d bit"),
            language.empty() ? _("unknown") : language.c_str(),
            channels, rate / 1e3f, sample_bits());
}

std::string audio_blob::format_name() const
{
    const char *sample_format_name = "";
    switch (sample_format)
    {
    case u8:
        sample_format_name = "u8";
        break;
    case s16:
        sample_format_name = "s16";
        break;
    case f32:
        sample_format_name = "f32";
        break;
    case d64:
        sample_format_name = "d64";
        break;
    }
    return str::asprintf("%s-%d-%d-%s",
            language.empty() ? _("unknown") : language.c_str(),
            channels, rate, sample_format_name);
}

int audio_blob::sample_bits() const
{
    int bits = 0;
    switch (sample_format)
    {
    case u8:
        bits = 8;
        break;
    case s16:
        bits = 16;
        break;
    case f32:
        bits = 32;
        break;
    case d64:
        bits = 64;
        break;
    }
    return bits;
}

subtitle_box::subtitle_box() :
    language(),
    format(text),
    style(),
    str(),
    images(),
    presentation_start_time(std::numeric_limits<int64_t>::min()),
    presentation_stop_time(std::numeric_limits<int64_t>::min())
{
}

std::string subtitle_box::format_info() const
{
    return (language.empty() ? _("unknown") : language);
}

std::string subtitle_box::format_name() const
{
    return (language.empty() ? _("unknown") : language);
}

void subtitle_box::image_t::save(std::ostream &os) const
{
    s11n::save(os, w);
    s11n::save(os, h);
    s11n::save(os, x);
    s11n::save(os, y);
    s11n::save(os, palette.size());
    if (palette.size() > 0)
    {
        s11n::save(os, static_cast<const void *>(&(palette[0])), palette.size() * sizeof(uint8_t));
    }
    s11n::save(os, data.size());
    if (data.size() > 0)
    {
        s11n::save(os, static_cast<const void *>(&(data[0])), data.size() * sizeof(uint8_t));
    }
    s11n::save(os, linesize);
}

void subtitle_box::image_t::load(std::istream &is)
{
    size_t s;
    s11n::load(is, w);
    s11n::load(is, h);
    s11n::load(is, x);
    s11n::load(is, y);
    s11n::load(is, s);
    palette.resize(s);
    if (palette.size() > 0)
    {
        s11n::load(is, static_cast<void *>(&(palette[0])), palette.size() * sizeof(uint8_t));
    }
    s11n::load(is, s);
    data.resize(s);
    if (data.size() > 0)
    {
        s11n::load(is, static_cast<void *>(&(data[0])), data.size() * sizeof(uint8_t));
    }
    s11n::load(is, linesize);
}

void subtitle_box::save(std::ostream &os) const
{
    s11n::save(os, language);
    s11n::save(os, static_cast<int>(format));
    s11n::save(os, style);
    s11n::save(os, str);
    s11n::save(os, images);
    s11n::save(os, presentation_start_time);
    s11n::save(os, presentation_stop_time);
}

void subtitle_box::load(std::istream &is)
{
    s11n::load(is, language);
    int x;
    s11n::load(is, x);
    format = static_cast<format_t>(x);
    s11n::load(is, style);
    s11n::load(is, str);
    s11n::load(is, images);
    s11n::load(is, presentation_start_time);
    s11n::load(is, presentation_stop_time);
}
