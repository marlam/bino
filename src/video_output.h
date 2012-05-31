/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010, 2011, 2012
 * Martin Lambers <marlam@marlam.de>
 * Frédéric Devernay <Frederic.Devernay@inrialpes.fr>
 * Joe <cuchac@email.cz>
 * Binocle <http://binocle.com> (author: Olivier Letz <oletz@binocle.com>)
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

#ifndef VIDEO_OUTPUT_H
#define VIDEO_OUTPUT_H

#include <vector>
#include <string>

#include <GL/glew.h>

#include "blob.h"

#include "media_data.h"
#include "subtitle_renderer.h"
#include "dispatch.h"


class subtitle_updater;
#if HAVE_LIBXNVCTRL
class CNvSDIout;
#endif // HAVE_LIBXNVCTRL

class video_output : public controller
{
private:
    bool _initialized;

    /* We manage two frames, each with its own set of properties etc.
     * The active frame is the one that is displayed, the other frame is the one
     * that is prepared for display.
     * Each frame contains a left view and may contain a right view. */
    int _active_index;                                  // 0 or 1

    video_frame _frame[2];              // input frames (active / preparing)
    // Step 1: input of video data
    video_frame _input_last_frame;      // last frame for this step
    GLuint _input_pbo;                  // pixel-buffer object for texture uploading
    GLuint _input_fbo;                  // frame-buffer object for texture clearing
    GLuint _input_yuv_y_tex[2];         // for yuv formats: y component
    GLuint _input_yuv_u_tex[2];         // for yuv formats: u component
    GLuint _input_yuv_v_tex[2];         // for yuv formats: v component
    GLuint _input_bgra32_tex[2];        // for bgra32 format
    int _input_yuv_chroma_width_divisor;        // for yuv formats: chroma subsampling
    int _input_yuv_chroma_height_divisor;       // for yuv formats: chroma subsampling
    subtitle_box _subtitle[2];          // the current subtitle box
    GLuint _subtitle_tex[2];            // subtitle texture
    bool _subtitle_tex_current[2];      // whether the subtitle tex contains the current subtitle buffer
    // Step 2: color space conversion and color correction
    parameters _color_last_params[2];   // last params for this step; used for reinitialization check
    video_frame _color_last_frame[2];   // last frame for this step; used for reinitialization check
    GLuint _color_prg[2];               // color space transformation, color adjustment
    GLuint _color_fbo;                  // framebuffer object to render into the sRGB texture
    GLuint _color_tex[2][2];            // output: SRGB8 or linear RGB16 texture
    // Step 3: rendering
    parameters _render_params;          // current parameters for display
    parameters _render_last_params;     // last params for this step; used for reinitialization check
    video_frame _render_last_frame;     // last frame for this step; used for reinitialization check
    GLuint _render_prg;                 // reads sRGB texture, renders according to _params[_active_index]
    GLuint _render_dummy_tex;           // an empty subtitle texture
    GLuint _render_mask_tex;            // for the masking modes even-odd-{rows,columns}, checkerboard
    blob _3d_ready_sync_buf;            // for 3-D Ready Sync pixels
    // OpenGL viewports and tex coordinates for drawing the two views of the video frame
    GLint _full_viewport[4];
    GLint _viewport[2][4];
    float _tex_coords[2][4][2];

    subtitle_updater *_subtitle_updater;        // the subtitle updater thread
#if HAVE_LIBXNVCTRL
    CNvSDIout *_nv_sdi_output;          // access the nvidia quadro sdi output card
    int64_t _last_nv_sdi_displayed_frameno;
#endif // HAVE_LIBXNVCTRL

    // GL Helper functions
    bool xglCheckError(const std::string& where = std::string()) const;
    bool xglCheckFBO(const std::string& where = std::string()) const;
    GLuint xglCompileShader(const std::string& name, GLenum type, const std::string& src) const;
    GLuint xglCreateProgram(GLuint vshader, GLuint fshader) const;
    GLuint xglCreateProgram(const std::string& name,
            const std::string& vshader_src, const std::string& fshader_src) const;
    void xglLinkProgram(const std::string& name, const GLuint prg) const;
    void xglDeleteProgram(GLuint prg) const;

    bool srgb8_textures_are_color_renderable();

    void draw_quad(float x, float y, float w, float h,
            const float tex_coords[2][4][2] = NULL,
            const float more_tex_coords[4][2] = NULL) const;

    // Step 1: initialize/deinitialize, and check if reinitialization is necessary
    void input_init(const video_frame &frame);
    void input_deinit();
    bool input_is_compatible(const video_frame &current_frame);
    void subtitle_init(int index);
    void subtitle_deinit(int index);
    // Step 2: initialize/deinitialize, and check if reinitialization is necessary
    void color_init(int index, const parameters& params, const video_frame &frame);
    void color_deinit(int index);
    bool color_is_compatible(int index, const parameters& params, const video_frame &current_frame);
    // Step 3: initialize/deinitialize, and check if reinitialization is necessary
    void render_init();
    void render_deinit();
    bool render_needs_subtitle(const parameters& params);
    bool render_needs_coloradjust(const parameters& params);
    bool render_needs_ghostbust(const parameters& params);
    bool render_is_compatible();

protected:
    subtitle_renderer _subtitle_renderer;

    virtual GLEWContext* glewGetContext() const = 0;

    // Get the total viewport size.
    int full_display_width() const;
    int full_display_height() const;
    // Get size of the viewport area that is used for video. This is overridable for Equalizer.
    virtual int video_display_width() const;
    virtual int video_display_height() const;

    virtual bool context_is_stereo() const = 0; // Is our current OpenGL context a stereo context?
    virtual void recreate_context(bool stereo) = 0;     // Recreate an OpenGL context and make it current
    virtual void trigger_resize(int w, int h) = 0;      // Trigger a resize the video area

    void clear() const;                         // Clear the video area
    void reshape(int w, int h, const parameters& params = dispatch::parameters());       // Call this when the video area was resized

    /* Get screen properties (fixed) */
    virtual int screen_width() const = 0;       // in pixels
    virtual int screen_height() const = 0;      // in pixels
    virtual float screen_pixel_aspect_ratio() const = 0;// the aspect ratio of a pixel on screen

    /* Get current video area properties */
    virtual int width() const = 0;              // in pixels
    virtual int height() const = 0;             // in pixels
    virtual int pos_x() const = 0;              // in pixels
    virtual int pos_y() const = 0;              // in pixels

    /* Display the current frame.
     * The first version is used by Equalizer, which needs to set some special properties.
     * The second version is used by NVIDIA SDI output.
     * The third version is for everyone else.
     * TODO: This function needs to handle interlaced frames! */
    void display_current_frame(int64_t display_frameno, bool keep_viewport, bool mono_right_instead_of_left,
            float x, float y, float w, float h,
            const GLint viewport[2][4], const float tex_coords[2][4][2],
            int dst_width, int dst_height,
            parameters::stereo_mode_t stereo_mode);
    void display_current_frame(int64_t display_frameno, int dst_width, int dst_height, parameters::stereo_mode_t stereo_mode)
    {
        display_current_frame(display_frameno, false, false, -1.0f, -1.0f, 2.0f, 2.0f,
                _viewport, _tex_coords, dst_width, dst_height, stereo_mode);
    }
    void display_current_frame(int64_t display_frameno = 0)
    {
        display_current_frame(display_frameno, false, false, -1.0f, -1.0f, 2.0f, 2.0f,
                _viewport, _tex_coords, full_display_width(), full_display_height(),
                dispatch::parameters().stereo_mode());
    }

#if HAVE_LIBXNVCTRL
    void sdi_output(int64_t display_frameno = 0);
#endif // HAVE_LIBXNVCTRL

public:
    /* Constructor, Destructor */
    video_output();
    virtual ~video_output();

    /* Initialize the video output, or throw an exception */
    virtual void init();
    /* Wait for subtitle renderer initialization to finish. Has to be called
     * if the video has subtitles. Returns the number of microseconds that the
     * waiting took. */
    virtual int64_t wait_for_subtitle_renderer() = 0;
    /* Deinitialize the video output */
    virtual void deinit();

    /* Set a video area size suitable for the given input/output settings */
    void set_suitable_size(int w, int h, float ar, parameters::stereo_mode_t stereo_mode);

    /* Get capabilities */
    virtual bool supports_stereo() const = 0;           // Is OpenGL quad buffered stereo available?

    /* Center video area on screen */
    virtual void center() = 0;

    /* Enter/exit fullscreen mode */
    virtual void enter_fullscreen() = 0;
    virtual void exit_fullscreen() = 0;

    /* Process window system events (if applicable) */
    virtual void process_events() = 0;
    
    /* Prepare a new frame for display. */
    virtual void prepare_next_frame(const video_frame &frame, const subtitle_box &subtitle);
    /* Switch to the next frame (make it the current one) */
    virtual void activate_next_frame();
    /* Get an estimation of when the next frame will appear on screen */
    virtual int64_t time_to_next_frame_presentation() const;
};

#endif
