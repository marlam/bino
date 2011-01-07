/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010-2011
 * Martin Lambers <marlam@marlam.de>
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
    // Video properties (fixed during playback)
    int _src_format;
    bool _src_is_mono;
    int _src_width;
    int _src_height;
    float _src_aspect_ratio;
    // Screen properties (fixed during playback)
    int _screen_width;
    int _screen_height;
    float _screen_pixel_aspect_ratio;
    // Video output mode (fixed during playback)
    enum video_output::mode _mode;
    // Video output properties (variable)
    video_output_state _state;
    int _win_width;
    int _win_height;

    /* We have two texture sets for input: one holding the current video frame
     * for output, and one for preparing the next video frame. Each texture set
     * has textures for the left and right view. */
    int _active_tex_set;        // 0 or 1
    bool _have_valid_data[2];   // do we have valid data in the given texture set?
    // Step 1: input of video data
    GLuint _pbo;                // pixel-buffer object for texture uploading
    GLuint _yuv_y_tex[2][2];    // for yuv formats: y component
    GLuint _yuv_u_tex[2][2];    // for yuv formats: u component
    GLuint _yuv_v_tex[2][2];    // for yuv formats: v component
    int _yuv_chroma_width_divisor;      // for yuv formats: chroma subsampling
    int _yuv_chroma_height_divisor;     // for yuv formats: chroma subsampling
    GLuint _bgra32_tex[2][2];   // for bgra32 format
    // Step 2: color-correction
    GLuint _color_prg;          // color space transformation, color adjustment
    GLuint _color_fbo;          // framebuffer object to render into the sRGB texture
    GLuint _srgb_tex[2];        // output: sRGB texture
    // Step 3: rendering
    GLuint _render_prg;         // reads sRGB texture, renders according to _mode
    GLuint _mask_tex;           // for the masking modes even-odd-{rows,columns}, checkerboard
    // Is the GL stuff initialized?
    bool _initialized;
    // XXX: Hack: work around broken SRGB texture implementations
    bool _srgb_textures_are_broken;
    // OpenGL Viewport for drawing the video frame
    GLint _viewport[4];

protected:
    /* In a sub-class that provides an OpenGL context, call the following
     * initialization functions in the order in which they appear here.
     * You must make sure that the OpenGL context provides GL 2.1 + FBOs. */
    void set_mode(enum video_output::mode mode);
    void set_source_info(int width, int height, float aspect_ratio, int format, bool mono);
    void set_screen_info(int width, int height, float pixel_aspect_ratio);
    void compute_win_size(int width = -1, int height = -1);
    void set_state(const video_output_state &_state);
    void initialize();

    // Swap the texture sets (one active, one for preparing the next video frame)
    void swap_tex_set();
    // Clear the video area
    void clear();
    // Display the current texture set. The first version of this function is used
    // by Equalizer; simple windows will use the second version.
    void display(bool toggle_swap_eyes, float x, float y, float w, float h, const int viewport[4]);
    void display() { display(false, -1.0f, -1.0f, 2.0f, 2.0f, _viewport); }
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
            int video_format, bool mono,
            int src_width, int src_height, float src_aspect_ratio,
            int mode, const video_output_state &state, unsigned int flags,
            int win_width, int win_height) = 0;

    virtual enum mode mode() const
    {
        return _mode;
    }

    virtual const video_output_state &state() const
    {
        return _state;
    }

    virtual void *prepare_start(int view, int plane);
    virtual void prepare_finish(int view, int plane);

    virtual void activate() = 0;
    virtual void process_events() = 0;

    virtual void close() = 0;

    virtual void receive_notification(const notification &note) = 0;
};

std::vector<std::string> glew_versions();

#endif
