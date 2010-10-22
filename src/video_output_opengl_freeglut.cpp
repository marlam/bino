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

#include "config.h"

#include <vector>
#include <cstdlib>
#include <cmath>
#include <cstring>

#include <GL/glew.h>
#include <GL/freeglut.h>

#include "exc.h"
#include "msg.h"
#include "str.h"

#include "video_output_opengl_freeglut.h"


/* Global variables and callbacks for GLUT */

static video_output_opengl_freeglut *global_video_output_opengl_freeglut = NULL;

void global_video_output_opengl_freeglut_display(void)
{
    global_video_output_opengl_freeglut->display();
}

void global_video_output_opengl_freeglut_reshape(int w, int h)
{
    global_video_output_opengl_freeglut->reshape(w, h);
}

void global_video_output_opengl_freeglut_keyboard(unsigned char key, int x, int y)
{
    global_video_output_opengl_freeglut->keyboard(key, x, y);
}

void global_video_output_opengl_freeglut_special(int key, int x, int y)
{
    global_video_output_opengl_freeglut->special(key, x, y);
}


/* Class implementation */

video_output_opengl_freeglut::video_output_opengl_freeglut() throw ()
    : video_output_opengl(), _glut_initialized(false)
{
}

video_output_opengl_freeglut::~video_output_opengl_freeglut()
{
}

void video_output_opengl_freeglut::init_glut()
{
    if (!_glut_initialized)
    {
        std::vector<const char *> arguments;
        arguments.push_back(PACKAGE_NAME);
        arguments.push_back(NULL);
        int argc = arguments.size() - 1;
        char **argv = const_cast<char **>(&(arguments[0]));
        glutInit(&argc, argv);
        _glut_initialized = true;
    }
}

bool video_output_opengl_freeglut::supports_stereo()
{
    init_glut();
    unsigned int display_mode = GLUT_RGBA | GLUT_DOUBLE | GLUT_STEREO;
    glutInitDisplayMode(display_mode);
    return (glutGet(GLUT_DISPLAY_MODE_POSSIBLE) == 1);
}

void video_output_opengl_freeglut::open(
        video_frame_format preferred_frame_format,
        int src_width, int src_height, float src_aspect_ratio,
        int mode, const video_output_state &state, unsigned int flags,
        int win_width, int win_height)
{
    if (global_video_output_opengl_freeglut)
    {
        throw exc("cannot open a second freeglut output");
    }
    global_video_output_opengl_freeglut = this;

    set_mode(static_cast<video_output::mode>(mode));
    set_source_info(src_width, src_height, src_aspect_ratio, preferred_frame_format);

    /* Initialize freeglut */

    init_glut();
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_CONTINUE_EXECUTION);
    unsigned int display_mode = GLUT_RGBA | GLUT_DOUBLE;
    if (mode == stereo)
    {
        display_mode |= GLUT_STEREO;
    }
    if (mode == even_odd_rows || mode == even_odd_columns || mode == checkerboard)
    {
        display_mode |= GLUT_STENCIL;
    }
    glutInitDisplayMode(display_mode);
    if (glutGet(GLUT_DISPLAY_MODE_POSSIBLE) != 1)
    {
        throw exc("cannot set display mode");
    }
    float screen_pixel_aspect_ratio = 1.0f;
    float screen_width_px = glutGet(GLUT_SCREEN_WIDTH);
    float screen_width_mm = glutGet(GLUT_SCREEN_WIDTH_MM);
    float screen_height_px = glutGet(GLUT_SCREEN_HEIGHT);
    float screen_height_mm = glutGet(GLUT_SCREEN_HEIGHT_MM);
    if (screen_width_px > 0.0f && screen_height_px > 0.0f
            && screen_width_mm > 0.0f && screen_height_mm > 0.0f)
    {
        float pixel_width = screen_width_mm / screen_width_px;
        float pixel_height = screen_height_mm / screen_height_px;
        screen_pixel_aspect_ratio = pixel_width / pixel_height;
        if (std::fabs(screen_pixel_aspect_ratio - 1.0f) < 0.03f)
        {
            // This screen most probably has square pixels, and the difference to 1.0
            // is only due to slightly inaccurate measurements and rounding. Force
            // 1.0 so that the user gets expected results.
            screen_pixel_aspect_ratio = 1.0f;
        }
        msg::inf("display:");
        msg::inf("    %gx%g pixels, %gx%g millimeters, pixel aspect ratio %g:1",
                screen_width_px, screen_height_px, screen_width_mm, screen_height_mm,
                screen_pixel_aspect_ratio);
    }
    set_screen_info(screen_width_px, screen_height_px, screen_pixel_aspect_ratio);
    compute_win_size(win_width, win_height);
    glutInitWindowSize(video_output_opengl::win_width(), video_output_opengl::win_height());
    if ((flags & center) && !state.fullscreen)
    {
        glutInitWindowPosition(
                (screen_width_px - video_output_opengl::win_width()) / 2,
                (screen_height_px - video_output_opengl::win_height()) / 2);
    }
    _window_id = glutCreateWindow(PACKAGE_NAME);
    if (state.fullscreen)
    {
        glutFullScreen();
        glutSetCursor(GLUT_CURSOR_NONE);
    }
    set_state(state);

    glutDisplayFunc(global_video_output_opengl_freeglut_display);
    glutReshapeFunc(global_video_output_opengl_freeglut_reshape);
    glutKeyboardFunc(global_video_output_opengl_freeglut_keyboard);
    glutSpecialFunc(global_video_output_opengl_freeglut_special);

    /* Initialize OpenGL */

    GLenum err = glewInit();
    if (err != GLEW_OK)
    {
        throw exc(std::string("cannot initialize GLEW: ")
                + reinterpret_cast<const char *>(glewGetErrorString(err)));
    }
    initialize(
            glewIsSupported("GL_ARB_texture_non_power_of_two"),
            glewIsSupported("GL_ARB_fragment_shader"));
}

void video_output_opengl_freeglut::display()
{
    video_output_opengl::display();
    glutSwapBuffers();
}

void video_output_opengl_freeglut::keyboard(unsigned char key, int, int)
{
    switch (key)
    {
    case 27:
    case 'q':
        send_cmd(command::quit);
        break;
    case 's':
        send_cmd(command::toggle_swap_eyes);
        break;
    case 'f':
        send_cmd(command::toggle_fullscreen);
        break;
    case ' ':
    case 'p':
        send_cmd(command::toggle_pause);
        break;
    case '1':
        send_cmd(command::adjust_contrast, -0.05f);
        break;
    case '2':
        send_cmd(command::adjust_contrast, +0.05f);
        break;
    case '3':
        send_cmd(command::adjust_brightness, -0.05f);
        break;
    case '4':
        send_cmd(command::adjust_brightness, +0.05f);
        break;
    case '5':
        send_cmd(command::adjust_hue, -0.05f);
        break;
    case '6':
        send_cmd(command::adjust_hue, +0.05f);
        break;
    case '7':
        send_cmd(command::adjust_saturation, -0.05f);
        break;
    case '8':
        send_cmd(command::adjust_saturation, +0.05f);
        break;
    default:
        break;
    }
}

void video_output_opengl_freeglut::special(int key, int, int)
{
    switch (key)
    {
    case GLUT_KEY_LEFT:
        send_cmd(command::seek, -10.0f);
        break;
    case GLUT_KEY_RIGHT:
        send_cmd(command::seek, +10.0f);
        break;
    case GLUT_KEY_UP:
        send_cmd(command::seek, -60.0f);
        break;
    case GLUT_KEY_DOWN:
        send_cmd(command::seek, +60.0f);
        break;
    case GLUT_KEY_PAGE_UP:
        send_cmd(command::seek, -600.0f);
        break;
    case GLUT_KEY_PAGE_DOWN:
        send_cmd(command::seek, +600.0f);
        break;
    default:
        break;
    }
}

void video_output_opengl_freeglut::activate()
{
    if (glutGetWindow() == 0)
    {
        return;
    }

    swap_tex_set();
    glutPostRedisplay();
}

void video_output_opengl_freeglut::process_events()
{
    if (glutGetWindow() == 0)
    {
        send_cmd(command::quit);
        return;
    }

    glutMainLoopEvent();
}

void video_output_opengl_freeglut::close()
{
    if (glutGetWindow() != 0)
    {
        glutSetCursor(GLUT_CURSOR_INHERIT);
        glutDestroyWindow(_window_id);
    }
    glutLeaveMainLoop();
    global_video_output_opengl_freeglut = NULL;
}

void video_output_opengl_freeglut::receive_notification(const notification &note)
{
    switch (note.type)
    {
    case notification::swap_eyes:
        state().swap_eyes = note.current.flag;
        glutPostRedisplay();
        break;
    case notification::fullscreen:
        if (note.previous.flag != note.current.flag)
        {
            if (note.previous.flag)
            {
                state().fullscreen = false;
                glutReshapeWindow(win_width(), win_height());
                glutSetCursor(GLUT_CURSOR_INHERIT);
            }
            else
            {
                state().fullscreen = true;
                glutFullScreen();
                glutSetCursor(GLUT_CURSOR_NONE);
            }
        }
        break;
    case notification::pause:
        break;
    case notification::contrast:
        state().contrast = note.current.value;
        glutPostRedisplay();
        break;
    case notification::brightness:
        state().brightness = note.current.value;
        glutPostRedisplay();
        break;
    case notification::hue:
        state().hue = note.current.value;
        glutPostRedisplay();
        break;
    case notification::saturation:
        state().saturation = note.current.value;
        glutPostRedisplay();
        break;
    case notification::pos:
        break;
    }
}
