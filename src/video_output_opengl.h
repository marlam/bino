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

#ifndef VIDEO_OUTPUT_OPENGL_H
#define VIDEO_OUTPUT_OPENGL_H

#include <vector>
#include <string>

#include <GL/glew.h>

#include "video_output.h"


/* See video_output.h for API documentation */

class video_output_opengl : public video_output
{
private:
    enum video_output::mode _mode;
    int _src_width;
    int _src_height;
    float _src_aspect_ratio;
    enum decoder::video_frame_format _src_preferred_frame_format;
    int _screen_width;
    int _screen_height;
    float _screen_pixel_aspect_ratio;
    int _win_width;
    int _win_height;
    bool _initialized;
    GLuint _prg;
    GLuint _rgb_tex[2][2];
    GLuint _y_tex[2][2];
    GLuint _u_tex[2][2];
    GLuint _v_tex[2][2];
    GLuint _mask_tex;
    int _active_tex_set;
    bool _input_is_mono;
    video_output_state _state;
    GLuint _pbo;
    bool _have_valid_data[2];

    void bind_textures(int unitset, int index);
    void draw_quad(float x, float y, float w, float h);

protected:
    /* In a sub-class that provides an OpenGL context, call the following
     * initialization functions in the order in which they appear here.
     * You must make sure that the OpenGL context provides GL 2.1 + FBOs. */
    void set_mode(enum video_output::mode mode);
    void set_source_info(int width, int height, float aspect_ratio, enum decoder::video_frame_format preferred_frame_format);
    void set_screen_info(int width, int height, float pixel_aspect_ratio);
    void compute_win_size(int width = -1, int height = -1);
    void set_state(const video_output_state &_state);
    void initialize();

    // Swap the texture sets (one active, one for preparing the next video frame)
    void swap_tex_set();
    // Display the current texture
    void display(enum video_output::mode mode, float x, float y, float w, float h);
    void display() { display(_mode, -1.0f, -1.0f, 2.0f, 2.0f); }
    // Call this when the GL window was resized:
    void reshape(int w, int h);

    // Access information
    video_output_state &state()
    {
        return _state;
    }
    int screen_width() const
    {
        return _screen_width;
    }
    int screen_height() const
    {
        return _screen_height;
    }
    int win_width() const
    {
        return _win_width;
    }
    int win_height() const
    {
        return _win_height;
    }

    // Deinitialize.
    void deinitialize();

    // These functions must be implemented in a subclass and must
    // return the position of the video area on the screen.
    virtual int screen_pos_x() = 0;
    virtual int screen_pos_y() = 0;

public:
    video_output_opengl(bool receive_notifications = true) throw ();
    virtual ~video_output_opengl();

    virtual bool supports_stereo() = 0;

    virtual void open(
            enum decoder::video_frame_format preferred_format,
            int src_width, int src_height, float src_aspect_ratio,
            int mode, const video_output_state &state, unsigned int flags,
            int win_width, int win_height) = 0;

    virtual enum mode mode() const
    {
        return _mode;
    }

    virtual enum decoder::video_frame_format frame_format() const;

    virtual const video_output_state &state() const
    {
        return _state;
    }

    virtual void prepare(
            uint8_t *l_data[3], size_t l_line_size[3],
            uint8_t *r_data[3], size_t r_line_size[3]);
    virtual void activate() = 0;
    virtual void process_events() = 0;

    virtual void close() = 0;

    virtual void receive_notification(const notification &note) = 0;
};

std::vector<std::string> glew_versions();

#endif
