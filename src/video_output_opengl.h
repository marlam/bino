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

#include <GL/glew.h>

#include "video_output.h"


class video_output_opengl : public video_output
{
private:
    bool _glut_initialized;
    video_frame_format _preferred_frame_format;
    video_output::mode _mode;
    float _screen_pixel_aspect_ratio;
    int _src_width;
    int _src_height;
    float _src_aspect_ratio;
    int _win_width;
    int _win_height;
    GLuint _prg;
    GLuint _rgb_tex[2][2];
    GLuint _y_tex[2][2];
    GLuint _u_tex[2][2];
    GLuint _v_tex[2][2];
    int _active_tex_set;
    int _window_id;
    bool _input_is_mono;
    struct state _state;
    bool _yuv420p_supported;

    void bind_textures(int unitset, int index);

    void init_glut();
    void display();
    void reshape(int w, int h);
    void keyboard(unsigned char key, int x, int y);
    void special(int key, int x, int y);

public:
    video_output_opengl() throw ();
    ~video_output_opengl();

    virtual bool supports_stereo();

    virtual void open(
            video_frame_format preferred_format,
            int src_width, int src_height, float src_aspect_ratio,
            int mode, const struct state &state, unsigned int flags,
            int win_width, int win_height);

    virtual video_frame_format frame_format() const;

    virtual const struct state &state() const
    {
        return _state;
    }

    virtual void prepare(
            uint8_t *l_data[3], size_t l_line_size[3],
            uint8_t *r_data[3], size_t r_line_size[3]);
    virtual void activate();
    virtual void process_events();

    virtual void close();

    virtual void receive_notification(const notification &note);

friend void global_video_output_opengl_display(void);
friend void global_video_output_opengl_reshape(int w, int h);
friend void global_video_output_opengl_keyboard(unsigned char key, int x, int y);
friend void global_video_output_opengl_special(int key, int x, int y);
};

#endif
