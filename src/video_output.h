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

#ifndef VIDEO_OUTPUT_H
#define VIDEO_OUTPUT_H

#include <stdint.h>

#include "decoder.h"
#include "controller.h"


class video_output_state
{
public:
    float contrast;     // -1 - +1
    float brightness;   // -1 - +1
    float hue;          // -1 - +1
    float saturation;   // -1 - +1
    bool fullscreen;
    bool swap_eyes;

    video_output_state() throw ();
    ~video_output_state();
};

class video_output : public controller
{
public:
    enum mode
    {
        automatic,                      // To be determined by app. This must not be passed to open()!
        stereo,                         // OpenGL quad buffered stereo
        mono_left,                      // Left view only
        mono_right,                     // Right view only
        top_bottom,                     // Left view top, right view bottom
        top_bottom_half,                // Left view top, right view bottom, half height
        left_right,                     // Left view left, right view right
        left_right_half,                // Left view left, right view right, half width
        even_odd_rows,                  // Left view even rows, right view odd rows
        even_odd_columns,               // Left view even columns, right view odd columns
        checkerboard,                   // Checkerboard pattern
        anaglyph_red_cyan_monochrome,   // Red/cyan anaglyph, monochrome method
        anaglyph_red_cyan_full_color,   // Red/cyan anaglyph, full color method
        anaglyph_red_cyan_half_color,   // Red/cyan anaglyph, half color method
        anaglyph_red_cyan_dubois        // Red/cyan anaglyph, high quality Dubois method
    };
    static std::string mode_name(enum mode m);
    static enum mode mode_from_name(const std::string &name, bool *ok = NULL);
    static bool mode_is_2d(enum mode m);

    enum flags
    {
        center = 1,                     // Center window on screen
    };

public:
    video_output(bool receive_notifications = true) throw ();
    ~video_output();

    /* Get capabilities */
    virtual bool supports_stereo() = 0; // Support for OpenGL quad buffered stereo?

    /* Initialize */
    virtual void open(
            enum decoder::video_frame_format preferred_format,
            int src_width, int src_height, float src_aspect_ratio,
            int mode, const video_output_state &state, unsigned int flags,
            int win_width, int win_height) = 0;

    /* Get the video mode */
    virtual enum mode mode() const = 0;

    /* Get the required video frame format. This can differ from the preferred format! */
    virtual enum decoder::video_frame_format frame_format() const = 0;

    /* Get current state */
    virtual const video_output_state &state() const = 0;

    /* Prepare a left/right view pair for display */
    virtual void prepare(
            uint8_t *l_data[3], size_t l_line_size[3],
            uint8_t *r_data[3], size_t r_line_size[3]) = 0;
    /* Display the prepared left/right view pair */
    virtual void activate() = 0;
    /* Process window system events */
    virtual void process_events() = 0;

    /* Cleanup */
    virtual void close() = 0;
};

#endif
