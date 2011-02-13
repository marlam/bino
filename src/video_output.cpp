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

#include <cstdlib>
#include <cmath>

#include <GL/glew.h>

#include "exc.h"
#include "msg.h"
#include "str.h"
#include "timer.h"
#include "dbg.h"

#include "video_output.h"
#include "video_output_color.fs.glsl.h"
#include "video_output_render.fs.glsl.h"
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

video_output::video_output(bool receive_notifications) :
    controller(receive_notifications),
    _initialized(false)
{
    // XXX: Hack: work around broken SRGB texture implementations
    _srgb_textures_are_broken = std::getenv("SRGB_TEXTURES_ARE_BROKEN");

    _input_pbo = 0;
    _active_index = 1;
    for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            _input_yuv_y_tex[i][j] = 0;
            _input_yuv_u_tex[i][j] = 0;
            _input_yuv_v_tex[i][j] = 0;
            _input_bgra32_tex[i][j] = 0;
        }
        _color_srgb_tex[i] = 0;
    }
    _color_prg = 0;
    _color_fbo = 0;
    _render_prg = 0;
    _render_mask_tex = 0;
}

video_output::~video_output()
{
}

void video_output::init()
{
    if (!_initialized)
    {
        /* currently nothing to do */
        _initialized = true;
    }
}

void video_output::deinit()
{
    if (_initialized)
    {
        make_context_current();
        assert(xgl::CheckError(HERE));
        input_deinit(0);
        input_deinit(1);
        color_deinit();
        render_deinit();
        assert(xgl::CheckError(HERE));
        _initialized = false;
    }
}

float video_output::screen_pixel_aspect_ratio()
{
    float screen_pixel_ar = screen_aspect_ratio()
        * static_cast<float>(screen_height()) / static_cast<float>(screen_width());
    if (std::fabs(screen_pixel_ar - 1.0f) < 0.03f)
    {
        // This screen most probably has square pixels, and the difference to 1.0
        // is only due to slightly inaccurate measurements and rounding. Force
        // 1.0 so that the user gets expected results.
        screen_pixel_ar = 1.0f;
    }
    return screen_pixel_ar;
}

void video_output::set_suitable_size(int w, int h, float ar, parameters::stereo_mode_t stereo_mode)
{
    int width = w;
    int height = h;
    if (stereo_mode == parameters::left_right)
    {
        w *= 2;
    }
    else if (stereo_mode == parameters::top_bottom)
    {
        h *= 2;
    }
    float aspect_ratio = width * screen_pixel_aspect_ratio() / height;
    if (stereo_mode == parameters::left_right)
    {
        aspect_ratio /= 2.0f;
    }
    else if (stereo_mode == parameters::top_bottom)
    {
        aspect_ratio *= 2.0f;
    }
    if (ar > aspect_ratio)
    {
        width *= ar / aspect_ratio;
    }
    else
    {
        height *= aspect_ratio / ar;
    }
    int max_width = screen_width() - screen_width() / 20;
    if (width > max_width)
    {
        width = max_width;
    }
    int max_height = screen_height() - screen_height() / 20;
    if (height > max_height)
    {
        height = max_height;
    }
    trigger_resize(width, height);
}

void video_output::input_init(int index, const video_frame &frame)
{
    assert(xgl::CheckError(HERE));
    glGenBuffers(1, &_input_pbo);
    if (frame.layout == video_frame::bgra32)
    {
        for (int i = 0; i < (frame.stereo_layout == video_frame::mono ? 1 : 2); i++)
        {
            glGenTextures(1, &(_input_bgra32_tex[index][i]));
            glBindTexture(GL_TEXTURE_2D, _input_bgra32_tex[index][i]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, frame.width, frame.height,
                    0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
        }
    }
    else
    {
        _input_yuv_chroma_width_divisor[index] = 1;
        _input_yuv_chroma_height_divisor[index] = 1;
        bool need_chroma_filtering = false;
        if (frame.layout == video_frame::yuv422p)
        {
            _input_yuv_chroma_width_divisor[index] = 2;
            need_chroma_filtering = true;
        }
        else if (frame.layout == video_frame::yuv420p)
        {
            _input_yuv_chroma_width_divisor[index] = 2;
            _input_yuv_chroma_height_divisor[index] = 2;
            need_chroma_filtering = true;
        }
        for (int i = 0; i < (frame.stereo_layout == video_frame::mono ? 1 : 2); i++)
        {
            glGenTextures(1, &(_input_yuv_y_tex[index][i]));
            glBindTexture(GL_TEXTURE_2D, _input_yuv_y_tex[index][i]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8,
                    frame.width,
                    frame.height,
                    0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
            glGenTextures(1, &(_input_yuv_u_tex[index][i]));
            glBindTexture(GL_TEXTURE_2D, _input_yuv_u_tex[index][i]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, need_chroma_filtering ? GL_LINEAR : GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, need_chroma_filtering ? GL_LINEAR : GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8,
                    frame.width / _input_yuv_chroma_width_divisor[index],
                    frame.height / _input_yuv_chroma_height_divisor[index],
                    0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
            glGenTextures(1, &(_input_yuv_v_tex[index][i]));
            glBindTexture(GL_TEXTURE_2D, _input_yuv_v_tex[index][i]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, need_chroma_filtering ? GL_LINEAR : GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, need_chroma_filtering ? GL_LINEAR : GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8,
                    frame.width / _input_yuv_chroma_width_divisor[index],
                    frame.height / _input_yuv_chroma_height_divisor[index],
                    0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
        }
    }
    assert(xgl::CheckError(HERE));
}

bool video_output::input_is_compatible(int index, const video_frame &current_frame)
{
    return (_frame[index].width == current_frame.width
            && _frame[index].height == current_frame.height
            && _frame[index].layout == current_frame.layout
            && _frame[index].color_space == current_frame.color_space
            && _frame[index].value_range == current_frame.value_range
            && _frame[index].chroma_location == current_frame.chroma_location
            && _frame[index].stereo_layout == current_frame.stereo_layout);
}

void video_output::input_deinit(int index)
{
    assert(xgl::CheckError(HERE));
    glDeleteBuffers(1, &_input_pbo);
    _input_pbo = 0;
    for (int i = 0; i < 2; i++)
    {
        if (_input_yuv_y_tex[index][i] != 0)
        {
            glDeleteTextures(1, &(_input_yuv_y_tex[index][i]));
            _input_yuv_y_tex[index][i] = 0;
        }
        if (_input_yuv_u_tex[index][i] != 0)
        {
            glDeleteTextures(1, &(_input_yuv_u_tex[index][i]));
            _input_yuv_u_tex[index][i] = 0;
        }
        if (_input_yuv_v_tex[index][i] != 0)
        {
            glDeleteTextures(1, &(_input_yuv_v_tex[index][i]));
            _input_yuv_v_tex[index][i] = 0;
        }
        if (_input_bgra32_tex[index][i] != 0)
        {
            glDeleteTextures(1, &(_input_bgra32_tex[index][i]));
            _input_bgra32_tex[index][i] = 0;
        }
    }
    _input_yuv_chroma_width_divisor[index] = 0;
    _input_yuv_chroma_height_divisor[index] = 0;
    _frame[index] = video_frame();
    assert(xgl::CheckError(HERE));
}

static int next_multiple_of_4(int x)
{
    return (x / 4 + (x % 4 == 0 ? 0 : 1)) * 4;
}

void video_output::prepare_next_frame(const video_frame &frame)
{
    make_context_current();
    assert(xgl::CheckError(HERE));
    int index = (_active_index == 0 ? 1 : 0);
    if (!input_is_compatible(index, frame))
    {
        input_deinit(index);
        input_init(index, frame);
        _frame[index] = frame;
    }
    int bytes_per_pixel = (frame.layout == video_frame::bgra32 ? 4 : 1);
    GLenum format = (frame.layout == video_frame::bgra32 ? GL_BGRA : GL_LUMINANCE);
    GLenum type = (frame.layout == video_frame::bgra32 ? GL_UNSIGNED_INT_8_8_8_8_REV : GL_UNSIGNED_BYTE);
    for (int i = 0; i < (frame.stereo_layout == video_frame::mono ? 1 : 2); i++)
    {
        for (int plane = 0; plane < (frame.layout == video_frame::bgra32 ? 1 : 3); plane++)
        {
            // Determine the texture and the dimensions
            int w = frame.width;
            int h = frame.height;
            GLuint tex;
            int row_size;
            if (frame.layout == video_frame::bgra32)
            {
                tex = _input_bgra32_tex[index][i];
            }
            else
            {
                if (plane != 0)
                {
                    w /= _input_yuv_chroma_width_divisor[index];
                    h /= _input_yuv_chroma_height_divisor[index];
                }
                tex = (plane == 0 ? _input_yuv_y_tex[index][i]
                        : plane == 1 ? _input_yuv_u_tex[index][i]
                        : _input_yuv_v_tex[index][i]);
            }
            row_size = next_multiple_of_4(w * bytes_per_pixel);
            // Get a pixel buffer object buffer for the data
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, _input_pbo);
            glBufferData(GL_PIXEL_UNPACK_BUFFER, row_size * h, NULL, GL_STREAM_DRAW);
            void *pboptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
            if (!pboptr)
            {
                msg::err("Cannot create a PBO buffer.");
                abort();
            }
            assert(reinterpret_cast<uintptr_t>(pboptr) % 4 == 0);
            // Get the plane data into the pbo
            frame.copy_plane(i, plane, pboptr);
            // Upload the data to the texture. We need to set GL_UNPACK_ROW_LENGTH for
            // misbehaving OpenGL implementations that do not seem to honor
            // GL_UNPACK_ALIGNMENT correctly in all cases (reported for Mac).
            glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, row_size / bytes_per_pixel);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, format, type, NULL);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        }
    }
    assert(xgl::CheckError(HERE));
}

void video_output::color_init(const video_frame &frame)
{
    assert(xgl::CheckError(HERE));
    glGenFramebuffersEXT(1, &_color_fbo);
    std::string layout_str;
    std::string color_space_str;
    std::string value_range_str;
    std::string chroma_offset_x_str;
    std::string chroma_offset_y_str;
    if (frame.layout == video_frame::bgra32)
    {
        layout_str = "layout_bgra32";
        color_space_str = "color_space_srgb";
        value_range_str = "value_range_8bit_full";
    }
    else
    {
        layout_str = "layout_yuv_p";
        if (frame.color_space == video_frame::yuv709)
        {
            color_space_str = "color_space_yuv709";
        }
        else
        {
            color_space_str = "color_space_yuv601";
        }
        if (frame.value_range == video_frame::u8_full)
        {
            value_range_str = "value_range_8bit_full";
        }
        else
        {
            value_range_str = "value_range_8bit_mpeg";
        }
        chroma_offset_x_str = "0.0";
        chroma_offset_y_str = "0.0";
        if (frame.layout == video_frame::yuv422p)
        {
            if (frame.chroma_location == video_frame::left)
            {
                chroma_offset_x_str = str::from(0.5f / static_cast<float>(frame.width
                            / _input_yuv_chroma_width_divisor[_active_index]));
            }
            else if (frame.chroma_location == video_frame::topleft)
            {
                chroma_offset_x_str = str::from(0.5f / static_cast<float>(frame.width
                            / _input_yuv_chroma_width_divisor[_active_index]));
                chroma_offset_y_str = str::from(0.5f / static_cast<float>(frame.height
                            / _input_yuv_chroma_height_divisor[_active_index]));
            }
        }
        else if (frame.layout == video_frame::yuv420p)
        {
            if (frame.chroma_location == video_frame::left)
            {
                chroma_offset_x_str = str::from(0.5f / static_cast<float>(frame.width
                            / _input_yuv_chroma_width_divisor[_active_index]));
            }
            else if (frame.chroma_location == video_frame::topleft)
            {
                chroma_offset_x_str = str::from(0.5f / static_cast<float>(frame.width
                            / _input_yuv_chroma_width_divisor[_active_index]));
                chroma_offset_y_str = str::from(0.5f / static_cast<float>(frame.height
                            / _input_yuv_chroma_height_divisor[_active_index]));
            }
        }
    }
    std::string color_fs_src(VIDEO_OUTPUT_COLOR_FS_GLSL_STR);
    str::replace(color_fs_src, "$layout", layout_str);
    str::replace(color_fs_src, "$color_space", color_space_str);
    str::replace(color_fs_src, "$value_range", value_range_str);
    str::replace(color_fs_src, "$chroma_offset_x", chroma_offset_x_str);
    str::replace(color_fs_src, "$chroma_offset_y", chroma_offset_y_str);
    _color_prg = xgl::CreateProgram("video_output_color", "", "", color_fs_src);
    xgl::LinkProgram("video_output_color", _color_prg);
    for (int i = 0; i < (frame.stereo_layout == video_frame::mono ? 1 : 2); i++)
    {
        glGenTextures(1, &(_color_srgb_tex[i]));
        glBindTexture(GL_TEXTURE_2D, _color_srgb_tex[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0,
                _srgb_textures_are_broken ? GL_RGB8 : GL_SRGB8,
                frame.width, frame.height, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
    }
    assert(xgl::CheckError(HERE));
}

void video_output::color_deinit()
{
    assert(xgl::CheckError(HERE));
    glDeleteFramebuffersEXT(1, &_color_fbo);
    _color_fbo = 0;
    if (_color_prg != 0)
    {
        xgl::DeleteProgram(_color_prg);
        _color_prg = 0;
    }
    for (int i = 0; i < 2; i++)
    {
        if (_color_srgb_tex[i] != 0)
        {
            glDeleteTextures(1, &(_color_srgb_tex[i]));
            _color_srgb_tex[i] = 0;
        }
    }
    _color_last_frame = video_frame();
    assert(xgl::CheckError(HERE));
}

bool video_output::color_is_compatible(const video_frame &current_frame)
{
    return (_color_last_frame.width == current_frame.width
            && _color_last_frame.height == current_frame.height
            && _color_last_frame.layout == current_frame.layout
            && _color_last_frame.color_space == current_frame.color_space
            && _color_last_frame.value_range == current_frame.value_range
            && _color_last_frame.chroma_location == current_frame.chroma_location
            && _color_last_frame.stereo_layout == current_frame.stereo_layout);
}

void video_output::render_init()
{
    assert(xgl::CheckError(HERE));
    std::string mode_str = (
            _params.stereo_mode == parameters::even_odd_rows ? "mode_even_odd_rows"
            : _params.stereo_mode == parameters::even_odd_columns ? "mode_even_odd_columns"
            : _params.stereo_mode == parameters::checkerboard ? "mode_checkerboard"
            : _params.stereo_mode == parameters::anaglyph_red_cyan_monochrome ? "mode_anaglyph_monochrome"
            : _params.stereo_mode == parameters::anaglyph_red_cyan_full_color ? "mode_anaglyph_full_color"
            : _params.stereo_mode == parameters::anaglyph_red_cyan_half_color ? "mode_anaglyph_half_color"
            : _params.stereo_mode == parameters::anaglyph_red_cyan_dubois ? "mode_anaglyph_dubois"
            : "mode_onechannel");
    std::string srgb_broken_str = (_srgb_textures_are_broken ? "1" : "0");
    std::string render_fs_src(VIDEO_OUTPUT_RENDER_FS_GLSL_STR);
    str::replace(render_fs_src, "$mode", mode_str);
    str::replace(render_fs_src, "$srgb_broken", srgb_broken_str);
    _render_prg = xgl::CreateProgram("video_output_render", "", "", render_fs_src);
    xgl::LinkProgram("video_output_render", _render_prg);
    if (_params.stereo_mode == parameters::even_odd_rows
            || _params.stereo_mode == parameters::even_odd_columns
            || _params.stereo_mode == parameters::checkerboard)
    {
        GLubyte even_odd_rows_mask[4] = { 0xff, 0xff, 0x00, 0x00 };
        GLubyte even_odd_columns_mask[4] = { 0xff, 0x00, 0xff, 0x00 };
        GLubyte checkerboard_mask[4] = { 0xff, 0x00, 0x00, 0xff };
        glGenTextures(1, &_render_mask_tex);
        glBindTexture(GL_TEXTURE_2D, _render_mask_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8, 2, 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE,
                _params.stereo_mode == parameters::even_odd_rows ? even_odd_rows_mask
                : _params.stereo_mode == parameters::even_odd_columns ? even_odd_columns_mask
                : checkerboard_mask);
    }
    assert(xgl::CheckError(HERE));
}

void video_output::render_deinit()
{
    assert(xgl::CheckError(HERE));
    if (_render_prg != 0)
    {
        xgl::DeleteProgram(_render_prg);
        _render_prg = 0;
    }
    if (_render_mask_tex != 0)
    {
        glDeleteTextures(1, &_render_mask_tex);
        _render_mask_tex = 0;
    }
    _render_last_params = parameters();
    assert(xgl::CheckError(HERE));
}

bool video_output::render_is_compatible()
{
    return (_render_last_params.stereo_mode == _params.stereo_mode);
}

void video_output::activate_next_frame()
{
    _active_index = (_active_index == 0 ? 1 : 0);
    trigger_update();
}

void video_output::set_parameters(const parameters &params)
{
    _params = params;
    trigger_update();
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

void video_output::display_current_frame(bool mono_right_instead_of_left,
            float x, float y, float w, float h, const GLint viewport[4])
{
    make_context_current();
    assert(xgl::CheckError(HERE));
    const video_frame &frame = _frame[_active_index];
    if (!frame.is_valid())
    {
        clear();
        return;
    }

    bool need_stereo_context = (_params.stereo_mode == parameters::stereo);
    if (need_stereo_context != context_is_stereo())
    {
        deinit();
        recreate_context(need_stereo_context);
        init();
    }
    if (frame.width != _color_last_frame.width
            || frame.height != _color_last_frame.height
            || frame.aspect_ratio < _color_last_frame.aspect_ratio
            || frame.aspect_ratio > _color_last_frame.aspect_ratio
            || _render_last_params.stereo_mode != _params.stereo_mode)
    {
        reshape(width(), height());
    }
    if (!_color_prg || !color_is_compatible(frame))
    {
        color_deinit();
        color_init(frame);
        _color_last_frame = frame;
    }
    if (!_render_prg || !render_is_compatible())
    {                
        render_deinit();
        render_init();
        _render_last_params = _params;
    }

    /* Use correct left and right view indices */

    int left = 0;
    int right = (frame.stereo_layout == video_frame::mono ? 0 : 1);
    if (_params.stereo_mode_swap)
    {
        std::swap(left, right);
    }
    if ((_params.stereo_mode == parameters::even_odd_rows
                || _params.stereo_mode == parameters::checkerboard)
            && (pos_y() + viewport[1]) % 2 == 0)
    {
        std::swap(left, right);
    }
    if ((_params.stereo_mode == parameters::even_odd_columns
                || _params.stereo_mode == parameters::checkerboard)
            && (pos_x() + viewport[0]) % 2 == 1)
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
    glViewport(0, 0, frame.width, frame.height);
    glUseProgram(_color_prg);
    if (frame.layout == video_frame::bgra32)
    {
        glUniform1i(glGetUniformLocation(_color_prg, "srgb_tex"), 0);
    }
    else
    {
        glUniform1i(glGetUniformLocation(_color_prg, "y_tex"), 0);
        glUniform1i(glGetUniformLocation(_color_prg, "u_tex"), 1);
        glUniform1i(glGetUniformLocation(_color_prg, "v_tex"), 2);
    }
    glUniform1f(glGetUniformLocation(_color_prg, "contrast"), _params.contrast);
    glUniform1f(glGetUniformLocation(_color_prg, "brightness"), _params.brightness);
    glUniform1f(glGetUniformLocation(_color_prg, "saturation"), _params.saturation);
    glUniform1f(glGetUniformLocation(_color_prg, "cos_hue"), std::cos(_params.hue * M_PI));
    glUniform1f(glGetUniformLocation(_color_prg, "sin_hue"), std::sin(_params.hue * M_PI));
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, _color_fbo);
    // left view: render into _color_srgb_tex[0]
    if (frame.layout == video_frame::bgra32)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, _input_bgra32_tex[_active_index][left]);
    }
    else
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, _input_yuv_y_tex[_active_index][left]);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, _input_yuv_u_tex[_active_index][left]);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, _input_yuv_v_tex[_active_index][left]);
    }
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,
            GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, _color_srgb_tex[0], 0);
    draw_quad(-1.0f, +1.0f, +2.0f, -2.0f);
    // right view: render into _color_srgb_tex[1]
    if (left != right)
    {
        if (frame.layout == video_frame::bgra32)
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, _input_bgra32_tex[_active_index][right]);
        }
        else
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, _input_yuv_y_tex[_active_index][right]);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, _input_yuv_u_tex[_active_index][right]);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, _input_yuv_v_tex[_active_index][right]);
        }
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,
                GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, _color_srgb_tex[1], 0);
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

    // at this point, the left view is in _color_srgb_tex[0],
    // and the right view (if it exists) is in _color_srgb_tex[1]
    right = (left != right ? 1 : 0);
    left = 0;

    // Step 3: rendering
    glUseProgram(_render_prg);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _color_srgb_tex[left]);
    if (left != right)
    {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, _color_srgb_tex[right]);
    }
    glUniform1i(glGetUniformLocation(_render_prg, "rgb_l"), left);
    glUniform1i(glGetUniformLocation(_render_prg, "rgb_r"), right);
    glUniform1f(glGetUniformLocation(_render_prg, "parallax"), _params.parallax * 0.05f);
    if (_params.stereo_mode != parameters::anaglyph_red_cyan_monochrome
            && _params.stereo_mode != parameters::anaglyph_red_cyan_full_color
            && _params.stereo_mode != parameters::anaglyph_red_cyan_half_color
            && _params.stereo_mode != parameters::anaglyph_red_cyan_dubois)
    {
        glUniform3f(glGetUniformLocation(_render_prg, "crosstalk"),
                _params.crosstalk_r * _params.ghostbust,
                _params.crosstalk_g * _params.ghostbust,
                _params.crosstalk_b * _params.ghostbust);
    }
    if (_params.stereo_mode == parameters::even_odd_rows
            || _params.stereo_mode == parameters::even_odd_columns
            || _params.stereo_mode == parameters::checkerboard)
    {
        glUniform1i(glGetUniformLocation(_render_prg, "mask_tex"), 2);
        glUniform1f(glGetUniformLocation(_render_prg, "step_x"), 1.0f / static_cast<float>(viewport[2]));
        glUniform1f(glGetUniformLocation(_render_prg, "step_y"), 1.0f / static_cast<float>(viewport[3]));
    }

    if (_params.stereo_mode == parameters::stereo)
    {
        glUniform1f(glGetUniformLocation(_render_prg, "channel"), 0.0f);
        glDrawBuffer(GL_BACK_LEFT);
        draw_quad(x, y, w, h);
        glUniform1f(glGetUniformLocation(_render_prg, "channel"), 1.0f);
        glDrawBuffer(GL_BACK_RIGHT);
        draw_quad(x, y, w, h);
    }
    else if (_params.stereo_mode == parameters::even_odd_rows
            || _params.stereo_mode == parameters::even_odd_columns
            || _params.stereo_mode == parameters::checkerboard)
    {
        float vpw = static_cast<float>(viewport[2]);
        float vph = static_cast<float>(viewport[3]);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, _render_mask_tex);
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
    else if (_params.stereo_mode == parameters::anaglyph_red_cyan_monochrome
            || _params.stereo_mode == parameters::anaglyph_red_cyan_full_color
            || _params.stereo_mode == parameters::anaglyph_red_cyan_half_color
            || _params.stereo_mode == parameters::anaglyph_red_cyan_dubois)
    {
        draw_quad(x, y, w, h);
    }
    else if (_params.stereo_mode == parameters::mono_left
            && !mono_right_instead_of_left)
    {
        glUniform1f(glGetUniformLocation(_render_prg, "channel"), 0.0f);
        draw_quad(x, y, w, h);
    }
    else if (_params.stereo_mode == parameters::mono_right
            || (_params.stereo_mode == parameters::mono_left && mono_right_instead_of_left))
    {
        glUniform1f(glGetUniformLocation(_render_prg, "channel"), 1.0f);
        draw_quad(x, y, w, h);
    }
    else if (_params.stereo_mode == parameters::left_right
            || _params.stereo_mode == parameters::left_right_half)
    {
        glUniform1f(glGetUniformLocation(_render_prg, "channel"), 0.0f);
        draw_quad(-1.0f, -1.0f, 1.0f, 2.0f);
        glUniform1f(glGetUniformLocation(_render_prg, "channel"), 1.0f);
        draw_quad(0.0f, -1.0f, 1.0f, 2.0f);
    }
    else if (_params.stereo_mode == parameters::top_bottom
            || _params.stereo_mode == parameters::top_bottom_half)
    {
        glUniform1f(glGetUniformLocation(_render_prg, "channel"), 0.0f);
        draw_quad(-1.0f, 0.0f, 2.0f, 1.0f);
        glUniform1f(glGetUniformLocation(_render_prg, "channel"), 1.0f);
        draw_quad(-1.0f, -1.0f, 2.0f, 1.0f);
    }
    assert(xgl::CheckError(HERE));
}

void video_output::clear()
{
    make_context_current();
    assert(xgl::CheckError(HERE));
    if (context_is_stereo())
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
    assert(xgl::CheckError(HERE));
}

void video_output::reshape(int w, int h)
{
    make_context_current();
    // Clear
    _viewport[0] = 0;
    _viewport[1] = 0;
    _viewport[2] = w;
    _viewport[3] = h;
    glViewport(0, 0, w, h);
    clear();
    if (!_frame[_active_index].is_valid())
    {
        return;
    }

    // Compute viewport with the right aspect ratio
    float dst_w = w;
    float dst_h = h;
    float dst_ar = dst_w * screen_pixel_aspect_ratio() / dst_h;
    float src_ar = _frame[_active_index].aspect_ratio;
    if (_params.stereo_mode == parameters::left_right)
    {
        src_ar *= 2.0f;
    }
    else if (_params.stereo_mode == parameters::top_bottom)
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
}

bool video_output::need_redisplay_on_move()
{
    // The masking modes must know if the video area starts with an even or
    // odd columns and/or row. If this changes, the display must be updated.
    return (_frame[_active_index].is_valid()
            && (_render_last_params.stereo_mode == parameters::even_odd_rows
                || _render_last_params.stereo_mode == parameters::even_odd_columns
                || _render_last_params.stereo_mode == parameters::checkerboard));
}
