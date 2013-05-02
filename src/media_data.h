/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010, 2011, 2012, 2013
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

#ifndef MEDIA_DATA_H
#define MEDIA_DATA_H

#include <string>
#include <stdint.h>

#include "s11n.h"
#include "msg.h"


class device_request : public serializable
{
public:
    typedef enum
    {
        no_device,      // No device request.
        sys_default,    // Request for system default video device type.
        firewire,       // Request for a firefire video device.
        x11,            // Request for an X11 grabber.
    } device_t;

    device_t device;    // The device type.
    int width;          // Request frames of the given width (0 means default).
    int height;         // Request frames of the given height (0 means default).
    int frame_rate_num; // Request a specific frame rate (0/0 means default).
    int frame_rate_den; // For example 1/25, 1/30, ...
    bool request_mjpeg; // Request MJPEG format from device

    // Constructor
    device_request();

    // Is this a request for a device?
    bool is_device() const
    {
        return device != no_device;
    }

    // Serialization
    void save(std::ostream &os) const;
    void load(std::istream &is);
};

class parameters : public serializable
{
public:

    // Stereo layout: describes how left and right view are stored
    typedef enum {
        layout_mono,           // 1 video source: center view
        layout_separate,       // 2 video sources: left and right view independent
        layout_alternating,    // 2 video sources: left and right view consecutively
        layout_top_bottom,     // 1 video source: left view top, right view bottom, both with full size
        layout_top_bottom_half,// 1 video source: left view top, right view bottom, both with half size
        layout_left_right,     // 1 video source: left view left, right view right, both with full size
        layout_left_right_half,// 1 video source: left view left, right view right, both with half size
        layout_even_odd_rows   // 1 video source: left view even lines, right view odd lines
    } stereo_layout_t;

    // Convert the stereo layout to and from a string representation
    static std::string stereo_layout_to_string(stereo_layout_t stereo_layout, bool stereo_layout_swap);
    static void stereo_layout_from_string(const std::string &s, stereo_layout_t &stereo_layout, bool &stereo_layout_swap);
    static bool parse_stereo_layout(const std::string& s, stereo_layout_t* stereo_layout);

    // Stereo mode: the output mode for left and right view
    typedef enum {
        mode_stereo,                   // OpenGL quad buffered stereo
        mode_alternating,              // Left and right view alternating
        mode_mono_left,                // Left view only
        mode_mono_right,               // Right view only
        mode_top_bottom,               // Left view top, right view bottom
        mode_top_bottom_half,          // Left view top, right view bottom, half height
        mode_left_right,               // Left view left, right view right
        mode_left_right_half,          // Left view left, right view right, half width
        mode_even_odd_rows,            // Left view even rows, right view odd rows
        mode_even_odd_columns,         // Left view even columns, right view odd columns
        mode_checkerboard,             // Checkerboard pattern
        mode_hdmi_frame_pack,          // HDMI Frame packing (top-bottom separated by 1/49 height)
        mode_red_cyan_monochrome,      // Red/cyan anaglyph, monochrome method
        mode_red_cyan_half_color,      // Red/cyan anaglyph, half color method
        mode_red_cyan_full_color,      // Red/cyan anaglyph, full color method
        mode_red_cyan_dubois,          // Red/cyan anaglyph, high quality Dubois method
        mode_green_magenta_monochrome, // Green/magenta anaglyph, monochrome method
        mode_green_magenta_half_color, // Green/magenta anaglyph, half color method
        mode_green_magenta_full_color, // Green/magenta anaglyph, full color method
        mode_green_magenta_dubois,     // Green/magenta anaglyph, high quality Dubois method
        mode_amber_blue_monochrome,    // Amber/blue anaglyph, monochrome method
        mode_amber_blue_half_color,    // Amber/blue anaglyph, half color method
        mode_amber_blue_full_color,    // Amber/blue anaglyph, full color method
        mode_amber_blue_dubois,        // Amber/blue anaglyph, high quality Dubois method
        mode_red_green_monochrome,     // Red/green anaglyph, monochrome method
        mode_red_blue_monochrome,      // Red/blue anaglyph, monochrome method
    } stereo_mode_t;

    // Convert the stereo mode to and from a string representation
    static std::string stereo_mode_to_string(stereo_mode_t stereo_mode, bool stereo_mode_swap);
    static void stereo_mode_from_string(const std::string &s, stereo_mode_t &stereo_mode, bool &stereo_mode_swap);
    static bool parse_stereo_mode(const std::string& s, stereo_mode_t* stereo_mode);

    typedef enum {
        no_loop,                        // Do not loop.
        loop_current,                   // Loop the current media input.
    } loop_mode_t;

    // Convert the loop mode to and from a string representation
    static std::string loop_mode_to_string(loop_mode_t loop_mode);
    static loop_mode_t loop_mode_from_string(const std::string &s);

#define PARAMETER(TYPE, NAME) \
    private: \
    TYPE _ ## NAME; \
    bool _ ## NAME ## _set; \
    static const TYPE _ ## NAME ## _default; \
    public: \
    TYPE NAME() const \
    { \
        return _ ## NAME ## _set ? _ ## NAME : _ ## NAME ## _default; \
    } \
    void set_ ## NAME(TYPE val) \
    { \
        _ ## NAME = val; \
        _ ## NAME ## _set = true; \
    } \
    void unset_ ## NAME() \
    { \
        _ ## NAME ## _set = false; \
    } \
    bool NAME ## _is_set() const \
    { \
        return _ ## NAME ## _set; \
    } \
    bool NAME ## _is_default() const \
    { \
        return (NAME() >= _ ## NAME ## _default && NAME() <= _ ## NAME ## _default); \
    }

    // Invariant parameters
    PARAMETER(msg::level_t, log_level)        // Global log level
    PARAMETER(bool, benchmark)                // Benchmark mode
    PARAMETER(int, swap_interval)             // Swap interval
    // Per-Session parameters
    PARAMETER(int, audio_device)              // Audio output device index, -1 = default
    PARAMETER(int, quality)                   // Rendering quality, 0=fastest .. 4=best
    PARAMETER(stereo_mode_t, stereo_mode)     // Stereo mode
    PARAMETER(bool, stereo_mode_swap)         // Swap left and right view
    PARAMETER(float, crosstalk_r)             // Crosstalk level for red, 0 .. 1
    PARAMETER(float, crosstalk_g)             // Crosstalk level for green, 0 .. 1
    PARAMETER(float, crosstalk_b)             // Crosstalk level for blue, 0 .. 1
    PARAMETER(int, fullscreen_screens)        // Screens to use in fullscreen mode (bit set), 0=primary screen
    PARAMETER(bool, fullscreen_flip_left)     // Flip left view vertically in fullscreen mode
    PARAMETER(bool, fullscreen_flop_left)     // Flop left view horizontally in fullscreen mode
    PARAMETER(bool, fullscreen_flip_right)    // Flip right view vertically in fullscreen mode
    PARAMETER(bool, fullscreen_flop_right)    // Flop right view horizontally in fullscreen mode
    PARAMETER(bool, fullscreen_inhibit_screensaver)     // Inhibit screensaver when in fullscreen mode
    PARAMETER(bool, fullscreen_3d_ready_sync) // Use DLP 3-D Ready Sync in fullscreen mode
    PARAMETER(float, contrast)                // Contrast adjustment, -1 .. +1
    PARAMETER(float, brightness)              // Brightness adjustment, -1 .. +1
    PARAMETER(float, hue)                     // Hue adjustment, -1 .. +1
    PARAMETER(float, saturation)              // Saturation adjustment, -1 .. +1
    PARAMETER(float, zoom)                    // Zoom, 0 = off (show full video width) .. 1 = full (use full screen height)
    PARAMETER(loop_mode_t, loop_mode)         // Current loop behaviour.
    PARAMETER(int64_t, audio_delay)           // Audio delay in microseconds
    PARAMETER(std::string, subtitle_encoding) // Subtitle encoding, empty means keep default
    PARAMETER(std::string, subtitle_font)     // Subtitle font name, empty means keep default
    PARAMETER(int, subtitle_size)             // Subtitle point size, -1 means keep default
    PARAMETER(float, subtitle_scale)          // Scale factor
    PARAMETER(uint64_t, subtitle_color)       // Subtitle color in uint32_t bgra32 format, > UINT32_MAX means keep default
    PARAMETER(int, subtitle_shadow)           // Subtitle shadow, -1 = default, 0 = force off, 1 = force on
#if HAVE_LIBXNVCTRL
    PARAMETER(int, sdi_output_format)         // SDI output format
    PARAMETER(stereo_mode_t, sdi_output_left_stereo_mode)  // SDI output left stereo mode
    PARAMETER(stereo_mode_t, sdi_output_right_stereo_mode) // SDI output right stereo mode
#endif // HAVE_LIBXNVCTRL
    // Per-Video parameters
    PARAMETER(int, video_stream)              // Video stream index
    PARAMETER(int, audio_stream)              // Audio stream index, or -1 if there is no audio stream
    PARAMETER(int, subtitle_stream)           // Subtitle stream index, or -1 to disable subtitles
    PARAMETER(stereo_layout_t, stereo_layout) // Assume this stereo layout
    PARAMETER(bool, stereo_layout_swap)       // Assume this stereo layout
    PARAMETER(float, crop_aspect_ratio)       // Crop the video to this aspect ratio, 0 = don't crop.
    PARAMETER(float, source_aspect_ratio)     // Force video source to this aspect ratio, 0 = don't force.
    PARAMETER(float, parallax)                // Parallax adjustment, -1 .. +1
    PARAMETER(float, ghostbust)               // Amount of crosstalk ghostbusting, 0 .. 1
    PARAMETER(float, subtitle_parallax)       // Subtitle parallax adjustment, -1 .. +1
    // Volatile parameters
    PARAMETER(bool, fullscreen)               // Fullscreen mode
    PARAMETER(bool, center)                   // Should the video be centered?
    PARAMETER(float, audio_volume)            // Audio volume, 0 .. 1
    PARAMETER(bool, audio_mute)               // Audio mute: -1 = unknown, 0 = off, 1 = on

public:
    // Constructor
    parameters();

    // Serialization
    void save(std::ostream &os) const;
    void load(std::istream &is);

    // Serialize per-session parameters
    std::string save_session_parameters() const;
    void load_session_parameters(const std::string &s);

    // Serialize per-video parameters
    void unset_video_parameters();
    std::string save_video_parameters() const;
    void load_video_parameters(const std::string &s);
};

class video_frame
{
public:
    // Data layout
    typedef enum
    {
        bgra32,         // Single plane: BGRABGRABGRA....
        yuv444p,        // Three planes, Y/U/V, all with the same size
        yuv422p,        // Three planes, U and V with half width: one U/V pair for 2x1 Y values
        yuv420p,        // Three planes, U and V with half width and half height: one U/V pair for 2x2 Y values
    } layout_t;

    // Color space
    typedef enum
    {
        srgb,           // SRGB color space
        yuv601,         // YUV according to ITU.BT-601
        yuv709,         // YUV according to ITU.BT-709
    } color_space_t;

    // Value range
    typedef enum
    {
        u8_full,        // 0-255 for all components
        u8_mpeg,        // 16-235 for Y, 16-240 for U and V
        u10_full,       // 0-1023 for all components (stored in 16 bits)
        u10_mpeg,       // 64-940 for Y, 64-960 for U and V (stored in 16 bits)
    } value_range_t;

    // Location of chroma samples (only relevant for chroma subsampling layouts)
    typedef enum
    {
        center,         // U/V at center of the corresponding Y locations
        left,           // U/V vertically at the center, horizontally at the left Y locations
        topleft         // U/V at the corresponding top left Y location
    } chroma_location_t;

    int raw_width;                      // Width of the data in pixels
    int raw_height;                     // Height of the data in pixels
    float raw_aspect_ratio;             // Aspect ratio of the data
    int width;                          // Width of one view in pixels
    int height;                         // Height of one view in pixels
    float aspect_ratio;                 // Aspect ratio of one view when displayed
    layout_t layout;                    // Data layout
    color_space_t color_space;          // Color space
    value_range_t value_range;          // Value range
    chroma_location_t chroma_location;  // Chroma sample location
    parameters::stereo_layout_t stereo_layout; // Stereo layout
    bool stereo_layout_swap;            // Whether the stereo layout needs to swap left and right view
    // The data. Note that a frame does not own the data stored in these pointers,
    // so it does not free them on destruction.
    void *data[2][3];                   // Data pointer for 1-3 planes in 1-2 views. NULL if unused.
    size_t line_size[2][3];             // Line size for 1-3 planes in 1-2 views. 0 if unused.

    int64_t presentation_time;          // Presentation timestamp

    // Constructor
    video_frame();

    // Set width/height/ar from raw width/height/ar according to stereo layout
    void set_view_dimensions();

    // Does this frame contain valid data?
    bool is_valid() const
    {
        return (raw_width > 0 && raw_height > 0);
    }

    // Copy the data of the given view (0=left, 1=right) and the given plane (see layout)
    // to the given destination.
    void copy_plane(int view, int plane, void *dst) const;

    // Return a string describing the format (layout, color space, value range, chroma location)
    std::string format_info() const;    // Human readable information
    std::string format_name() const;    // Short code
};

class audio_blob
{
public:
    // Sample format
    typedef enum
    {
        u8,             // uint8_t
        s16,            // int16_t
        f32,            // float
        d64             // double
    } sample_format_t;

    // Description of the content
    std::string language;               // Language information (empty if unknown)
    int channels;                       // 1 (mono), 2 (stereo), 4 (quad), 6 (5:1), 7 (6:1), or 8 (7:1)
    int rate;                           // Samples per second
    sample_format_t sample_format;      // Sample format

    // The data. Note that an audio blob does not own the data stored in these pointers,
    // so it does not free them on destruction.
    void *data;                         // Pointer to the data
    size_t size;                        // Data size in bytes

    int64_t presentation_time;          // Presentation timestamp

    // Constructor
    audio_blob();

    // Does this blob contain valid data?
    bool is_valid() const
    {
        return (channels > 0 && rate > 0);
    }

    // Return a string describing the format
    std::string format_info() const;    // Human readable information
    std::string format_name() const;    // Short code

    // Return the number of bits the sample format
    int sample_bits() const;
};

class subtitle_box : public serializable
{
public:
    // Format
    typedef enum
    {
        ass,            // Advanced SubStation Alpha (ASS) format
        text,           // UTF-8 text
        image           // Image in BGRA32 format, with box coordinates
    } format_t;

    // Image data
    class image_t : public serializable
    {
    public:
        int w, h;                       // Dimensions
        int x, y;                       // Position w.r.t. the video frame
        std::vector<uint8_t> palette;   // Palette, with R,G,B,A components for each palette entry.
        std::vector<uint8_t> data;      // Bitmap using the palette
        size_t linesize;                // Size of one bitmap line (may differ from width)

        void save(std::ostream &os) const;
        void load(std::istream &is);
    };

    // Description of the content
    std::string language;               // Language information (empty if unknown)

    // Data
    format_t format;                    // Subtitle data format
    std::string style;                  // Style info (only if format is ass)
    std::string str;                    // Event text (only if format ass or text)
    std::vector<image_t> images;        // Images. These need to be alpha-blended.

    // Presentation time information
    int64_t presentation_start_time;    // Presentation timestamp
    int64_t presentation_stop_time;     // End of presentation timestamp

    // Constructor, Destructor
    subtitle_box();

    // Comparison.
    bool operator==(const subtitle_box &box) const
    {
        if (!is_valid() && !box.is_valid()) {
            return true;
        } else if (format == image) {
            return (presentation_start_time == box.presentation_start_time
                    && presentation_stop_time == box.presentation_stop_time);
        } else {
            return style == box.style && str == box.str;
        }
    }
    bool operator!=(const subtitle_box &box) const
    {
        return !operator==(box);
    }

    // Does this box contain valid data?
    bool is_valid() const
    {
        return (((format == ass || format == text) && !str.empty())
                || (format == image && !images.empty()));
    }

    // Does this box stay constant during its complete presentation time?
    // (ASS subtitles may be animated and thus need to be rerendered when the clock changes)
    bool is_constant() const
    {
        return (format != ass);
    }

    // Return a string describing the format
    std::string format_info() const;    // Human readable information
    std::string format_name() const;    // Short code

    // Serialization
    void save(std::ostream &os) const;
    void load(std::istream &is);
};

#endif
