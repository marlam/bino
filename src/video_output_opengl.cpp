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

#include "exc.h"
#include "msg.h"
#include "str.h"

#include "video_output_opengl.h"
#include "video_output_opengl.fs.glsl.h"
#include "xgl.h"


video_output_opengl::video_output_opengl() throw () : video_output()
{
}

video_output_opengl::~video_output_opengl()
{
}

void video_output_opengl::set_source_info(int width, int height, float aspect_ratio, video_frame_format preferred_frame_format)
{
    _src_width = width;
    _src_height = height;
    _src_aspect_ratio = aspect_ratio;
    _src_preferred_frame_format = preferred_frame_format;
}

void video_output_opengl::set_screen_info(float pixel_aspect_ratio)
{
    _screen_pixel_aspect_ratio = pixel_aspect_ratio;
}

void video_output_opengl::set_mode(video_output::mode mode)
{
    _mode = mode;
}

void video_output_opengl::set_win_size(int width, int height)
{
    _win_width = width;
    _win_height = height;
}

void video_output_opengl::set_state(const video_output_state &state)
{
    _state = state;
}

void video_output_opengl::swap_tex_set()
{
    _active_tex_set = (_active_tex_set == 0 ? 1 : 0);
}

void video_output_opengl::initialize(bool have_texture_non_power_of_two, bool have_fragment_shader)
{
    _use_non_power_of_two = true;
    if (!have_texture_non_power_of_two)
    {
        _use_non_power_of_two = false;
        msg::wrn("OpenGL extension GL_ARB_texture_non_power_of_two is not available:");
        msg::wrn("    - Inefficient use of graphics memory");
    }

    _yuv420p_supported = true;
    if (have_fragment_shader)
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
                if (_src_preferred_frame_format == yuv420p)
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
                if (_src_preferred_frame_format == yuv420p)
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
        if (_src_preferred_frame_format == yuv420p)
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

void video_output_opengl::deinitialize()
{
    if (_prg != 0)
    {
        xgl::DeleteProgram(_prg);
    }
    if (frame_format() == yuv420p)
    {
        glDeleteTextures(2 * 2, reinterpret_cast<GLuint *>(_y_tex));
        glDeleteTextures(2 * 2, reinterpret_cast<GLuint *>(_u_tex));
        glDeleteTextures(2 * 2, reinterpret_cast<GLuint *>(_v_tex));
    }
    else
    {
        glDeleteTextures(2 * 2, reinterpret_cast<GLuint *>(_rgb_tex));
    }

}

video_frame_format video_output_opengl::frame_format() const
{
    if (_src_preferred_frame_format == yuv420p && _yuv420p_supported)
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

void video_output_opengl::display(video_output::mode mode)
{
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

    if (mode == stereo)
    {
        glDrawBuffer(GL_BACK_LEFT);
        bind_textures(0, left);
        draw_full_quad();
        glDrawBuffer(GL_BACK_RIGHT);
        bind_textures(0, right);
        draw_full_quad();
    }
    else if (mode == even_odd_rows || mode == even_odd_columns || mode == checkerboard)
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
            && (mode == anaglyph_red_cyan_monochrome
                || mode == anaglyph_red_cyan_full_color
                || mode == anaglyph_red_cyan_half_color
                || mode == anaglyph_red_cyan_dubois))
    {
        bind_textures(0, left);
        bind_textures(1, right);
        draw_full_quad();
    }
    else if (_prg == 0 && mode == anaglyph_red_cyan_full_color)
    {
        glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE);
        bind_textures(0, left);
        draw_full_quad();
        glColorMask(GL_FALSE, GL_TRUE, GL_TRUE, GL_FALSE);
        bind_textures(0, right);
        draw_full_quad();
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }
    else if (mode == mono_left)
    {
        bind_textures(0, left);
        draw_full_quad();
    }
    else if (mode == mono_right)
    {
        bind_textures(0, right);
        draw_full_quad();
    }
    else if (mode == left_right || mode == left_right_half)
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
    else if (mode == top_bottom || mode == top_bottom_half)
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
}

void video_output_opengl::reshape(int w, int h)
{
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
            std::vector<GLubyte> data(w, 0xff);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, w);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            // we count lines from the top, OpenGL counts from the bottom
            for (int y = (h % 2 == 0 ? 1 : 0); y < h; y += 2)
            {
                glWindowPos2i(0, y);
                glDrawPixels(w, 1, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, &(data[0]));
            }
        }
        else if (_mode == even_odd_columns)
        {
            std::vector<GLubyte> data(h, 0xff);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 1);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            for (int x = 0; x < w; x += 2)
            {
                glWindowPos2i(x, 0);
                glDrawPixels(1, h, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, &(data[0]));
            }
        }
        else
        {
            std::vector<GLubyte> data(w + 1);
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
                        &(data[(y % 2 == (h % 2 == 0 ? 0 : 1) ? 0 : 1)]));
            }
        }
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
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
