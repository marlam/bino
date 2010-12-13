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
#include "timer.h"

#include "video_output_opengl.h"
#include "video_output_opengl_color.fs.glsl.h"
#include "video_output_opengl_render.fs.glsl.h"
#include "xgl.h"


/* Video output overview:
 *
 * Video output happens in three steps: video data input, color correction,
 * and rendering.
 *
 * Step 1: Video data input.
 * We have two texture sets for input: one holding the current video frame,
 * and one for preparing the next video frame. Each texture set has textures
 * for the left and right view. The video data is transferred to texture
 * memory using pixel buffer objects, for better performance.
 *
 * Step 2: Color correction.
 * The input data is first converted to YUV (for the most common yuv420p
 * input format, this just means gathering of the three components from the
 * three planes). Then color adjustment in the YUV space is performed.
 * Finally the result is converted to sRGB and stored in an GL_SRGB texture.
 * In this color correction step, no interpolation is done, because we're
 * dealing with non-linear values, and interpolating them would lead to
 * errors. We do not convert to linear RGB (as opposed to sRGB) in this step
 * because storing linear RGB in a GL_RGB texture would lose some precision
 * when compared to the non-linear input data.
 *
 * Step 3: Rendering.
 * This step reads from the sRGB textures created in the previous step, which
 * means that the GL will transform the input to linear RGB automatically and
 * handle hardware accelerated bilinear interpolation correctly. Thus,
 * magnification or minification are safe in this step. Furthermore, we can
 * do interpolation on the linear RGB values for the masking output modes.
 * We then transform the resulting linear RGB values back to non-linear sRGB
 * values for output. We do not use the GL_ARB_framebuffer_sRGB extension for
 * this purpose because 1) we need computations on non-linear values for the
 * anaglyph methods and 2) sRGB framebuffers are not yet widely supported.
 *
 * Open issues / TODO:
 * 1. The conversion from YUV to non-linear RGB assumes ITU.BT-601 input.
 *    This is wrong in case of HDTV, where ITU.BT-709 would be correct.
 *    This is also wrong in case FFmpeg gives us input which uses the full
 *    value range 0-255 for each component (should not happen for video data).
 *    We should get the information which conversion to use from FFmpeg,
 *    but I currently don't know how.
 * 2. I *think* that the matrices used for the Dubois method in the rendering
 *    shader should be applied to non-linear RGB values, but I'm not 100%
 *    sure.
 */


video_output_opengl::video_output_opengl(bool receive_notifications) throw ()
    : video_output(receive_notifications), _initialized(false)
{
}

video_output_opengl::~video_output_opengl()
{
}

void video_output_opengl::set_source_info(int width, int height, float aspect_ratio,
        enum decoder::video_frame_format preferred_frame_format)
{
    _src_width = width;
    _src_height = height;
    _src_aspect_ratio = aspect_ratio;
    _src_preferred_frame_format = preferred_frame_format;
}

void video_output_opengl::set_screen_info(int width, int height, float pixel_aspect_ratio)
{
    _screen_width = width;
    _screen_height = height;
    _screen_pixel_aspect_ratio = pixel_aspect_ratio;
}

void video_output_opengl::set_mode(enum video_output::mode mode)
{
    _mode = mode;
}

void video_output_opengl::compute_win_size(int win_width, int win_height)
{
    _win_width = win_width;
    _win_height = win_height;
    if (_win_width < 0)
    {
        _win_width = _src_width;
        if (_mode == left_right)
        {
                _win_width *= 2;
        }
    }
    if (_win_height < 0)
    {
        _win_height = _src_height;
        if (_mode == top_bottom)
        {
            _win_height *= 2;
        }
    }
    float _win_ar = _win_width * _screen_pixel_aspect_ratio / _win_height;
    if (_mode == left_right)
    {
        _win_ar /= 2.0f;
    }
    else if (_mode == top_bottom)
    {
        _win_ar *= 2.0f;
    }
    if (_src_aspect_ratio >= _win_ar)
    {
        _win_width *= _src_aspect_ratio / _win_ar;
    }
    else
    {
        _win_height *= _win_ar / _src_aspect_ratio;
    }
    int max_win_width = _screen_width - _screen_width / 10;
    if (_win_width > max_win_width)
    {
        _win_width = max_win_width;
    }
    int max_win_height = _screen_height - _screen_height / 10;
    if (_win_height > max_win_height)
    {
        _win_height = max_win_height;
    }
}

void video_output_opengl::set_state(const video_output_state &state)
{
    _state = state;
}

void video_output_opengl::swap_tex_set()
{
    _active_tex_set = (_active_tex_set == 0 ? 1 : 0);
}

void video_output_opengl::initialize()
{
    if (_initialized)
    {
        deinitialize();
    }

    // XXX: Hack: work around broken SRGB texture implementations
    _srgb_textures_are_broken = std::getenv("SRGB_TEXTURES_ARE_BROKEN");

    // Step 1: input of video data
    _active_tex_set = 0;
    _input_is_mono = false;
    _have_valid_data[0] = false;
    _have_valid_data[1] = false;
    glGenBuffers(1, &_pbo);
    if (frame_format() == decoder::frame_format_yuv420p)
    {
        for (int i = 0; i < 2; i++)
        {
            for (int j = 0; j < 2; j++)
            {
                glGenTextures(1, &(_yuv420p_y_tex[i][j]));
                glBindTexture(GL_TEXTURE_2D, _yuv420p_y_tex[i][j]);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8, _src_width, _src_height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
                glGenTextures(1, &(_yuv420p_u_tex[i][j]));
                glBindTexture(GL_TEXTURE_2D, _yuv420p_u_tex[i][j]);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8, _src_width / 2, _src_height / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
                glGenTextures(1, &(_yuv420p_v_tex[i][j]));
                glBindTexture(GL_TEXTURE_2D, _yuv420p_v_tex[i][j]);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8, _src_width / 2, _src_height / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
            }
        }
    }
    else
    {
        for (int i = 0; i < 2; i++)
        {
            for (int j = 0; j < 2; j++)
            {
                glGenTextures(1, &(_bgra32_tex[i][j]));
                glBindTexture(GL_TEXTURE_2D, _bgra32_tex[i][j]);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, _src_width, _src_height, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
            }
        }
    }

    // Step 2: color-correction
    std::string input_str = (frame_format() == decoder::frame_format_yuv420p
            ? "input_yuv420p" : "input_bgra32");
    std::string color_fs_src = xgl::ShaderSourcePrep(
            VIDEO_OUTPUT_OPENGL_COLOR_FS_GLSL_STR,
            std::string("$input=") + input_str);
    _color_prg = xgl::CreateProgram("video_output_color", "", "", color_fs_src);
    xgl::LinkProgram("video_output_color", _color_prg);
    glGenFramebuffersEXT(1, &_color_fbo);
    for (int j = 0; j < 2; j++)
    {
        glGenTextures(1, &(_srgb_tex[j]));
        glBindTexture(GL_TEXTURE_2D, _srgb_tex[j]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0,
                _srgb_textures_are_broken ? GL_RGB8 : GL_SRGB8,
                _src_width, _src_height, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
    }

    // Step 3: rendering
    std::string mode_str = (
            _mode == even_odd_rows ? "mode_even_odd_rows"
            : _mode == even_odd_columns ? "mode_even_odd_columns"
            : _mode == checkerboard ? "mode_checkerboard"
            : _mode == anaglyph_red_cyan_monochrome ? "mode_anaglyph_monochrome"
            : _mode == anaglyph_red_cyan_full_color ? "mode_anaglyph_full_color"
            : _mode == anaglyph_red_cyan_half_color ? "mode_anaglyph_half_color"
            : _mode == anaglyph_red_cyan_dubois ? "mode_anaglyph_dubois"
            : "mode_onechannel");
    std::string srgb_broken_str = (_srgb_textures_are_broken ? "1" : "0");
    std::string render_fs_src = xgl::ShaderSourcePrep(
            VIDEO_OUTPUT_OPENGL_RENDER_FS_GLSL_STR,
            std::string("$mode=") + mode_str + std::string(", $srgb_broken=") + srgb_broken_str);
    _render_prg = xgl::CreateProgram("video_output_render", "", "", render_fs_src);
    xgl::LinkProgram("video_output_render", _render_prg);
    if (_mode == even_odd_rows || _mode == even_odd_columns || _mode == checkerboard)
    {
        GLubyte even_odd_rows_mask[4] = { 0xff, 0xff, 0x00, 0x00 };
        GLubyte even_odd_columns_mask[4] = { 0xff, 0x00, 0xff, 0x00 };
        GLubyte checkerboard_mask[4] = { 0xff, 0x00, 0x00, 0xff };
        glGenTextures(1, &_mask_tex);
        glBindTexture(GL_TEXTURE_2D, _mask_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8, 2, 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE,
                _mode == even_odd_rows ? even_odd_rows_mask
                : _mode == even_odd_columns ? even_odd_columns_mask
                : checkerboard_mask);
    }

    // Initialize GL things
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    _initialized = true;
}

void video_output_opengl::deinitialize()
{
    if (!_initialized)
    {
        return;
    }

    _have_valid_data[0] = false;
    _have_valid_data[1] = false;
    glDeleteBuffers(1, &_pbo);
    if (frame_format() == decoder::frame_format_yuv420p)
    {
        glDeleteTextures(2 * 2, reinterpret_cast<GLuint *>(_yuv420p_y_tex));
        glDeleteTextures(2 * 2, reinterpret_cast<GLuint *>(_yuv420p_u_tex));
        glDeleteTextures(2 * 2, reinterpret_cast<GLuint *>(_yuv420p_v_tex));
    }
    else
    {
        glDeleteTextures(2 * 2, reinterpret_cast<GLuint *>(_bgra32_tex));
    }
    xgl::DeleteProgram(_color_prg);
    glDeleteFramebuffersEXT(1, &_color_fbo);
    glDeleteTextures(2, reinterpret_cast<GLuint *>(_srgb_tex));
    xgl::DeleteProgram(_render_prg);
    if (_mode == even_odd_rows || _mode == even_odd_columns || _mode == checkerboard)
    {
        glDeleteTextures(1, &_mask_tex);
    }
    _initialized = false;
}

enum decoder::video_frame_format video_output_opengl::frame_format() const
{
    return _src_preferred_frame_format;
}

static void draw_quad(float x, float y, float w, float h)
{
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(x, y);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(x + w, y);
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(x + w, y + h);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(x, y + h);
    glEnd();
}

void video_output_opengl::display(bool toggle_swap_eyes, float x, float y, float w, float h)
{
    glClear(GL_COLOR_BUFFER_BIT);
    if (!_have_valid_data[_active_tex_set])
    {
        return;
    }

    /* Use correct left and right view indices */

    int left = 0;
    int right = (_input_is_mono ? 0 : 1);
    if (_state.swap_eyes)
    {
        std::swap(left, right);
    }
    if (toggle_swap_eyes)
    {
        std::swap(left, right);
    }
    int viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    if ((_mode == even_odd_rows || _mode == checkerboard) && (screen_pos_y() + viewport[1]) % 2 == 0)
    {
        std::swap(left, right);
    }
    if ((_mode == even_odd_columns || _mode == checkerboard) && (screen_pos_x() + viewport[0]) % 2 == 1)
    {
        std::swap(left, right);
    }

    /* Initialize GL things */

    glEnable(GL_TEXTURE_2D);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    /* Step 2: color-correction */

    GLboolean scissor_test = glIsEnabled(GL_SCISSOR_TEST);
    glDisable(GL_SCISSOR_TEST);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glViewport(0, 0, _src_width, _src_height);
    glUseProgram(_color_prg);
    if (frame_format() == decoder::frame_format_yuv420p)
    {
        glUniform1i(glGetUniformLocation(_color_prg, "y_tex"), 0);
        glUniform1i(glGetUniformLocation(_color_prg, "u_tex"), 1);
        glUniform1i(glGetUniformLocation(_color_prg, "v_tex"), 2);
    }
    else
    {
        glUniform1i(glGetUniformLocation(_color_prg, "srgb_tex"), 0);
    }
    glUniform1f(glGetUniformLocation(_color_prg, "contrast"), _state.contrast);
    glUniform1f(glGetUniformLocation(_color_prg, "brightness"), _state.brightness);
    glUniform1f(glGetUniformLocation(_color_prg, "saturation"), _state.saturation);
    glUniform1f(glGetUniformLocation(_color_prg, "cos_hue"), std::cos(_state.hue * M_PI));
    glUniform1f(glGetUniformLocation(_color_prg, "sin_hue"), std::sin(_state.hue * M_PI));
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, _color_fbo);
    // left view: render into _srgb_tex[0]
    if (frame_format() == decoder::frame_format_yuv420p)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, _yuv420p_y_tex[_active_tex_set][left]);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, _yuv420p_u_tex[_active_tex_set][left]);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, _yuv420p_v_tex[_active_tex_set][left]);
    }
    else
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, _bgra32_tex[_active_tex_set][left]);
    }
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,
            GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, _srgb_tex[0], 0);
    draw_quad(-1.0f, +1.0f, +2.0f, -2.0f);
    // right view: render into _srgb_tex[1]
    if (left != right)
    {
        if (frame_format() == decoder::frame_format_yuv420p)
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, _yuv420p_y_tex[_active_tex_set][right]);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, _yuv420p_u_tex[_active_tex_set][right]);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, _yuv420p_v_tex[_active_tex_set][right]);
        }
        else
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, _bgra32_tex[_active_tex_set][right]);
        }
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,
                GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, _srgb_tex[1], 0);
        draw_quad(-1.0f, +1.0f, +2.0f, -2.0f);
    }
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    if (scissor_test)
    {
        glEnable(GL_SCISSOR_TEST);
    }

    // at this point, the left view is in _srgb_tex[0],
    // and the right view (if it exists) is in _srgb_tex[1]
    right = (left != right ? 1 : 0);
    left = 0;

    // Step 3: rendering
    glUseProgram(_render_prg);
    glUniform1i(glGetUniformLocation(_render_prg, "rgb_l"), left);
    glUniform1i(glGetUniformLocation(_render_prg, "rgb_r"), right);
    if (_mode == even_odd_rows || _mode == even_odd_columns || _mode == checkerboard)
    {
        glUniform1i(glGetUniformLocation(_render_prg, "mask_tex"), 2);
        glUniform1f(glGetUniformLocation(_render_prg, "step_x"), 1.0f / static_cast<float>(viewport[2]));
        glUniform1f(glGetUniformLocation(_render_prg, "step_y"), 1.0f / static_cast<float>(viewport[3]));
    }

    if (_mode == stereo)
    {
        glActiveTexture(GL_TEXTURE0);
        glDrawBuffer(GL_BACK_LEFT);
        glBindTexture(GL_TEXTURE_2D, _srgb_tex[left]);
        draw_quad(x, y, w, h);
        glDrawBuffer(GL_BACK_RIGHT);
        glBindTexture(GL_TEXTURE_2D, _srgb_tex[right]);
        draw_quad(x, y, w, h);
    }
    else if (_mode == even_odd_rows || _mode == even_odd_columns || _mode == checkerboard)
    {
        float vpw = static_cast<float>(viewport[2]);
        float vph = static_cast<float>(viewport[3]);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, _srgb_tex[left]);
        if (left != right)
        {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, _srgb_tex[right]);
        }
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, _mask_tex);
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f);
        glMultiTexCoord2f(GL_TEXTURE1, 0.0f, 0.0f);
        glVertex2f(x, y);
        glTexCoord2f(1.0f, 0.0f);
        glMultiTexCoord2f(GL_TEXTURE1, vpw / 2.0f, 0.0f);
        glVertex2f(x + w, y);
        glTexCoord2f(1.0f, 1.0f);
        glMultiTexCoord2f(GL_TEXTURE1, vpw / 2.0f, vph / 2.0f);
        glVertex2f(x + w, y + h);
        glTexCoord2f(0.0f, 1.0f);
        glMultiTexCoord2f(GL_TEXTURE1, 0.0f, vph / 2.0f);
        glVertex2f(x, y + h);
        glEnd();
    }
    else if (_mode == anaglyph_red_cyan_monochrome
            || _mode == anaglyph_red_cyan_full_color
            || _mode == anaglyph_red_cyan_half_color
            || _mode == anaglyph_red_cyan_dubois)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, _srgb_tex[left]);
        if (left != right)
        {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, _srgb_tex[right]);
        }
        draw_quad(x, y, w, h);
    }
    else if (_mode == mono_left)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, _srgb_tex[left]);
        draw_quad(x, y, w, h);
    }
    else if (_mode == mono_right)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, _srgb_tex[right]);
        draw_quad(x, y, w, h);
    }
    else if (_mode == left_right || _mode == left_right_half)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, _srgb_tex[left]);
        draw_quad(-1.0f, -1.0f, 1.0f, 2.0f);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, _srgb_tex[right]);
        draw_quad(0.0f, -1.0f, 1.0f, 2.0f);
    }
    else if (_mode == top_bottom || _mode == top_bottom_half)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, _srgb_tex[left]);
        draw_quad(-1.0f, 0.0f, 2.0f, 1.0f);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, _srgb_tex[right]);
        draw_quad(-1.0f, -1.0f, 2.0f, 1.0f);
    }
}

void video_output_opengl::reshape(int w, int h)
{
    // Clear
    glViewport(0, 0, w, h);
    glClear(GL_COLOR_BUFFER_BIT);

    // Compute viewport with the right aspect ratio
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

    // Setup viewport and save new size
    glViewport(vp_x, vp_y, vp_w, vp_h);
    if (!_state.fullscreen)
    {
        _win_width = w;
        _win_height = h;
    }
}

static void upload_texture(
        GLuint tex, GLuint pbo,
        GLsizei w, GLsizei h, int bytes_per_pixel, int line_size,
        GLenum fmt, GLenum type, const GLvoid *data)
{
    uintptr_t p = reinterpret_cast<uintptr_t>(data);
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

    glPixelStorei(GL_UNPACK_ROW_LENGTH, line_size / bytes_per_pixel);
    glPixelStorei(GL_UNPACK_ALIGNMENT, row_alignment);
    glBindTexture(GL_TEXTURE_2D, tex);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, h * line_size, NULL, GL_STREAM_DRAW);
    void *pboptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    std::memcpy(pboptr, data, h * line_size);
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, fmt, type, NULL);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

void video_output_opengl::prepare(
        uint8_t *l_data[3], size_t l_line_size[3],
        uint8_t *r_data[3], size_t r_line_size[3])
{
    int tex_set = (_active_tex_set == 0 ? 1 : 0);
    if (!l_data[0])
    {
        _have_valid_data[tex_set] = false;
        return;
    }
    _input_is_mono = (l_data[0] == r_data[0] && l_data[1] == r_data[1] && l_data[2] == r_data[2]);

    /* Step 1: input of video data */

    glActiveTexture(GL_TEXTURE0);
    if (frame_format() == decoder::frame_format_yuv420p)
    {
        upload_texture(_yuv420p_y_tex[tex_set][0], _pbo,
                _src_width, _src_height, 1, l_line_size[0],
                GL_LUMINANCE, GL_UNSIGNED_BYTE, l_data[0]);
        upload_texture(_yuv420p_u_tex[tex_set][0], _pbo,
                _src_width / 2, _src_height / 2, 1, l_line_size[1],
                GL_LUMINANCE, GL_UNSIGNED_BYTE, l_data[1]);
        upload_texture(_yuv420p_v_tex[tex_set][0], _pbo,
                _src_width / 2, _src_height / 2, 1, l_line_size[2],
                GL_LUMINANCE, GL_UNSIGNED_BYTE, l_data[2]);
        if (!_input_is_mono)
        {
            upload_texture(_yuv420p_y_tex[tex_set][1], _pbo,
                    _src_width, _src_height, 1, r_line_size[0],
                    GL_LUMINANCE, GL_UNSIGNED_BYTE, r_data[0]);
            upload_texture(_yuv420p_u_tex[tex_set][1], _pbo,
                    _src_width / 2, _src_height / 2, 1, r_line_size[1],
                    GL_LUMINANCE, GL_UNSIGNED_BYTE, r_data[1]);
            upload_texture(_yuv420p_v_tex[tex_set][1], _pbo,
                    _src_width / 2, _src_height / 2, 1, r_line_size[2],
                    GL_LUMINANCE, GL_UNSIGNED_BYTE, r_data[2]);
        }
    }
    else if (frame_format() == decoder::frame_format_bgra32)
    {
        upload_texture(_bgra32_tex[tex_set][0], _pbo,
                _src_width, _src_height, 4, l_line_size[0],
                GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, l_data[0]);
        if (!_input_is_mono)
        {
            upload_texture(_bgra32_tex[tex_set][1], _pbo,
                    _src_width, _src_height, 4, r_line_size[0],
                    GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, r_data[0]);
        }
    }
    _have_valid_data[tex_set] = true;
}

std::vector<std::string> glew_versions()
{
    std::vector<std::string> v;
    v.push_back(str::asprintf("GLEW %s", reinterpret_cast<const char *>(glewGetString(GLEW_VERSION))));
    return v;
}
