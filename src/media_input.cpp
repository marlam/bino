/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010, 2011, 2012
 * Martin Lambers <marlam@marlam.de>
 * Frédéric Devernay <frederic.devernay@inrialpes.fr>
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

#include "config.h"

#include <limits>

#include "gettext.h"
#define _(string) gettext(string)

#include "dbg.h"
#include "exc.h"
#include "msg.h"
#include "str.h"

#include "media_input.h"


media_input::media_input() :
    _active_video_stream(-1), _active_audio_stream(-1), _active_subtitle_stream(-1),
    _have_active_video_read(false), _have_active_audio_read(false), _have_active_subtitle_read(false),
    _last_audio_data_size(0), _initial_skip(0), _duration(-1)
{
}

media_input::~media_input()
{
}

void media_input::get_video_stream(int stream, int &media_object, int &media_object_video_stream) const
{
    assert(stream < video_streams());

    size_t i = 0;
    while (_media_objects[i].video_streams() < stream + 1)
    {
        stream -= _media_objects[i].video_streams();
        i++;
    }
    media_object = i;
    media_object_video_stream = stream;
}

void media_input::get_audio_stream(int stream, int &media_object, int &media_object_audio_stream) const
{
    assert(stream < audio_streams());

    size_t i = 0;
    while (_media_objects[i].audio_streams() < stream + 1)
    {
        stream -= _media_objects[i].audio_streams();
        i++;
    }
    media_object = i;
    media_object_audio_stream = stream;
}

void media_input::get_subtitle_stream(int stream, int &media_object, int &media_object_subtitle_stream) const
{
    assert(stream < subtitle_streams());

    size_t i = 0;
    while (_media_objects[i].subtitle_streams() < stream + 1)
    {
        stream -= _media_objects[i].subtitle_streams();
        i++;
    }
    media_object = i;
    media_object_subtitle_stream = stream;
}

// Get the basename of an URL (just the file name, without leading paths)
static std::string basename(const std::string &url)
{
    size_t last_slash = url.find_last_of('/');
    size_t last_backslash = url.find_last_of('\\');
    size_t i = std::min(last_slash, last_backslash);
    if (last_slash != std::string::npos && last_backslash != std::string::npos)
    {
        i = std::max(last_slash, last_backslash);
    }
    if (i == std::string::npos)
    {
        return url;
    }
    else
    {
        return url.substr(i + 1);
    }
}

void media_input::open(const std::vector<std::string> &urls, const device_request &dev_request)
{
    assert(urls.size() > 0);

    // Open media objects
    _is_device = dev_request.is_device();
    _media_objects.resize(urls.size());
    for (size_t i = 0; i < urls.size(); i++)
    {
        _media_objects[i].open(urls[i], dev_request);
    }

    // Construct id for this input
    _id = basename(_media_objects[0].url());
    for (size_t i = 1; i < _media_objects.size(); i++)
    {
        _id += '/';
        _id += basename(_media_objects[i].url());
    }

    // Gather metadata
    for (size_t i = 0; i < _media_objects.size(); i++)
    {
        // Note that we may have multiple identical tag names in our metadata
        for (size_t j = 0; j < _media_objects[i].tags(); j++)
        {
            _tag_names.push_back(_media_objects[i].tag_name(j));
            _tag_values.push_back(_media_objects[i].tag_value(j));
        }
    }

    // Gather streams and stream names
    for (size_t i = 0; i < _media_objects.size(); i++)
    {
        for (int j = 0; j < _media_objects[i].video_streams(); j++)
        {
            _video_stream_names.push_back(_media_objects[i].video_frame_template(j).format_info());
        }
    }
    if (_video_stream_names.size() > 1)
    {
        for (size_t i = 0; i < _video_stream_names.size(); i++)
        {
            _video_stream_names[i].insert(0,
                    std::string(1, '#') + str::from(i + 1) + '/'
                    + str::from(_video_stream_names.size()) + ": ");
        }
    }
    for (size_t i = 0; i < _media_objects.size(); i++)
    {
        for (int j = 0; j < _media_objects[i].audio_streams(); j++)
        {
            _audio_stream_names.push_back(_media_objects[i].audio_blob_template(j).format_info());
        }
    }
    if (_audio_stream_names.size() > 1)
    {
        for (size_t i = 0; i < _audio_stream_names.size(); i++)
        {
            _audio_stream_names[i].insert(0,
                    std::string(1, '#') + str::from(i + 1) + '/'
                    + str::from(_audio_stream_names.size()) + ": ");
        }
    }
    for (size_t i = 0; i < _media_objects.size(); i++)
    {
        for (int j = 0; j < _media_objects[i].subtitle_streams(); j++)
        {
            _subtitle_stream_names.push_back(_media_objects[i].subtitle_box_template(j).format_info());
        }
    }
    if (_subtitle_stream_names.size() > 1)
    {
        for (size_t i = 0; i < _subtitle_stream_names.size(); i++)
        {
            _subtitle_stream_names[i].insert(0,
                    std::string(1, '#') + str::from(i + 1) + '/'
                    + str::from(_subtitle_stream_names.size()) + ": ");
        }
    }

    // Set duration information
    _duration = std::numeric_limits<int64_t>::max();
    for (size_t i = 0; i < _media_objects.size(); i++)
    {
        for (int j = 0; j < _media_objects[i].video_streams(); j++)
        {
            int64_t d = _media_objects[i].video_duration(j);
            if (d < _duration)
            {
                _duration = d;
            }
        }
        for (int j = 0; j < _media_objects[i].audio_streams(); j++)
        {
            int64_t d = _media_objects[i].audio_duration(j);
            if (d < _duration)
            {
                _duration = d;
            }
        }
        // Ignore subtitle stream duration; it seems unreliable and is not important anyway.
    }

    // Skip advertisement in 3dtv.at movies. Only works for single media objects.
    try { _initial_skip = str::to<int64_t>(tag_value("StereoscopicSkip")); } catch (...) { }

    // Find stereo layout and set active video stream(s)
    _supports_stereo_layout_separate = false;
    if (video_streams() == 2)
    {
        int o0, o1, v0, v1;
        get_video_stream(0, o0, v0);
        get_video_stream(1, o1, v1);
        video_frame t0 = _media_objects[o0].video_frame_template(v0);
        video_frame t1 = _media_objects[o1].video_frame_template(v1);
        if (t0.width == t1.width
                && t0.height == t1.height
                && (t0.aspect_ratio <= t1.aspect_ratio && t0.aspect_ratio >= t1.aspect_ratio)
                && t0.layout == t1.layout
                && t0.color_space == t1.color_space
                && t0.value_range == t1.value_range
                && t0.chroma_location == t1.chroma_location)
        {
            _supports_stereo_layout_separate = true;
        }
    }
    if (_supports_stereo_layout_separate)
    {
        _active_video_stream = 0;
        int o, s;
        get_video_stream(_active_video_stream, o, s);
        _video_frame = _media_objects[o].video_frame_template(s);
        _video_frame.stereo_layout = parameters::layout_separate;
    }
    else if (video_streams() > 0)
    {
        _active_video_stream = 0;
        int o, s;
        get_video_stream(_active_video_stream, o, s);
        _video_frame = _media_objects[o].video_frame_template(s);
    }
    else
    {
        _active_video_stream = -1;
    }
    if (_active_video_stream >= 0)
    {
        select_video_stream(_active_video_stream);
    }

    // Set active audio stream
    _active_audio_stream = (audio_streams() > 0 ? 0 : -1);
    if (_active_audio_stream >= 0)
    {
        int o, s;
        get_audio_stream(_active_audio_stream, o, s);
        _audio_blob = _media_objects[o].audio_blob_template(s);
        select_audio_stream(_active_audio_stream);
    }

    // Set active subtitle stream
    _active_subtitle_stream = -1;       // no subtitles by default

    // Print summary
    msg::inf(_("Input:"));
    for (int i = 0; i < video_streams(); i++)
    {
        int o, s;
        get_video_stream(i, o, s);
        msg::inf(4, _("Video %s: %s"), video_stream_name(i).c_str(),
                _media_objects[o].video_frame_template(s).format_name().c_str());
    }
    if (video_streams() == 0)
    {
        msg::inf(4, _("No video."));
    }
    for (int i = 0; i < audio_streams(); i++)
    {
        int o, s;
        get_audio_stream(i, o, s);
        msg::inf(4, _("Audio %s: %s"), audio_stream_name(i).c_str(),
                _media_objects[o].audio_blob_template(s).format_name().c_str());
    }
    if (audio_streams() == 0)
    {
        msg::inf(4, _("No audio."));
    }
    for (int i = 0; i < subtitle_streams(); i++)
    {
        int o, s;
        get_subtitle_stream(i, o, s);
        msg::inf(4, _("Subtitle %s: %s"), subtitle_stream_name(i).c_str(),
                _media_objects[o].subtitle_box_template(s).format_name().c_str());
    }
    if (subtitle_streams() == 0)
    {
        msg::inf(4, _("No subtitle."));
    }
    msg::inf(4, _("Duration: %g seconds"), duration() / 1e6f);
    if (video_streams() > 0)
    {
        msg::inf(4, _("Stereo layout: %s"), parameters::stereo_layout_to_string(
                    video_frame_template().stereo_layout, video_frame_template().stereo_layout_swap).c_str());
    }
}

size_t media_input::urls() const
{
    return _media_objects.size();
}

const std::string &media_input::url(size_t i) const
{
    return _media_objects[i].url();
}

const std::string &media_input::id() const
{
    return _id;
}

bool media_input::is_device() const
{
    return _is_device;
}

size_t media_input::tags() const
{
    return _tag_names.size();
}

const std::string &media_input::tag_name(size_t i) const
{
    assert(_tag_names.size() > i);
    return _tag_names[i];
}

const std::string &media_input::tag_value(size_t i) const
{
    assert(_tag_values.size() > i);
    return _tag_values[i];
}

const std::string &media_input::tag_value(const std::string &tag_name) const
{
    static std::string empty;
    for (size_t i = 0; i < _tag_names.size(); i++)
    {
        if (std::string(tag_name) == _tag_names[i])
        {
            return _tag_values[i];
        }
    }
    return empty;
}

const video_frame &media_input::video_frame_template() const
{
    assert(_active_video_stream >= 0);
    return _video_frame;
}

int media_input::video_frame_rate_numerator() const
{
    assert(_active_video_stream >= 0);
    int o, s;
    get_video_stream(_active_video_stream, o, s);
    return _media_objects[o].video_frame_rate_numerator(s);
}

int media_input::video_frame_rate_denominator() const
{
    assert(_active_video_stream >= 0);
    int o, s;
    get_video_stream(_active_video_stream, o, s);
    return _media_objects[o].video_frame_rate_denominator(s);
}

int64_t media_input::video_frame_duration() const
{
    assert(_active_video_stream >= 0);
    return static_cast<int64_t>(video_frame_rate_denominator()) * 1000000 / video_frame_rate_numerator();
}

const audio_blob &media_input::audio_blob_template() const
{
    assert(_active_audio_stream >= 0);
    return _audio_blob;
}

const subtitle_box &media_input::subtitle_box_template() const
{
    assert(_active_subtitle_stream >= 0);
    return _subtitle_box;
}

bool media_input::stereo_layout_is_supported(parameters::stereo_layout_t layout, bool) const
{
    if (video_streams() < 1)
    {
        return false;
    }
    assert(_active_video_stream >= 0);
    assert(_active_video_stream < video_streams());
    int o, s;
    get_video_stream(_active_video_stream, o, s);
    const video_frame &t = _media_objects[o].video_frame_template(s);
    bool supported = true;
    if (((layout == parameters::layout_left_right || layout == parameters::layout_left_right_half) && t.raw_width % 2 != 0)
            || ((layout == parameters::layout_top_bottom || layout == parameters::layout_top_bottom_half) && t.raw_height % 2 != 0)
            || (layout == parameters::layout_even_odd_rows && t.raw_height % 2 != 0)
            || (layout == parameters::layout_separate && !_supports_stereo_layout_separate))
    {
        supported = false;
    }
    return supported;
}

void media_input::set_stereo_layout(parameters::stereo_layout_t layout, bool swap)
{
    assert(stereo_layout_is_supported(layout, swap));
    if (_have_active_video_read)
    {
        (void)finish_video_frame_read();
    }
    if (_have_active_audio_read)
    {
        (void)finish_audio_blob_read();
    }
    if (_have_active_subtitle_read)
    {
        (void)finish_subtitle_box_read();
    }
    int o, s;
    get_video_stream(_active_video_stream, o, s);
    const video_frame &t = _media_objects[o].video_frame_template(s);
    _video_frame = t;
    _video_frame.stereo_layout = layout;
    _video_frame.stereo_layout_swap = swap;
    _video_frame.set_view_dimensions();
    // Reset active stream in case we switched to or from 'separate'.
    select_video_stream(_active_video_stream);
    if (layout == parameters::layout_separate)
    {
        // If we switched the layout to 'separate', then we have to seek to the
        // position of the first video stream, or else the second video stream
        // is out of sync.
        int64_t pos = _media_objects[o].tell();
        if (pos > std::numeric_limits<int64_t>::min())
        {
            seek(pos);
        }
    }
}

void media_input::select_video_stream(int video_stream)
{
    if (_have_active_video_read)
    {
        (void)finish_video_frame_read();
    }
    if (_have_active_audio_read)
    {
        (void)finish_audio_blob_read();
    }
    if (_have_active_subtitle_read)
    {
        (void)finish_subtitle_box_read();
    }
    assert(video_stream >= 0);
    assert(video_stream < video_streams());
    if (_video_frame.stereo_layout == parameters::layout_separate)
    {
        _active_video_stream = 0;
        for (size_t i = 0; i < _media_objects.size(); i++)
        {
            for (int j = 0; j < _media_objects[i].video_streams(); j++)
            {
                _media_objects[i].video_stream_set_active(j, true);
            }
        }
    }
    else
    {
        _active_video_stream = video_stream;
        int o, s;
        get_video_stream(_active_video_stream, o, s);
        for (size_t i = 0; i < _media_objects.size(); i++)
        {
            for (int j = 0; j < _media_objects[i].video_streams(); j++)
            {
                _media_objects[i].video_stream_set_active(j, (i == static_cast<size_t>(o) && j == s));
            }
        }
    }
    // Re-set video frame template
    parameters::stereo_layout_t stereo_layout_bak = _video_frame.stereo_layout;
    bool stereo_layout_swap_bak = _video_frame.stereo_layout_swap;
    int o, s;
    get_video_stream(_active_video_stream, o, s);
    _video_frame = _media_objects[o].video_frame_template(s);
    _video_frame.stereo_layout = stereo_layout_bak;
    _video_frame.stereo_layout_swap = stereo_layout_swap_bak;
    _video_frame.set_view_dimensions();
}

void media_input::select_audio_stream(int audio_stream)
{
    if (_have_active_video_read)
    {
        (void)finish_video_frame_read();
    }
    if (_have_active_audio_read)
    {
        (void)finish_audio_blob_read();
    }
    if (_have_active_subtitle_read)
    {
        (void)finish_subtitle_box_read();
    }
    assert(audio_stream >= 0);
    assert(audio_stream < audio_streams());
    _active_audio_stream = audio_stream;
    int o, s;
    get_audio_stream(_active_audio_stream, o, s);
    for (size_t i = 0; i < _media_objects.size(); i++)
    {
        for (int j = 0; j < _media_objects[i].audio_streams(); j++)
        {
            _media_objects[i].audio_stream_set_active(j, (i == static_cast<size_t>(o) && j == s));
        }
    }
    // Re-set audio blob template
    _audio_blob = _media_objects[o].audio_blob_template(s);
}

void media_input::select_subtitle_stream(int subtitle_stream)
{
    if (_have_active_video_read)
    {
        (void)finish_video_frame_read();
    }
    if (_have_active_audio_read)
    {
        (void)finish_audio_blob_read();
    }
    if (_have_active_subtitle_read)
    {
        (void)finish_subtitle_box_read();
    }
    assert(subtitle_stream >= -1);
    assert(subtitle_stream < subtitle_streams());
    _active_subtitle_stream = subtitle_stream;
    int o = -1, s = -1;
    if (_active_subtitle_stream >= 0)
    {
        get_subtitle_stream(_active_subtitle_stream, o, s);
    }
    for (size_t i = 0; i < _media_objects.size(); i++)
    {
        for (int j = 0; j < _media_objects[i].subtitle_streams(); j++)
        {
            _media_objects[i].subtitle_stream_set_active(j, (i == static_cast<size_t>(o) && j == s));
        }
    }
    // Re-set subtitle box template
    if (_active_subtitle_stream >= 0)
        _subtitle_box = _media_objects[o].subtitle_box_template(s);
    else
        _subtitle_box = subtitle_box();
}

void media_input::start_video_frame_read()
{
    assert(_active_video_stream >= 0);
    if (_have_active_video_read)
    {
        return;
    }
    if (_video_frame.stereo_layout == parameters::layout_separate)
    {
        int o0, s0, o1, s1;
        get_video_stream(0, o0, s0);
        get_video_stream(1, o1, s1);
        _media_objects[o0].start_video_frame_read(s0, 1);
        _media_objects[o1].start_video_frame_read(s1, 1);
    }
    else
    {
        int o, s;
        get_video_stream(_active_video_stream, o, s);
        _media_objects[o].start_video_frame_read(s,
                _video_frame.stereo_layout == parameters::layout_alternating ? 2 : 1);
    }
    _have_active_video_read = true;
}

video_frame media_input::finish_video_frame_read()
{
    assert(_active_video_stream >= 0);
    if (!_have_active_video_read)
    {
        start_video_frame_read();
    }
    video_frame frame;
    if (_video_frame.stereo_layout == parameters::layout_separate)
    {
        int o0, s0, o1, s1;
        get_video_stream(0, o0, s0);
        get_video_stream(1, o1, s1);
        video_frame f0 = _media_objects[o0].finish_video_frame_read(s0);
        video_frame f1 = _media_objects[o1].finish_video_frame_read(s1);
        if (f0.is_valid() && f1.is_valid())
        {
            frame = _video_frame;
            for (int p = 0; p < 3; p++)
            {
                frame.data[0][p] = f0.data[0][p];
                frame.data[1][p] = f1.data[0][p];
                frame.line_size[0][p] = f0.line_size[0][p];
                frame.line_size[1][p] = f1.line_size[0][p];
            }
            frame.presentation_time = f0.presentation_time;
        }
    }
    else
    {
        int o, s;
        get_video_stream(_active_video_stream, o, s);
        video_frame f = _media_objects[o].finish_video_frame_read(s);
        if (f.is_valid())
        {
            frame = _video_frame;
            for (int v = 0; v < 2; v++)
            {
                for (int p = 0; p < 3; p++)
                {
                    frame.data[v][p] = f.data[v][p];
                    frame.line_size[v][p] = f.line_size[v][p];
                }
            }
            frame.presentation_time = f.presentation_time;
        }
    }
    _have_active_video_read = false;
    return frame;
}

void media_input::start_audio_blob_read(size_t size)
{
    assert(_active_audio_stream >= 0);
    if (_have_active_audio_read)
    {
        return;
    }
    int o, s;
    get_audio_stream(_active_audio_stream, o, s);
    _media_objects[o].start_audio_blob_read(s, size);
    _last_audio_data_size = size;
    _have_active_audio_read = true;
}

audio_blob media_input::finish_audio_blob_read()
{
    assert(_active_audio_stream >= 0);
    int o, s;
    get_audio_stream(_active_audio_stream, o, s);
    if (!_have_active_audio_read)
    {
        start_audio_blob_read(_last_audio_data_size);
    }
    _have_active_audio_read = false;
    return _media_objects[o].finish_audio_blob_read(s);
}

void media_input::start_subtitle_box_read()
{
    assert(_active_subtitle_stream >= 0);
    if (_have_active_subtitle_read)
    {
        return;
    }
    int o, s;
    get_subtitle_stream(_active_subtitle_stream, o, s);
    _media_objects[o].start_subtitle_box_read(s);
    _have_active_subtitle_read = true;
}

subtitle_box media_input::finish_subtitle_box_read()
{
    assert(_active_subtitle_stream >= 0);
    int o, s;
    get_subtitle_stream(_active_subtitle_stream, o, s);
    if (!_have_active_subtitle_read)
    {
        start_subtitle_box_read();
    }
    _have_active_subtitle_read = false;
    return _media_objects[o].finish_subtitle_box_read(s);
}

int64_t media_input::tell()
{
    int64_t pos = std::numeric_limits<int64_t>::min();
    int o, s;
    if (_active_audio_stream >= 0)
    {
        get_audio_stream(_active_audio_stream, o, s);
        pos = _media_objects[o].tell();
    }
    else if (_active_video_stream >= 0)
    {
        get_video_stream(_active_video_stream, o, s);
        pos = _media_objects[o].tell();
    }
    return pos;
}

void media_input::seek(int64_t pos)
{
    if (_have_active_video_read)
    {
        (void)finish_video_frame_read();
    }
    if (_have_active_audio_read)
    {
        (void)finish_audio_blob_read();
    }
    if (_have_active_subtitle_read)
    {
        (void)finish_subtitle_box_read();
    }
    for (size_t i = 0; i < _media_objects.size(); i++)
    {
        _media_objects[i].seek(pos);
    }
}

void media_input::close()
{
    try
    {
        if (_have_active_video_read)
        {
            (void)finish_video_frame_read();
        }
        if (_have_active_audio_read)
        {
            (void)finish_audio_blob_read();
        }
        if (_have_active_subtitle_read)
        {
            (void)finish_subtitle_box_read();
        }
        for (size_t i = 0; i < _media_objects.size(); i++)
        {
            _media_objects[i].close();
        }
    }
    catch (...)
    {
    }
    _is_device = false;
    _id = "";
    _media_objects.clear();
    _tag_names.clear();
    _tag_values.clear();
    _video_stream_names.clear();
    _audio_stream_names.clear();
    _subtitle_stream_names.clear();
    _active_video_stream = -1;
    _active_audio_stream = -1;
    _active_subtitle_stream = -1;
    _initial_skip = 0;
    _duration = -1;
    _video_frame = video_frame();
    _audio_blob = audio_blob();
    _subtitle_box = subtitle_box();
}
