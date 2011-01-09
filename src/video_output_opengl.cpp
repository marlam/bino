/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010-2011
 * Martin Lambers <marlam@marlam.de>
 * Frédéric Devernay <frederic.devernay@inrialpes.fr>
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
#include "debug.h"

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
 * The input data is first converted to YUV (for the common planar YUV frame
 * formats, this just means gathering of the three components from the
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
 * The 420p and 422p chroma subsampling formats are currently handled by
 * sampling the U and V textures with bilinear interpolation at the correct
 * position according to the chroma location. Bilinear interpolation of U and V
 * is questionable since these values are not linear. However, I could not find
 * information on a better way to do this, and other players seem to use linear
 * interpolation, too.
 */


video_output_opengl::video_output_opengl(bool receive_notifications) throw ()
    : video_output(receive_notifications), _initialized(false)
{
}

video_output_opengl::~video_output_opengl()
{
}

void video_output_opengl::set_source_info(int width, int height, float aspect_ratio,
        int format, bool mono)
{
    _src_format = format;
    _src_is_mono = mono;
    _src_width = width;
    _src_height = height;
    _src_aspect_ratio = aspect_ratio;
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
    _have_valid_data[0] = false;
    _have_valid_data[1] = false;
    glGenBuffers(1, &_pbo);
    if (decoder::video_format_layout(_src_format) == decoder::video_layout_bgra32)
    {
        for (int i = 0; i < 2; i++)
        {
            for (int j = 0; j < (_src_is_mono ? 1 : 2); j++)
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
    else
    {
        _yuv_chroma_width_divisor = 1;
        _yuv_chroma_height_divisor = 1;
        bool need_chroma_filtering = false;
        if (decoder::video_format_layout(_src_format) == decoder::video_layout_yuv422p)
        {
            _yuv_chroma_width_divisor = 2;
            need_chroma_filtering = true;
        }
        else if (decoder::video_format_layout(_src_format) == decoder::video_layout_yuv420p)
        {
            _yuv_chroma_width_divisor = 2;
            _yuv_chroma_height_divisor = 2;
            need_chroma_filtering = true;
        }
        for (int i = 0; i < 2; i++)
        {
            for (int j = 0; j < (_src_is_mono ? 1 : 2); j++)
            {
                glGenTextures(1, &(_yuv_y_tex[i][j]));
                glBindTexture(GL_TEXTURE_2D, _yuv_y_tex[i][j]);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8,
                        _src_width, _src_height,
                        0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
                glGenTextures(1, &(_yuv_u_tex[i][j]));
                glBindTexture(GL_TEXTURE_2D, _yuv_u_tex[i][j]);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, need_chroma_filtering ? GL_LINEAR : GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, need_chroma_filtering ? GL_LINEAR : GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8,
                        _src_width / _yuv_chroma_width_divisor, _src_height / _yuv_chroma_height_divisor,
                        0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
                glGenTextures(1, &(_yuv_v_tex[i][j]));
                glBindTexture(GL_TEXTURE_2D, _yuv_v_tex[i][j]);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, need_chroma_filtering ? GL_LINEAR : GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, need_chroma_filtering ? GL_LINEAR : GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8,
                        _src_width / _yuv_chroma_width_divisor, _src_height / _yuv_chroma_height_divisor,
                        0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
            }
        }
    }

    // Step 2: color-correction
    std::string layout_str;
    std::string color_space_str;
    std::string value_range_str;
    std::string chroma_offset_x_str;
    std::string chroma_offset_y_str;
    if (decoder::video_format_layout(_src_format) == decoder::video_layout_bgra32)
    {
        layout_str = "layout_bgra32";
        color_space_str = "color_space_srgb";
        value_range_str = "value_range_8bit_full";
    }
    else
    {
        layout_str = "layout_yuv_p";
        if (decoder::video_format_color_space(_src_format) == decoder::video_color_space_yuv709)
        {
            color_space_str = "color_space_yuv709";
        }
        else
        {
            color_space_str = "color_space_yuv601";
        }
        if (decoder::video_format_value_range(_src_format) == decoder::video_value_range_8bit_full)
        {
            value_range_str = "value_range_8bit_full";
        }
        else
        {
            value_range_str = "value_range_8bit_mpeg";
        }
        chroma_offset_x_str = "0.0";
        chroma_offset_y_str = "0.0";
        if (decoder::video_format_layout(_src_format) == decoder::video_layout_yuv422p)
        {
            if (decoder::video_format_chroma_location(_src_format) == decoder::video_chroma_location_left)
            {
                chroma_offset_x_str = str::from(0.5f / static_cast<float>(_src_width / _yuv_chroma_width_divisor));
            }
            else if (decoder::video_format_chroma_location(_src_format) == decoder::video_chroma_location_topleft)
            {
                chroma_offset_x_str = str::from(0.5f / static_cast<float>(_src_width / _yuv_chroma_width_divisor));
                chroma_offset_y_str = str::from(0.5f / static_cast<float>(_src_height / _yuv_chroma_height_divisor));
            }
        }
        else if (decoder::video_format_layout(_src_format) == decoder::video_layout_yuv420p)
        {
            if (decoder::video_format_chroma_location(_src_format) == decoder::video_chroma_location_left)
            {
                chroma_offset_x_str = str::from(0.5f / static_cast<float>(_src_width / _yuv_chroma_width_divisor));
            }
            else if (decoder::video_format_chroma_location(_src_format) == decoder::video_chroma_location_topleft)
            {
                chroma_offset_x_str = str::from(0.5f / static_cast<float>(_src_width / _yuv_chroma_width_divisor));
                chroma_offset_y_str = str::from(0.5f / static_cast<float>(_src_height / _yuv_chroma_height_divisor));
            }
        }
    }
    std::string color_fs_src(VIDEO_OUTPUT_OPENGL_COLOR_FS_GLSL_STR);
    str::replace(color_fs_src, "$layout", layout_str);
    str::replace(color_fs_src, "$color_space", color_space_str);
    str::replace(color_fs_src, "$value_range", value_range_str);
    str::replace(color_fs_src, "$chroma_offset_x", chroma_offset_x_str);
    str::replace(color_fs_src, "$chroma_offset_y", chroma_offset_y_str);
    _color_prg = xgl::CreateProgram("video_output_color", "", "", color_fs_src);
    xgl::LinkProgram("video_output_color", _color_prg);
    glGenFramebuffersEXT(1, &_color_fbo);
    for (int j = 0; j < (_src_is_mono ? 1 : 2); j++)
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
    std::string render_fs_src(VIDEO_OUTPUT_OPENGL_RENDER_FS_GLSL_STR);
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
    str::replace(render_fs_src, "$mode", mode_str);
    str::replace(render_fs_src, "$srgb_broken", srgb_broken_str);
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
    if (decoder::video_format_layout(_src_format) == decoder::video_layout_bgra32)
    {
        glDeleteTextures(_src_is_mono ? 1 : 2, _bgra32_tex[0]);
    }
    else
    {
        glDeleteTextures(_src_is_mono ? 1 : 2, _yuv_y_tex[0]);
        glDeleteTextures(_src_is_mono ? 1 : 2, _yuv_y_tex[1]);
        glDeleteTextures(_src_is_mono ? 1 : 2, _yuv_u_tex[0]);
        glDeleteTextures(_src_is_mono ? 1 : 2, _yuv_u_tex[1]);
        glDeleteTextures(_src_is_mono ? 1 : 2, _yuv_v_tex[0]);
        glDeleteTextures(_src_is_mono ? 1 : 2, _yuv_v_tex[1]);
    }
    xgl::DeleteProgram(_color_prg);
    glDeleteFramebuffersEXT(1, &_color_fbo);
    glDeleteTextures(_src_is_mono ? 1 : 2, reinterpret_cast<GLuint *>(_srgb_tex));
    xgl::DeleteProgram(_render_prg);
    if (_mode == even_odd_rows || _mode == even_odd_columns || _mode == checkerboard)
    {
        glDeleteTextures(1, &_mask_tex);
    }
    _initialized = false;
}

void video_output_opengl::clear()
{
    if (_mode == stereo)
    {
        glDrawBuffer(GL_BACK_LEFT);
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawBuffer(GL_BACK_RIGHT);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    else
    {
        glClear(GL_COLOR_BUFFER_BIT);
    }
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

void video_output_opengl::display(bool mono_right_instead_of_left, float x, float y, float w, float h, const int viewport[4])
{
    clear();
    if (!_have_valid_data[_active_tex_set])
    {
        return;
    }

    /* Use correct left and right view indices */

    int left = 0;
    int right = (_src_is_mono ? 0 : 1);
    if (_state.swap_eyes)
    {
        std::swap(left, right);
    }
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
    if (decoder::video_format_layout(_src_format) == decoder::video_layout_bgra32)
    {
        glUniform1i(glGetUniformLocation(_color_prg, "srgb_tex"), 0);
    }
    else
    {
        glUniform1i(glGetUniformLocation(_color_prg, "y_tex"), 0);
        glUniform1i(glGetUniformLocation(_color_prg, "u_tex"), 1);
        glUniform1i(glGetUniformLocation(_color_prg, "v_tex"), 2);
    }
    glUniform1f(glGetUniformLocation(_color_prg, "contrast"), _state.contrast);
    glUniform1f(glGetUniformLocation(_color_prg, "brightness"), _state.brightness);
    glUniform1f(glGetUniformLocation(_color_prg, "saturation"), _state.saturation);
    glUniform1f(glGetUniformLocation(_color_prg, "cos_hue"), std::cos(_state.hue * M_PI));
    glUniform1f(glGetUniformLocation(_color_prg, "sin_hue"), std::sin(_state.hue * M_PI));
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, _color_fbo);
    // left view: render into _srgb_tex[0]
    if (decoder::video_format_layout(_src_format) == decoder::video_layout_bgra32)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, _bgra32_tex[_active_tex_set][left]);
    }
    else
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, _yuv_y_tex[_active_tex_set][left]);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, _yuv_u_tex[_active_tex_set][left]);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, _yuv_v_tex[_active_tex_set][left]);
    }
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,
            GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, _srgb_tex[0], 0);
    draw_quad(-1.0f, +1.0f, +2.0f, -2.0f);
    // right view: render into _srgb_tex[1]
    if (left != right)
    {
        if (decoder::video_format_layout(_src_format) == decoder::video_layout_bgra32)
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, _bgra32_tex[_active_tex_set][right]);
        }
        else
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, _yuv_y_tex[_active_tex_set][right]);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, _yuv_u_tex[_active_tex_set][right]);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, _yuv_v_tex[_active_tex_set][right]);
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
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _srgb_tex[left]);
    if (left != right)
    {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, _srgb_tex[right]);
    }
    glUniform1i(glGetUniformLocation(_render_prg, "rgb_l"), left);
    glUniform1i(glGetUniformLocation(_render_prg, "rgb_r"), right);
    glUniform1f(glGetUniformLocation(_render_prg, "parallax"), _state.parallax * 0.05f);
    if (_mode != anaglyph_red_cyan_monochrome
            && _mode != anaglyph_red_cyan_full_color
            && _mode != anaglyph_red_cyan_half_color
            && _mode != anaglyph_red_cyan_dubois)
    {
        glUniform3f(glGetUniformLocation(_render_prg, "crosstalk"),
                _state.crosstalk_r * _state.ghostbust,
                _state.crosstalk_g * _state.ghostbust,
                _state.crosstalk_b * _state.ghostbust);
    }
    if (_mode == even_odd_rows || _mode == even_odd_columns || _mode == checkerboard)
    {
        glUniform1i(glGetUniformLocation(_render_prg, "mask_tex"), 2);
        glUniform1f(glGetUniformLocation(_render_prg, "step_x"), 1.0f / static_cast<float>(viewport[2]));
        glUniform1f(glGetUniformLocation(_render_prg, "step_y"), 1.0f / static_cast<float>(viewport[3]));
    }

    if (_mode == stereo)
    {
        glUniform1f(glGetUniformLocation(_render_prg, "channel"), 0.0f);
        glDrawBuffer(GL_BACK_LEFT);
        draw_quad(x, y, w, h);
        glUniform1f(glGetUniformLocation(_render_prg, "channel"), 1.0f);
        glDrawBuffer(GL_BACK_RIGHT);
        draw_quad(x, y, w, h);
    }
    else if (_mode == even_odd_rows || _mode == even_odd_columns || _mode == checkerboard)
    {
        float vpw = static_cast<float>(viewport[2]);
        float vph = static_cast<float>(viewport[3]);
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
        draw_quad(x, y, w, h);
    }
    else if (_mode == mono_left && !mono_right_instead_of_left)
    {
        glUniform1f(glGetUniformLocation(_render_prg, "channel"), 0.0f);
        draw_quad(x, y, w, h);
    }
    else if (_mode == mono_right || (_mode == mono_left && mono_right_instead_of_left))
    {
        glUniform1f(glGetUniformLocation(_render_prg, "channel"), 1.0f);
        draw_quad(x, y, w, h);
    }
    else if (_mode == left_right || _mode == left_right_half)
    {
        glUniform1f(glGetUniformLocation(_render_prg, "channel"), 0.0f);
        draw_quad(-1.0f, -1.0f, 1.0f, 2.0f);
        glUniform1f(glGetUniformLocation(_render_prg, "channel"), 1.0f);
        draw_quad(0.0f, -1.0f, 1.0f, 2.0f);
    }
    else if (_mode == top_bottom || _mode == top_bottom_half)
    {
        glUniform1f(glGetUniformLocation(_render_prg, "channel"), 0.0f);
        draw_quad(-1.0f, 0.0f, 2.0f, 1.0f);
        glUniform1f(glGetUniformLocation(_render_prg, "channel"), 1.0f);
        draw_quad(-1.0f, -1.0f, 2.0f, 1.0f);
    }
}

void video_output_opengl::reshape(int w, int h)
{
    // Clear
    glViewport(0, 0, w, h);
    clear();

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

    // Save new size
    _viewport[0] = vp_x;
    _viewport[1] = vp_y;
    _viewport[2] = vp_w;
    _viewport[3] = vp_h;
    if (!_state.fullscreen)
    {
        _win_width = w;
        _win_height = h;
    }
}

/* Step 1: Input of video data:
 * prepare_start() and prepare_finish() for each data plane and each view
 * (for mono: only view 0). */

static int next_multiple_of_4(int x)
{
    return (x / 4 + (x % 4 == 0 ? 0 : 1)) * 4;
}

void *video_output_opengl::prepare_start(int /* view */, int plane)
{
    int w, h;
    int bytes_per_pixel;
    if (decoder::video_format_layout(_src_format) == decoder::video_layout_bgra32)
    {
        w = _src_width;
        h = _src_height;
        bytes_per_pixel = 4;
    }
    else
    {
        if (plane == 0)
        {
            w = next_multiple_of_4(_src_width);
            h = next_multiple_of_4(_src_height);
        }
        else
        {
            w = next_multiple_of_4(_src_width / _yuv_chroma_width_divisor);
            h = next_multiple_of_4(_src_height / _yuv_chroma_height_divisor);
        }
        bytes_per_pixel = 1;
    }
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, _pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, w * h * bytes_per_pixel, NULL, GL_STREAM_DRAW);
    void *pboptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    if (!pboptr)
    {
        debug::oom_abort();
    }
    if (reinterpret_cast<uintptr_t>(pboptr) % 4 != 0)
    {
        // We can assume that the buffer is always at least aligned at a 4-byte boundary.
        // This is just a sanity check; this error should never be triggered.
        msg::err("pixel buffer alignment is less than 4!");
        debug::crash();
    }
    return pboptr;
}

void video_output_opengl::prepare_finish(int view, int plane)
{
    int tex_set = (_active_tex_set == 0 ? 1 : 0);
    int w, h;
    GLenum format;
    GLenum type;
    GLuint tex;
    if (decoder::video_format_layout(_src_format) == decoder::video_layout_bgra32)
    {
        w = _src_width;
        h = _src_height;
        format = GL_BGRA;
        type = GL_UNSIGNED_INT_8_8_8_8_REV;
        tex = _bgra32_tex[tex_set][view];
    }
    else
    {
        if (plane == 0)
        {
            w = _src_width;
            h = _src_height;
            tex = _yuv_y_tex[tex_set][view];
        }
        else
        {
            w = _src_width / _yuv_chroma_width_divisor;
            h = _src_height / _yuv_chroma_height_divisor;
            tex = (plane == 1 ? _yuv_u_tex[tex_set][view] : _yuv_v_tex[tex_set][view]);
        }
        format = GL_LUMINANCE;
        type = GL_UNSIGNED_BYTE;
    }

    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, w);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, format, type, NULL);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    _have_valid_data[tex_set] = true;
}

std::vector<std::string> glew_versions()
{
    std::vector<std::string> v;
    v.push_back(str::asprintf("GLEW %s", reinterpret_cast<const char *>(glewGetString(GLEW_VERSION))));
    return v;
}
