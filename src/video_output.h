/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010-2011
 * Martin Lambers <marlam@marlam.de>
 * Frédéric Devernay <Frederic.Devernay@inrialpes.fr>
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

#include "media_data.h"
#include "controller.h"


class video_output : public controller
{
private:
    bool _initialized;
    bool _srgb_textures_are_broken;     // XXX: Hack: work around broken SRGB texture implementations

    /* We manage two frames, each with its own set of properties etc.
     * The active frame is the one that is displayed, the other frame is the one
     * that is prepared for display.
     * Each frame contains a left view and may contain a right view. */
    int _active_index;                                  // 0 or 1

    video_frame _frame[2];              // input frames (active / preparing)
    parameters _params;                 // current parameters for display
    // Step 1: input of video data
    GLuint _input_pbo;                  // pixel-buffer object for texture uploading
    GLuint _input_yuv_y_tex[2][2];      // for yuv formats: y component
    GLuint _input_yuv_u_tex[2][2];      // for yuv formats: u component
    GLuint _input_yuv_v_tex[2][2];      // for yuv formats: v component
    GLuint _input_bgra32_tex[2][2];     // for bgra32 format
    int _input_yuv_chroma_width_divisor[2];     // for yuv formats: chroma subsampling
    int _input_yuv_chroma_height_divisor[2];    // for yuv formats: chroma subsampling
    // Step 2: rendering
    video_frame _color_last_frame;      // last frame for this step; used for reinitialization check
    GLuint _color_prg;                  // color space transformation, color adjustment
    GLuint _color_fbo;                  // framebuffer object to render into the sRGB texture
    GLuint _color_srgb_tex[2];          // output: sRGB texture
    // Step 3: rendering
    parameters _render_last_params;     // last params for this step; used for reinitialization check
    GLuint _render_prg;                 // reads sRGB texture, renders according to _params[_active_index]
    GLuint _render_mask_tex;            // for the masking modes even-odd-{rows,columns}, checkerboard
    // OpenGL Viewport for drawing the video frame
    GLint _viewport[4];

private:
    // Step 1: initialize/deinitialize, and check if reinitialization is necessary
    void input_init(int index, const video_frame &frame);
    void input_deinit(int index);
    bool input_is_compatible(int index, const video_frame &current_frame);
    // Step 2: initialize/deinitialize, and check if reinitialization is necessary
    void color_init(const video_frame &frame);
    void color_deinit();
    bool color_is_compatible(const video_frame &current_frame);
    // Step 3: initialize/deinitialize, and check if reinitialization is necessary
    void render_init();
    void render_deinit();
    bool render_is_compatible();
    // Compute screen pixel aspect ratio
    float screen_pixel_aspect_ratio();

protected:
    virtual void make_context_current() = 0;    // Make sure our OpenGL context is current
    virtual bool context_is_stereo() = 0;       // Is our current OpenGL context a stereo context?
    virtual void recreate_context(bool stereo) = 0;     // Recreate an OpenGL context and make it current
    virtual void trigger_update() = 0;          // Trigger a redraw (i.e. make GL context current and call display())
    virtual void trigger_resize(int w, int h) = 0;      // Trigger a resize the video area

    void clear();                               // Clear the video area
    void reshape(int w, int h);                 // Call this when the video area was resized
    bool need_redisplay_on_move();              // Whether we need to redisplay if the video area moved

    /* Display the current frame.
     * First version: This version is used by Equalizer, which needs to set some special properties.
     * Second version: This version is called by the player which knows the current params.
     * Third version: This version is used to redisplay the current frame, e.g. for repaint events.
     * TODO: This function needs to handle interlaced frames! */
    void display_current_frame(bool mono_right_instead_of_left,
            float x, float y, float w, float h, const GLint viewport[4]);
    void display_current_frame()
    {
        display_current_frame(false, -1.0f, -1.0f, 2.0f, 2.0f, _viewport);
    }

public:
    /* Constructor, Destructor */
    video_output(bool receive_notifications = true);
    virtual ~video_output();

    /* Initialize the video output, or throw an exception */
    virtual void init();
    /* Deinitialize the video output */
    virtual void deinit();

    /* Set a video area size suitable for the given input/output settings */
    void set_suitable_size(int w, int h, float ar, parameters::stereo_mode_t stereo_mode);

    /* Get capabilities */
    virtual bool supports_stereo() const = 0;   // Is OpenGL quad buffered stereo available?

    /* Get screen properties (fixed) */
    virtual int screen_width() = 0;             // in pixels
    virtual int screen_height() = 0;            // in pixels
    virtual float screen_aspect_ratio() = 0;    // refers to actual ar (pixels may be non-square)

    /* Get current video area properties */
    virtual int width() = 0;                    // in pixels
    virtual int height() = 0;                   // in pixels
    virtual float aspect_ratio() = 0;           // refers to actual ar (pixels may be non-square)
    virtual int pos_x() = 0;                    // in pixels
    virtual int pos_y() = 0;                    // in pixels

    /* Center video area on screen */
    virtual void center() = 0;

    /* Enter/exit fullscreen mode */
    virtual void enter_fullscreen() = 0;
    virtual void exit_fullscreen() = 0;
    virtual bool toggle_fullscreen() = 0;

    /* Process window system events (if applicable) */
    virtual bool has_events() = 0;
    virtual void process_events() = 0;
    
    /* Prepare a new frame for display. */
    void prepare_next_frame(const video_frame &frame);
    /* Switch to the next frame (make it the current one) */
    void activate_next_frame();
    /* Set display parameters. */
    void set_parameters(const parameters &params);

    /* Receive a notification from the player. */
    virtual void receive_notification(const notification &note) = 0;
};

#endif
