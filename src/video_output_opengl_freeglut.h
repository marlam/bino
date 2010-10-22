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

#ifndef VIDEO_OUTPUT_OPENGL_FREEGLUT_H
#define VIDEO_OUTPUT_OPENGL_FREEGLUT_H

#include "video_output_opengl.h"


class video_output_opengl_freeglut : public video_output_opengl
{
private:
    bool _glut_initialized;
    int _window_id;

    void init_glut();
    void display();
    void keyboard(unsigned char key, int x, int y);
    void special(int key, int x, int y);

public:
    video_output_opengl_freeglut() throw ();
    ~video_output_opengl_freeglut();

    virtual bool supports_stereo();

    virtual void open(
            video_frame_format preferred_format,
            int src_width, int src_height, float src_aspect_ratio,
            int mode, const video_output_state &state, unsigned int flags,
            int win_width, int win_height);

    virtual void activate();
    virtual void process_events();

    virtual void close();

    virtual void receive_notification(const notification &note);

friend void global_video_output_opengl_freeglut_display(void);
friend void global_video_output_opengl_freeglut_reshape(int w, int h);
friend void global_video_output_opengl_freeglut_keyboard(unsigned char key, int x, int y);
friend void global_video_output_opengl_freeglut_special(int key, int x, int y);
};

#endif
