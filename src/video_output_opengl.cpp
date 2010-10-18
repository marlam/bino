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

#include "video_output_opengl.h"
#include "video_output_opengl.fs.glsl.h"
#include "xgl.h"


/* Global variables and callbacks for GLUT */

static video_output_opengl *global_video_output_gl = NULL;

void global_video_output_opengl_display(void)
{
    global_video_output_gl->display();
}

void global_video_output_opengl_reshape(int w, int h)
{
    global_video_output_gl->reshape(w, h);
}

void global_video_output_opengl_keyboard(unsigned char key, int x, int y)
{
    global_video_output_gl->keyboard(key, x, y);
}

void global_video_output_opengl_special(int key, int x, int y)
{
    global_video_output_gl->special(key, x, y);
}


/* Class implementation */

video_output_opengl::video_output_opengl() throw ()
    : video_output(), _glut_initialized(false)
{
}

video_output_opengl::~video_output_opengl()
{
}

void video_output_opengl::init_glut()
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

bool video_output_opengl::supports_stereo()
{
    init_glut();
    unsigned int display_mode = GLUT_RGBA | GLUT_DOUBLE | GLUT_STEREO;
    glutInitDisplayMode(display_mode);
    return (glutGet(GLUT_DISPLAY_MODE_POSSIBLE) == 1);
}

void video_output_opengl::open(
        video_frame_format preferred_frame_format,
        int src_width, int src_height, float src_aspect_ratio,
        int mode, const struct state &state, unsigned int flags,
        int win_width, int win_height)
{
    if (global_video_output_gl)
    {
        throw exc("cannot open a second gl output");
    }
    global_video_output_gl = this;

    _preferred_frame_format = preferred_frame_format;
    _mode = static_cast<video_output::mode>(mode);
    _src_width = src_width;
    _src_height = src_height;
    _src_aspect_ratio = src_aspect_ratio;

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
    _screen_pixel_aspect_ratio = 1.0f;
    float screen_width_px = glutGet(GLUT_SCREEN_WIDTH);
    float screen_width_mm = glutGet(GLUT_SCREEN_WIDTH_MM);
    float screen_height_px = glutGet(GLUT_SCREEN_HEIGHT);
    float screen_height_mm = glutGet(GLUT_SCREEN_HEIGHT_MM);
    if (screen_width_px > 0.0f && screen_height_px > 0.0f
            && screen_width_mm > 0.0f && screen_height_mm > 0.0f)
    {
        float pixel_width = screen_width_mm / screen_width_px;
        float pixel_height = screen_height_mm / screen_height_px;
        _screen_pixel_aspect_ratio = pixel_width / pixel_height;
        if (std::fabs(_screen_pixel_aspect_ratio - 1.0f) < 0.03f)
        {
            // This screen most probably has square pixels, and the difference to 1.0
            // is only due to slightly inaccurate measurements and rounding. Force
            // 1.0 so that the user gets expected results.
            _screen_pixel_aspect_ratio = 1.0f;
        }
        msg::inf("display:");
        msg::inf("    %gx%g pixels, %gx%g millimeters, pixel aspect ratio %g:1",
                screen_width_px, screen_height_px, screen_width_mm, screen_height_mm,
                _screen_pixel_aspect_ratio);
    }
    if (state.fullscreen)
    {
        win_width = glutGet(GLUT_SCREEN_WIDTH);
        win_height = glutGet(GLUT_SCREEN_HEIGHT);
    }
    else
    {
        if (win_width < 0)
        {
            win_width = src_width;
            if (_mode == left_right)
            {
                win_width *= 2;
            }
        }
        if (win_height < 0)
        {
            win_height = src_height;
            if (_mode == top_bottom)
            {
                win_height *= 2;
            }
        }
        float win_ar = win_width * _screen_pixel_aspect_ratio / win_height;
        if (_mode == left_right)
        {
            win_ar /= 2.0f;
        }
        else if (_mode == top_bottom)
        {
            win_ar *= 2.0f;
        }
        if (src_aspect_ratio >= win_ar)
        {
            win_width *= src_aspect_ratio / win_ar;
        }
        else
        {
            win_height *= win_ar / src_aspect_ratio;
        }
        int max_win_width = glutGet(GLUT_SCREEN_WIDTH);
        max_win_width -= max_win_width / 20;
        if (win_width > max_win_width)
        {
            win_width = max_win_width;
        }
        int max_win_height = glutGet(GLUT_SCREEN_HEIGHT);
        max_win_height -= max_win_height / 20;
        if (win_height > max_win_height)
        {
            win_height = max_win_height;
        }
    }
    glutInitWindowSize(win_width, win_height);
    if ((flags & center) && !state.fullscreen)
    {
        glutInitWindowPosition(
                (glutGet(GLUT_SCREEN_WIDTH) - win_width) / 2,
                (glutGet(GLUT_SCREEN_HEIGHT) - win_height) / 2);
    }
    _win_width = win_width;
    _win_height = win_height;
    _window_id = glutCreateWindow(PACKAGE_NAME);
    if (state.fullscreen)
    {
        glutFullScreen();
        glutSetCursor(GLUT_CURSOR_NONE);
    }
    _state = state;
    _input_is_mono = false;

    glutDisplayFunc(global_video_output_opengl_display);
    glutReshapeFunc(global_video_output_opengl_reshape);
    glutKeyboardFunc(global_video_output_opengl_keyboard);
    glutSpecialFunc(global_video_output_opengl_special);

    /* Initialize OpenGL */

    GLenum err = glewInit();
    if (err != GLEW_OK)
    {
        throw exc(std::string("cannot initialize GLEW: ")
                + reinterpret_cast<const char *>(glewGetErrorString(err)));
    }

    _use_non_power_of_two = true;
    if (!glewIsSupported("GL_ARB_texture_non_power_of_two"))
    {
        _use_non_power_of_two = false;
        msg::wrn("OpenGL extension GL_ARB_texture_non_power_of_two is not available:");
        msg::wrn("    - Inefficient use of graphics memory");
    }

    _yuv420p_supported = true;
    if (glewIsSupported("GL_ARB_fragment_shader"))
    {
        GLint tex_units;
        glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &tex_units);
        if (_mode == anaglyph_red_cyan_monochrome
                || _mode == anaglyph_red_cyan_full_color
                || _mode == anaglyph_red_cyan_half_color
                || _mode == anaglyph_red_cyan_dubois)
        {
            if (tex_units < 6)
            {
                _yuv420p_supported = false;
                if (_preferred_frame_format == yuv420p)
                {
                    msg::wrn("Not enough texture image units for anaglyph output:");
                    msg::wrn("    Disabling hardware accelerated color space conversion");
                }
            }
        }
        else
        {
            if (tex_units < 3)
            {
                _yuv420p_supported = false;
                if (_preferred_frame_format == yuv420p)
                {
                    msg::wrn("Not enough texture image units:");
                    msg::wrn("    Disabling hardware accelerated color space conversion");
                }
            }
        }
        std::string mode_str = (
                _mode == anaglyph_red_cyan_monochrome ? "mode_anaglyph_monochrome"
                : _mode == anaglyph_red_cyan_full_color ? "mode_anaglyph_full_color"
                : _mode == anaglyph_red_cyan_half_color ? "mode_anaglyph_half_color"
                : _mode == anaglyph_red_cyan_dubois ? "mode_anaglyph_dubois"
                : "mode_onechannel");
        std::string input_str = (frame_format() == yuv420p ? "input_yuv420p" : "input_rgb24");
        std::string fs_src = xgl::ShaderSourcePrep(VIDEO_OUTPUT_OPENGL_FS_GLSL_STR,
                std::string("$mode=") + mode_str + ", $input=" + input_str);
        _prg = xgl::CreateProgram("video_output", "", "", fs_src);
        xgl::LinkProgram("video_output", _prg);
        glUseProgram(_prg);
        if (frame_format() == yuv420p)
        {
            glUniform1i(glGetUniformLocation(_prg, "y_l"), 0);
            glUniform1i(glGetUniformLocation(_prg, "u_l"), 1);
            glUniform1i(glGetUniformLocation(_prg, "v_l"), 2);
            if (_mode == anaglyph_red_cyan_monochrome
                    || _mode == anaglyph_red_cyan_full_color
                    || _mode == anaglyph_red_cyan_half_color
                    || _mode == anaglyph_red_cyan_dubois)
            {
                glUniform1i(glGetUniformLocation(_prg, "y_r"), 3);
                glUniform1i(glGetUniformLocation(_prg, "u_r"), 4);
                glUniform1i(glGetUniformLocation(_prg, "v_r"), 5);
            }
        }
        else
        {
            glUniform1i(glGetUniformLocation(_prg, "rgb_l"), 0);
            if (_mode == anaglyph_red_cyan_monochrome
                    || _mode == anaglyph_red_cyan_full_color
                    || _mode == anaglyph_red_cyan_half_color
                    || _mode == anaglyph_red_cyan_dubois)
            {
                glUniform1i(glGetUniformLocation(_prg, "rgb_r"), 1);
            }
        }
    }
    else
    {
        _prg = 0;
        _yuv420p_supported = false;
        msg::wrn("OpenGL extension GL_ARB_fragment_shader is not available:");
        if (_preferred_frame_format == yuv420p)
        {
            msg::wrn("    - Color space conversion will not be hardware accelerated");
        }
        msg::wrn("    - Color adjustments will not be possible");
        if (_mode == anaglyph_red_cyan_monochrome
                || _mode == anaglyph_red_cyan_half_color
                || _mode == anaglyph_red_cyan_dubois)
        {
            msg::wrn("    - Falling back to the low quality anaglyph-full-color method");
            _mode = anaglyph_red_cyan_full_color;
        }
    }

    glEnable(GL_TEXTURE_2D);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    if (frame_format() == yuv420p)
    {
        for (int i = 0; i < 2; i++)
        {
            for (int j = 0; j < 2; j++)
            {
                glGenTextures(1, &(_y_tex[i][j]));
                glBindTexture(GL_TEXTURE_2D, _y_tex[i][j]);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glGenTextures(1, &(_u_tex[i][j]));
                glBindTexture(GL_TEXTURE_2D, _u_tex[i][j]);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glGenTextures(1, &(_v_tex[i][j]));
                glBindTexture(GL_TEXTURE_2D, _v_tex[i][j]);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            }
        }
    }
    else
    {
        for (int i = 0; i < 2; i++)
        {
            for (int j = 0; j < 2; j++)
            {
                glGenTextures(1, &(_rgb_tex[i][j]));
                glBindTexture(GL_TEXTURE_2D, _rgb_tex[i][j]);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            }
        }
    }
    _active_tex_set = 0;
}

video_frame_format video_output_opengl::frame_format() const
{
    if (_preferred_frame_format == yuv420p && _yuv420p_supported)
    {
        return yuv420p;
    }
    else
    {
        return rgb24;
    }
}

void video_output_opengl::bind_textures(int unitset, int index)
{
    if (frame_format() == yuv420p)
    {
        glActiveTexture(GL_TEXTURE0 + 3 * unitset + 0);
        glBindTexture(GL_TEXTURE_2D, _y_tex[_active_tex_set][index]);
        glActiveTexture(GL_TEXTURE0 + 3 * unitset + 1);
        glBindTexture(GL_TEXTURE_2D, _u_tex[_active_tex_set][index]);
        glActiveTexture(GL_TEXTURE0 + 3 * unitset + 2);
        glBindTexture(GL_TEXTURE_2D, _v_tex[_active_tex_set][index]);
    }
    else
    {
        glActiveTexture(GL_TEXTURE0 + unitset);
        glBindTexture(GL_TEXTURE_2D, _rgb_tex[_active_tex_set][index]);
    }
}

void video_output_opengl::draw_full_quad()
{
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(-1.0f, 1.0f);
    glTexCoord2f(_tex_max_x, 0.0f);
    glVertex2f(1.0f, 1.0f);
    glTexCoord2f(_tex_max_x, _tex_max_y);
    glVertex2f(1.0f, -1.0f);
    glTexCoord2f(0.0f, _tex_max_y);
    glVertex2f(-1.0f, -1.0f);
    glEnd();
}

void video_output_opengl::display()
{
    if (glutGetWindow() == 0)
    {
        return;
    }

    int left = 0;
    int right = (_input_is_mono ? 0 : 1);
    if (_state.swap_eyes)
    {
        std::swap(left, right);
    }

    glClear(GL_COLOR_BUFFER_BIT);
    if (_prg != 0)
    {
        glUniform1f(glGetUniformLocation(_prg, "contrast"), _state.contrast);
        glUniform1f(glGetUniformLocation(_prg, "brightness"), _state.brightness);
        glUniform1f(glGetUniformLocation(_prg, "saturation"), _state.saturation);
        glUniform1f(glGetUniformLocation(_prg, "cos_hue"), std::cos(_state.hue * M_PI));
        glUniform1f(glGetUniformLocation(_prg, "sin_hue"), std::sin(_state.hue * M_PI));
    }

    if (_mode == stereo)
    {
        glDrawBuffer(GL_BACK_LEFT);
        bind_textures(0, left);
        draw_full_quad();
        glDrawBuffer(GL_BACK_RIGHT);
        bind_textures(0, right);
        draw_full_quad();
    }
    else if (_mode == even_odd_rows || _mode == even_odd_columns || _mode == checkerboard)
    {
        glEnable(GL_STENCIL_TEST);
        glStencilFunc(GL_EQUAL, 1, 1);
        bind_textures(0, left);
        draw_full_quad();
        glStencilFunc(GL_NOTEQUAL, 1, 1);
        bind_textures(0, right);
        draw_full_quad();
        glDisable(GL_STENCIL_TEST);
    }
    else if (_prg != 0
            && (_mode == anaglyph_red_cyan_monochrome
                || _mode == anaglyph_red_cyan_full_color
                || _mode == anaglyph_red_cyan_half_color
                || _mode == anaglyph_red_cyan_dubois))
    {
        bind_textures(0, left);
        bind_textures(1, right);
        draw_full_quad();
    }
    else if (_prg == 0 && _mode == anaglyph_red_cyan_full_color)
    {
        glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE);
        bind_textures(0, left);
        draw_full_quad();
        glColorMask(GL_FALSE, GL_TRUE, GL_TRUE, GL_FALSE);
        bind_textures(0, right);
        draw_full_quad();
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }
    else if (_mode == mono_left)
    {
        bind_textures(0, left);
        draw_full_quad();
    }
    else if (_mode == mono_right)
    {
        bind_textures(0, right);
        draw_full_quad();
    }
    else if (_mode == left_right || _mode == left_right_half)
    {
        bind_textures(0, left);
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f);
        glVertex2f(-1.0f, 1.0f);
        glTexCoord2f(_tex_max_x, 0.0f);
        glVertex2f(0.0f, 1.0f);
        glTexCoord2f(_tex_max_x, _tex_max_y);
        glVertex2f(0.0f, -1.0f);
        glTexCoord2f(0.0f, _tex_max_y);
        glVertex2f(-1.0f, -1.0f);
        glEnd();
        bind_textures(0, right);
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f);
        glVertex2f(0.0f, 1.0f);
        glTexCoord2f(_tex_max_x, 0.0f);
        glVertex2f(1.0f, 1.0f);
        glTexCoord2f(_tex_max_x, _tex_max_y);
        glVertex2f(1.0f, -1.0f);
        glTexCoord2f(0.0f, _tex_max_y);
        glVertex2f(0.0f, -1.0f);
        glEnd();
    }
    else if (_mode == top_bottom || _mode == top_bottom_half)
    {
        bind_textures(0, left);
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f);
        glVertex2f(-1.0f, 1.0f);
        glTexCoord2f(_tex_max_x, 0.0f);
        glVertex2f(1.0f, 1.0f);
        glTexCoord2f(_tex_max_x, _tex_max_y);
        glVertex2f(1.0f, 0.0f);
        glTexCoord2f(0.0f, _tex_max_y);
        glVertex2f(-1.0f, 0.0f);
        glEnd();
        bind_textures(0, right);
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f);
        glVertex2f(-1.0f, 0.0f);
        glTexCoord2f(_tex_max_x, 0.0f);
        glVertex2f(1.0f, 0.0f);
        glTexCoord2f(_tex_max_x, _tex_max_y);
        glVertex2f(1.0f, -1.0f);
        glTexCoord2f(0.0f, _tex_max_y);
        glVertex2f(-1.0f, -1.0f);
        glEnd();
    }
    glutSwapBuffers();
}

void video_output_opengl::reshape(int w, int h)
{
    if (glutGetWindow() == 0)
    {
        return;
    }

    float dst_w = w;
    float dst_h = h;
    float dst_ar = dst_w * _screen_pixel_aspect_ratio / dst_h;
    float src_ar = _src_aspect_ratio;
    if (_mode == left_right)
    {
        src_ar *= 2.0f;
    }
    else if (_mode == top_bottom)
    {
        src_ar /= 2.0f;
    }
    int vp_w = w;
    int vp_h = h;
    if (src_ar >= dst_ar)
    {
        // need black borders top and bottom
        vp_h = dst_ar / src_ar * dst_h;
    }
    else
    {
        // need black borders left and right
        vp_w = src_ar / dst_ar * dst_w;
    }
    int vp_x = (w - vp_w) / 2;
    int vp_y = (h - vp_h) / 2;

    glViewport(vp_x, vp_y, vp_w, vp_h);
    if (!_state.fullscreen)
    {
        _win_width = w;
        _win_height = h;
    }

    // Set up stencil buffers if needed
    if (_mode == even_odd_rows || _mode == even_odd_columns || _mode == checkerboard)
    {
        glClearStencil(0);
        glClear(GL_STENCIL_BUFFER_BIT);
        glDisable(GL_STENCIL_TEST);
        glStencilFunc(GL_REPLACE, 1, 1);
        glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
        // When writing into the stencil buffer, GL_UNSIGNED_BYTE seems to be much faster than GL_BITMAP
        if (_mode == even_odd_rows)
        {
            GLubyte data[w];
            std::memset(data, 0xff, w);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, w);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            // we count lines from the top, OpenGL counts from the bottom
            for (int y = (h % 2 == 0 ? 1 : 0); y < h; y += 2)
            {
                glWindowPos2i(0, y);
                glDrawPixels(w, 1, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, data);
            }
        }
        else if (_mode == even_odd_columns)
        {
            GLubyte data[h];
            std::memset(data, 0xff, h);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 1);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            for (int x = 0; x < w; x += 2)
            {
                glWindowPos2i(x, 0);
                glDrawPixels(1, h, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, data);
            }
        }
        else
        {
            GLubyte data[w + 1];
            for (int i = 0; i <= w; i++)
            {
                data[i] = (i % 2 == 0 ? 0x00 : 0xff);
            }
            glPixelStorei(GL_UNPACK_ROW_LENGTH, w);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            for (int y = 0; y < h; y++)
            {
                glWindowPos2i(0, y);
                glDrawPixels(w, 1, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE,
                        data + (y % 2 == (h % 2 == 0 ? 0 : 1) ? 0 : 1));
            }
        }
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    }
}

void video_output_opengl::keyboard(unsigned char key, int, int)
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

void video_output_opengl::special(int key, int, int)
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

static int get_row_alignment(const uint8_t *ptr, size_t line_size)
{
    uintptr_t p = reinterpret_cast<uintptr_t>(ptr);
    int row_alignment = 1;
    if (p % 8 == 0 && line_size % 8 == 0)
    {
        row_alignment = 8;
    }
    else if (p % 4 == 0 && line_size % 4 == 0)
    {
        row_alignment = 4;
    }
    else if (p % 2 == 0 && line_size % 2 == 0)
    {
        row_alignment = 2;
    }
    return row_alignment;
}

static int make_power_of_two(int x)
{
    int y = 1;
    while (y < x)
    {
        y *= 2;
    }
    return y;
}

void video_output_opengl::prepare(
        uint8_t *l_data[3], size_t l_line_size[3],
        uint8_t *r_data[3], size_t r_line_size[3])
{
    if (glutGetWindow() == 0)
    {
        return;
    }

    int tex_set = (_active_tex_set == 0 ? 1 : 0);
    _input_is_mono = (l_data[0] == r_data[0] && l_data[1] == r_data[1] && l_data[2] == r_data[2]);

    int tex_width = _src_width;
    int tex_height = _src_height;
    _tex_max_x = 1.0f;
    _tex_max_y = 1.0f;
    if (!_use_non_power_of_two)
    {
        tex_width = make_power_of_two(_src_width);
        tex_height = make_power_of_two(_src_height);
        _tex_max_x = static_cast<float>(_src_width) / static_cast<float>(tex_width);
        _tex_max_y = static_cast<float>(_src_height) / static_cast<float>(tex_height);
    }

    glActiveTexture(GL_TEXTURE0);
    if (frame_format() == yuv420p)
    {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, l_line_size[0]);
        glPixelStorei(GL_UNPACK_ALIGNMENT, get_row_alignment(l_data[0], l_line_size[0]));
        glBindTexture(GL_TEXTURE_2D, _y_tex[tex_set][0]);
        if (_use_non_power_of_two)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8, _src_width, _src_height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, l_data[0]);
        }
        else
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8, tex_width, tex_height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _src_width, _src_height, GL_LUMINANCE, GL_UNSIGNED_BYTE, l_data[0]);
        }
        glPixelStorei(GL_UNPACK_ROW_LENGTH, l_line_size[1]);
        glPixelStorei(GL_UNPACK_ALIGNMENT, get_row_alignment(l_data[1], l_line_size[1]));
        glBindTexture(GL_TEXTURE_2D, _u_tex[tex_set][0]);
        if (_use_non_power_of_two)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8, _src_width / 2, _src_height / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, l_data[1]);
        }
        else
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8, tex_width / 2, tex_height / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _src_width / 2, _src_height / 2, GL_LUMINANCE, GL_UNSIGNED_BYTE, l_data[1]);
        }
        glPixelStorei(GL_UNPACK_ROW_LENGTH, l_line_size[2]);
        glPixelStorei(GL_UNPACK_ALIGNMENT, get_row_alignment(l_data[2], l_line_size[2]));
        glBindTexture(GL_TEXTURE_2D, _v_tex[tex_set][0]);
        if (_use_non_power_of_two)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8, _src_width / 2, _src_height / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, l_data[2]);
        }
        else
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8, tex_width / 2, tex_height / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _src_width / 2, _src_height / 2, GL_LUMINANCE, GL_UNSIGNED_BYTE, l_data[2]);
        }
        if (!_input_is_mono)
        {
            glPixelStorei(GL_UNPACK_ROW_LENGTH, r_line_size[0]);
            glPixelStorei(GL_UNPACK_ALIGNMENT, get_row_alignment(r_data[0], r_line_size[0]));
            glBindTexture(GL_TEXTURE_2D, _y_tex[tex_set][1]);
            if (_use_non_power_of_two)
            {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8, _src_width, _src_height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, r_data[0]);
            }
            else
            {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8, tex_width, tex_height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _src_width, _src_height, GL_LUMINANCE, GL_UNSIGNED_BYTE, r_data[0]);
            }
            glPixelStorei(GL_UNPACK_ROW_LENGTH, r_line_size[1]);
            glPixelStorei(GL_UNPACK_ALIGNMENT, get_row_alignment(r_data[1], r_line_size[1]));
            glBindTexture(GL_TEXTURE_2D, _u_tex[tex_set][1]);
            if (_use_non_power_of_two)
            {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8, _src_width / 2, _src_height / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, r_data[1]);
            }
            else
            {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8, tex_width / 2, tex_height / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _src_width / 2, _src_height / 2, GL_LUMINANCE, GL_UNSIGNED_BYTE, r_data[1]);
            }
            glPixelStorei(GL_UNPACK_ROW_LENGTH, r_line_size[2]);
            glPixelStorei(GL_UNPACK_ALIGNMENT, get_row_alignment(r_data[2], r_line_size[2]));
            glBindTexture(GL_TEXTURE_2D, _v_tex[tex_set][1]);
            if (_use_non_power_of_two)
            {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8, _src_width / 2, _src_height / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, r_data[2]);
            }
            else
            {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8, tex_width / 2, tex_height / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _src_width / 2, _src_height / 2, GL_LUMINANCE, GL_UNSIGNED_BYTE, r_data[2]);
            }
        }
    }
    else if (frame_format() == rgb24)
    {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, l_line_size[0] / 3);
        glPixelStorei(GL_UNPACK_ALIGNMENT, get_row_alignment(l_data[0], l_line_size[0]));
        glBindTexture(GL_TEXTURE_2D, _rgb_tex[tex_set][0]);
        if (_use_non_power_of_two)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, _src_width, _src_height, 0, GL_RGB, GL_UNSIGNED_BYTE, l_data[0]);
        }
        else
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, tex_width, tex_height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _src_width, _src_height, GL_RGB, GL_UNSIGNED_BYTE, l_data[0]);
        }
        if (!_input_is_mono)
        {
            glPixelStorei(GL_UNPACK_ROW_LENGTH, r_line_size[0] / 3);
            glPixelStorei(GL_UNPACK_ALIGNMENT, get_row_alignment(r_data[0], r_line_size[0]));
            glBindTexture(GL_TEXTURE_2D, _rgb_tex[tex_set][1]);
            if (_use_non_power_of_two)
            {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, _src_width, _src_height, 0, GL_RGB, GL_UNSIGNED_BYTE, r_data[0]);
            }
            else
            {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, tex_width, tex_height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _src_width, _src_height, GL_RGB, GL_UNSIGNED_BYTE, r_data[0]);
            }
        }
    }
}

void video_output_opengl::activate()
{
    if (glutGetWindow() == 0)
    {
        return;
    }

    _active_tex_set = (_active_tex_set == 0 ? 1 : 0);
    glutPostRedisplay();
}

void video_output_opengl::process_events()
{
    if (glutGetWindow() == 0)
    {
        send_cmd(command::quit);
        return;
    }

    glutMainLoopEvent();
}

void video_output_opengl::close()
{
    if (glutGetWindow() != 0)
    {
        glutSetCursor(GLUT_CURSOR_INHERIT);
        glutDestroyWindow(_window_id);
    }
    glutLeaveMainLoop();
    global_video_output_gl = NULL;
}

void video_output_opengl::receive_notification(const notification &note)
{
    switch (note.type)
    {
    case notification::swap_eyes:
        _state.swap_eyes = note.current.flag;
        glutPostRedisplay();
        break;
    case notification::fullscreen:
        if (note.previous.flag != note.current.flag)
        {
            if (note.previous.flag)
            {
                _state.fullscreen = false;
                glutReshapeWindow(_win_width, _win_height);
                glutSetCursor(GLUT_CURSOR_INHERIT);
            }
            else
            {
                _state.fullscreen = true;
                glutFullScreen();
                glutSetCursor(GLUT_CURSOR_NONE);
            }
        }
        break;
    case notification::pause:
        break;
    case notification::contrast:
        _state.contrast = note.current.value;
        glutPostRedisplay();
        break;
    case notification::brightness:
        _state.brightness = note.current.value;
        glutPostRedisplay();
        break;
    case notification::hue:
        _state.hue = note.current.value;
        glutPostRedisplay();
        break;
    case notification::saturation:
        _state.saturation = note.current.value;
        glutPostRedisplay();
        break;
    case notification::pos:
        break;
    }
}
