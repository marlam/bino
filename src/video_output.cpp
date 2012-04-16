/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010, 2011, 2012
 * Martin Lambers <marlam@marlam.de>
 * Frédéric Devernay <frederic.devernay@inrialpes.fr>
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

#include "config.h"

#include <limits>
#include <cstdlib>
#include <cstring>
#include <cmath>

#include <GL/glew.h>

#include "gettext.h"
#define _(string) gettext(string)

#include "exc.h"
#include "msg.h"
#include "str.h"
#include "timer.h"
#include "blob.h"
#include "dbg.h"

#include "video_output.h"
#include "video_output_color.fs.glsl.h"
#include "video_output_render.fs.glsl.h"

#if HAVE_LIBXNVCTRL
#include "NvSDIout.h"
#endif // HAVE_LIBXNVCTRL

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
 * If the input data had an 8 bit value range, the result is converted to sRGB
 * and stored in an GL_SRGB texture. If the input data had a larger value range,
 * the result is converted to linear RGB and stored in an GL_RGB16 texture.
 * In this color correction step, no interpolation is done, because we're
 * dealing with non-linear values, and interpolating them would lead to
 * errors. We do not store linear RGB in GL_RGB8 textures because that would
 * lose some precision when compared to the input data - so we either use
 * GL_SRGB8 (and store sRGB values) or GL_RGB16 (and store linear values).
 * In both cases, the rendering step can properly interpolate.
 *
 * Step 3: Rendering.
 * This step reads from the color textures created in the previous step. In the
 * case of GL_SRGB8 textures, this means that OpenGL will transform the input to
 * linear RGB automatically and handle hardware accelerated bilinear
 * interpolation correctly. Thus, magnification or minification are safe in this
 * step. With GL_RGB16 textures and the linear values stored therein, no special
 * handling is necessary.
 * Furthermore, we can do interpolation on the linear RGB values for the masking
 * output modes. We then transform the resulting linear RGB values back to
 * non-linear sRGB values for output. We do not use the GL_ARB_framebuffer_sRGB
 * extension for this purpose because 1) we need computations on non-linear
 * values for the anaglyph methods and 2) sRGB framebuffers are not yet widely
 * supported.
 *
 * Open issues / TODO:
 * The 420p and 422p chroma subsampling formats are currently handled by
 * sampling the U and V textures with bilinear interpolation at the correct
 * position according to the chroma location. Bilinear interpolation of U and V
 * is questionable since these values are not linear. However, I could not find
 * information on a better way to do this, and other players seem to use linear
 * interpolation, too.
 */

class subtitle_updater : public thread
{
private:
    // The last rendered subtitle
    subtitle_box _last_subtitle;
    int64_t _last_timestamp;
    parameters _last_params;
    int _last_outwidth;
    int _last_outheight;
    float _last_pixel_ar;
    // The current subtitle to render
    subtitle_box _subtitle;
    int64_t _timestamp;
    parameters _params;
    int _outwidth;
    int _outheight;
    float _pixel_ar;
    // The renderer and rendering buffer
    subtitle_renderer* _renderer;
    blob _buffer;
    int _bb_x, _bb_y, _bb_w, _bb_h;
    bool _buffer_changed;

public:
    subtitle_updater(subtitle_renderer* sr);

    // Reset
    void reset();
    // Set all necessary information to render the next subtitle.
    void set(const subtitle_box& subtitle, int64_t timestamp,
            const parameters& params, int outwidth, int outheight, float pixel_ar);
    // Start the subtitle rendering with thread::start(), which will execute the run() fuction.
    // The wait for it to finish using the thread::finish() function.
    virtual void run();
    // Find out if the buffer changed, and get all required info.
    bool get(int *outwidth, int* outheight,
            void** ptr, int* bb_x, int* bb_y, int* bb_w, int* bb_h);
};

subtitle_updater::subtitle_updater(subtitle_renderer* renderer) :
    _renderer(renderer)
{
    reset();
}

void subtitle_updater::reset()
{
    _last_outwidth = -1;
    _last_outheight = -1;
    _last_timestamp = std::numeric_limits<int64_t>::min();
}

void subtitle_updater::set(
        const subtitle_box& subtitle, int64_t timestamp,
        const parameters& params, int outwidth, int outheight, float pixel_ar)
{
    _subtitle = subtitle;
    _timestamp = timestamp;
    _params = params;
    _outwidth = outwidth;
    _outheight = outheight;
    _pixel_ar = pixel_ar;
}

void subtitle_updater::run()
{
    _buffer_changed = false;
    if (_subtitle.is_valid()
            && (_subtitle != _last_subtitle
                || (!_subtitle.is_constant() && _timestamp != _last_timestamp)
                || _outwidth != _last_outwidth
                || _outheight != _last_outheight
                || (_pixel_ar < _last_pixel_ar || _pixel_ar > _last_pixel_ar)
                || _params.subtitle_encoding() != _last_params.subtitle_encoding()
                || _params.subtitle_font() != _last_params.subtitle_font()
                || _params.subtitle_size() != _last_params.subtitle_size()
                || (_params.subtitle_scale() < _last_params.subtitle_scale()
                    || _params.subtitle_scale() > _last_params.subtitle_scale())
                || _params.subtitle_color() != _last_params.subtitle_color()
                || _params.subtitle_shadow() != _last_params.subtitle_shadow())) {
        // We have a new subtitle or a new video display size or new parameters,
        // therefore we need to render it.
        _buffer_changed = _renderer->prerender(
                _subtitle, _timestamp, _params,
                _outwidth, _outheight, _pixel_ar,
                _bb_x, _bb_y, _bb_w, _bb_h);
        _last_subtitle = _subtitle;
        _last_timestamp = _timestamp;
        _last_params = _params;
        _last_outwidth = _outwidth;
        _last_outheight = _outheight;
        _last_pixel_ar = _pixel_ar;
    }

    if (_buffer_changed) {
        size_t bufsize = _bb_w * _bb_h * sizeof(uint32_t);
        if (_buffer.size() < bufsize)
            _buffer.resize(bufsize);
        _renderer->render(_buffer.ptr<uint32_t>());
    }
}

bool subtitle_updater::get(int *outwidth, int* outheight,
        void** ptr, int* bb_x, int* bb_y, int* bb_w, int* bb_h)
{
    *outwidth = _outwidth;
    *outheight = _outheight;
    *ptr = _buffer.ptr();
    *bb_x = _bb_x;
    *bb_y = _bb_y;
    *bb_w = _bb_w;
    *bb_h = _bb_h;
    return _buffer_changed;
}


static const float full_tex_coords[2][4][2] =
{
    { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } },
    { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } }
};

video_output::video_output() : controller(), _initialized(false)
{
    _input_pbo = 0;
    _input_fbo = 0;
    _active_index = 1;
    for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            _input_yuv_y_tex[i][j] = 0;
            _input_yuv_u_tex[i][j] = 0;
            _input_yuv_v_tex[i][j] = 0;
            _input_bgra32_tex[i][j] = 0;
            _color_tex[i][j] = 0;
        }
        _input_subtitle_tex[i] = 0;
        _input_subtitle_tex_current[i] = false;
        _color_prg[i] = 0;
    }
    _color_fbo = 0;
    _render_prg = 0;
    _render_dummy_tex = 0;
    _render_mask_tex = 0;
    _subtitle_updater = new subtitle_updater(&_subtitle_renderer);
#if HAVE_LIBXNVCTRL
    _nv_sdi_output = new CNvSDIout();
#endif // HAVE_LIBXNVCTRL
}

video_output::~video_output()
{
    delete _subtitle_updater;
#if HAVE_LIBXNVCTRL
    delete _nv_sdi_output;
#endif // HAVE_LIBXNVCTRL
}

/**
 * \param where     Location of the check.
 * \returns         True.
 *
 * Checks if a GL error occured.
 * If an error occured, an appropriate exception is thrown.\n
 * The purpose of the return value is to be able to put this function
 * into an assert statement.
 */
bool video_output::xglCheckError(const std::string& where) const
{
    GLenum e = glGetError();
    if (e != GL_NO_ERROR) {
        std::string pfx = (where.length() > 0 ? where + ": " : "");
        throw exc(pfx + str::asprintf(_("OpenGL error 0x%04X."), static_cast<unsigned int>(e)));
        // Don't use gluErrorString(e) here to avoid depending on libGLU just for this
        return false;
    }
    return true;
}

bool video_output::xglCheckFBO(const std::string& where) const
{
    GLenum e = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
    if (e != GL_FRAMEBUFFER_COMPLETE_EXT) {
        std::string pfx = (where.length() > 0 ? where + ": " : "");
        throw exc(pfx + str::asprintf(_("OpenGL Framebuffer status error 0x%04X."), static_cast<unsigned int>(e)));
        return false;
    }
    return true;
}

static void xglKillCrlf(char* str)
{
    size_t l = std::strlen(str);
    if (l > 0 && str[l - 1] == '\n')
        str[--l] = '\0';
    if (l > 0 && str[l - 1] == '\r')
        str[l - 1] = '\0';
}

/**
 * \param name      Name of the shader. Can be an arbitrary string.
 * \param type      GL_VERTEX_SHADER or GL_FRAGMENT_SHADER.
 * \param src       The source code of the shader.
 * \returns         The GL shader object.
 *
 * Creates a GL shader object. The \a name of the shader is only used
 * for error reporting purposes. If compilation fails, an exception is thrown.
 */
GLuint video_output::xglCompileShader(const std::string& name, GLenum type, const std::string& src) const
{
    msg::dbg("Compiling %s shader %s.", type == GL_VERTEX_SHADER ? "vertex" : "fragment", name.c_str());

    // XXX: Work around a bad bad bug in the free OpenGL drivers for ATI cards on Ubuntu
    // 10.10: the compilation of shader source depends on the locale, and gives wrong
    // results e.g. in de_DE.UTF-8. So we backup the locale, set it to "C", and restore
    // the backup after compilation.
    std::string locale_backup = setlocale(LC_ALL, NULL);
    setlocale(LC_ALL, "C");

    GLuint shader = glCreateShader(type);
    const GLchar* glsrc = src.c_str();
    glShaderSource(shader, 1, &glsrc, NULL);
    glCompileShader(shader);

    setlocale(LC_ALL, locale_backup.c_str());

    std::string log;
    GLint e, l;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &e);
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &l);
    if (l > 0) {
        char* tmplog = new char[l];
        glGetShaderInfoLog(shader, l, NULL, tmplog);
        xglKillCrlf(tmplog);
        log = std::string(tmplog);
        delete[] tmplog;
    } else {
        log = std::string("");
    }

    if (e == GL_TRUE && log.length() > 0) {
        msg::wrn(_("OpenGL %s '%s': compiler warning:"),
                type == GL_VERTEX_SHADER ? _("vertex shader") : _("fragment shader"),
                name.c_str());
        msg::wrn_txt("%s", log.c_str());
    } else if (e != GL_TRUE) {
        std::string when = str::asprintf(_("OpenGL %s '%s': compilation failed."),
                type == GL_VERTEX_SHADER ? _("vertex shader") : _("fragment shader"),
                name.c_str());
        std::string what = str::asprintf("\n%s", log.length() > 0 ? log.c_str() : _("unknown error"));
        throw exc(str::asprintf(_("%s: %s"), when.c_str(), what.c_str()));
    }
    return shader;
}

/**
 * \param vshader   A vertex shader, or 0.
 * \param fshader   A fragment shader, or 0.
 * \returns         The GL program object.
 *
 * Creates a GL program object.
 */
GLuint video_output::xglCreateProgram(GLuint vshader, GLuint fshader) const
{
    assert(vshader != 0 || fshader != 0);
    GLuint program = glCreateProgram();
    if (vshader != 0)
        glAttachShader(program, vshader);
    if (fshader != 0)
        glAttachShader(program, fshader);
    return program;
}

/**
 * \param name              Name of the program. Can be an arbitrary string.
 * \param vshader_src       Source of the vertex shader, or an empty string.
 * \param fshader_src       Source of the fragment shader, or an empty string.
 * \returns                 The GL program object.
 *
 * Creates a GL program object. The \a name of the program is only used
 * for error reporting purposes.
 */
GLuint video_output::xglCreateProgram(const std::string& name,
        const std::string& vshader_src, const std::string& fshader_src) const
{
    GLuint vshader = 0, fshader = 0;
    if (vshader_src.length() > 0)
        vshader = xglCompileShader(name, GL_VERTEX_SHADER, vshader_src);
    if (fshader_src.length() > 0)
        fshader = xglCompileShader(name, GL_FRAGMENT_SHADER, fshader_src);
    return xglCreateProgram(vshader, fshader);
}

/**
 * \param name      Name of the program. Can be an arbitrary string.
 * \param prg       The GL program object.
 * \returns         The relinked GL program object.
 *
 * Links a GL program object. The \a name of the program is only used
 * for error reporting purposes. If linking fails, an exception is thrown.
 */
void video_output::xglLinkProgram(const std::string& name, const GLuint prg) const
{
    msg::dbg("Linking OpenGL program %s.", name.c_str());

    glLinkProgram(prg);

    std::string log;
    GLint e, l;
    glGetProgramiv(prg, GL_LINK_STATUS, &e);
    glGetProgramiv(prg, GL_INFO_LOG_LENGTH, &l);
    if (l > 0) {
        char *tmplog = new char[l];
        glGetProgramInfoLog(prg, l, NULL, tmplog);
        xglKillCrlf(tmplog);
        log = std::string(tmplog);
        delete[] tmplog;
    } else {
        log = std::string("");
    }

    if (e == GL_TRUE && log.length() > 0) {
        msg::wrn(_("OpenGL program '%s': linker warning:"), name.c_str());
        msg::wrn_txt("%s", log.c_str());
    } else if (e != GL_TRUE) {
        std::string when = str::asprintf(_("OpenGL program '%s': linking failed."), name.c_str());
        std::string what = str::asprintf("\n%s", log.length() > 0 ? log.c_str() : _("unknown error"));
        throw exc(when + ": " + what);
    }
}

/**
 * \param program   The program.
 *
 * Deletes a GL program and all its associated shaders. Does nothing if
 * \a program is not a valid program.
 */
void video_output::xglDeleteProgram(GLuint program) const
{
    if (glIsProgram(program)) {
        GLint shader_count;
        glGetProgramiv(program, GL_ATTACHED_SHADERS, &shader_count);
        GLuint* shaders = new GLuint[shader_count];
        glGetAttachedShaders(program, shader_count, NULL, shaders);
        for (int i = 0; i < shader_count; i++)
            glDeleteShader(shaders[i]);
        delete[] shaders;
        glDeleteProgram(program);
    }
}

bool video_output::srgb8_textures_are_color_renderable()
{
    bool retval = true;
    GLuint fbo;
    GLuint tex;

    glGenFramebuffersEXT(1, &fbo);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8, 2, 2, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
    GLint framebuffer_bak;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &framebuffer_bak);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,
            GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, tex, 0);
    GLenum e = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
    if (e != GL_FRAMEBUFFER_COMPLETE_EXT) {
        retval = false;
    }
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, framebuffer_bak);
    glDeleteFramebuffersEXT(1, &fbo);
    glDeleteTextures(1, &tex);
    return retval;
}


void video_output::start_subtitle_updating(
        const video_frame& frame, const subtitle_box& subtitle, const parameters& params)
{
    if (subtitle.is_valid()) {
        assert(_subtitle_renderer.is_initialized());
        int sub_outwidth, sub_outheight;
        if (_subtitle_renderer.render_to_display_size(subtitle)) {
            sub_outwidth = video_display_width();
            sub_outheight = video_display_height();
        } else {
            sub_outwidth = frame.width;
            sub_outheight = frame.height;
        }
        _subtitle_updater->set(subtitle, frame.presentation_time,
                params, sub_outwidth, sub_outheight,
                screen_pixel_aspect_ratio());
        _subtitle_updater->start();
    }
}

void video_output::finish_subtitle_updating(const subtitle_box& subtitle, int index)
{
    if (subtitle.is_valid()) {
        _subtitle_updater->finish();
        int sub_outwidth, sub_outheight;
        void *ptr;
        int bb_x, bb_y, bb_w, bb_h;
        bool buffer_updated = _subtitle_updater->get(
                &sub_outwidth, &sub_outheight,
                &ptr, &bb_x, &bb_y, &bb_w, &bb_h);
        if (buffer_updated || !_input_subtitle_tex_current[index]) {
            // Make sure the texture has the right size
            assert(xglCheckError(HERE));
            GLint tex_w, tex_h;
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, _input_subtitle_tex[index]);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &tex_w);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &tex_h);
            if (tex_w != sub_outwidth || tex_h != sub_outheight) {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, sub_outwidth, sub_outheight, 0,
                        GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
            }
            // Clear the texture
            GLint framebuffer_bak;
            glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &framebuffer_bak);
            glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, _input_fbo);
            glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,
                    GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, _input_subtitle_tex[index], 0);
            xglCheckFBO(HERE);
            glClear(GL_COLOR_BUFFER_BIT);
            glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, framebuffer_bak);
            // Get a PBO buffer of appropriate size for the bounding box.
            if (bb_w > 0 && bb_h > 0) {
                size_t size = bb_w * bb_h * sizeof(uint32_t);
                glBindBuffer(GL_PIXEL_UNPACK_BUFFER, _input_pbo);
                glBufferData(GL_PIXEL_UNPACK_BUFFER, size, NULL, GL_STREAM_DRAW);
                void* pboptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
                if (!pboptr)
                    throw exc(_("Cannot create a PBO buffer."));
                assert(reinterpret_cast<uintptr_t>(pboptr) % 4 == 0);
                // Copy the bounding box into the buffer
                std::memcpy(pboptr, ptr, size);
                // Update the appropriate part of the texture.
                glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
                glPixelStorei(GL_UNPACK_ROW_LENGTH, bb_w);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
                glTexSubImage2D(GL_TEXTURE_2D, 0, bb_x, bb_y, bb_w, bb_h,
                        GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
                glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
                assert(xglCheckError(HERE));
            }
        }
        _input_subtitle_tex_current[index] = true;
        if (buffer_updated) {
            _input_subtitle_tex_current[index == 0 ? 1 : 0] = false;
        }
    }
}


void video_output::init()
{
    if (!_initialized)
    {
        /* currently nothing to do */
        _initialized = true;

#if HAVE_LIBXNVCTRL
        _nv_sdi_output->init(_params.sdi_output_format());
#endif // HAVE_LIBXNVCTRL
    }
}

void video_output::deinit()
{
    if (_initialized)
    {
        clear();
        xglCheckError(HERE);
#if HAVE_LIBXNVCTRL
        if (_nv_sdi_output->isInitialized()) {
            _nv_sdi_output->deinit();
        }
#endif // HAVE_LIBXNVCTRL
        input_deinit(0);
        input_deinit(1);
        color_deinit(0);
        color_deinit(1);
        render_deinit();
        xglCheckError(HERE);
        _initialized = false;
    }
}

void video_output::set_suitable_size(int width, int height, float ar, parameters::stereo_mode_t stereo_mode)
{
    float aspect_ratio = width * screen_pixel_aspect_ratio() / height;
    if (stereo_mode == parameters::mode_left_right)
    {
        aspect_ratio /= 2.0f;
    }
    else if (stereo_mode == parameters::mode_top_bottom
            || stereo_mode == parameters::mode_hdmi_frame_pack)
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
    xglCheckError(HERE);
    glGenBuffers(1, &_input_pbo);
    glGenFramebuffersEXT(1, &_input_fbo);
    if (frame.layout == video_frame::bgra32)
    {
        for (int i = 0; i < (frame.stereo_layout == parameters::layout_mono ? 1 : 2); i++)
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
        bool type_u8 = (frame.value_range == video_frame::u8_full || frame.value_range == video_frame::u8_mpeg);
        GLint internal_format = type_u8 ? GL_LUMINANCE8 : GL_LUMINANCE16;
        GLint type = type_u8 ? GL_UNSIGNED_BYTE : GL_UNSIGNED_SHORT;
        for (int i = 0; i < (frame.stereo_layout == parameters::layout_mono ? 1 : 2); i++)
        {
            glGenTextures(1, &(_input_yuv_y_tex[index][i]));
            glBindTexture(GL_TEXTURE_2D, _input_yuv_y_tex[index][i]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, internal_format,
                    frame.width,
                    frame.height,
                    0, GL_LUMINANCE, type, NULL);
            glGenTextures(1, &(_input_yuv_u_tex[index][i]));
            glBindTexture(GL_TEXTURE_2D, _input_yuv_u_tex[index][i]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, need_chroma_filtering ? GL_LINEAR : GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, need_chroma_filtering ? GL_LINEAR : GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, internal_format,
                    frame.width / _input_yuv_chroma_width_divisor[index],
                    frame.height / _input_yuv_chroma_height_divisor[index],
                    0, GL_LUMINANCE, type, NULL);
            glGenTextures(1, &(_input_yuv_v_tex[index][i]));
            glBindTexture(GL_TEXTURE_2D, _input_yuv_v_tex[index][i]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, need_chroma_filtering ? GL_LINEAR : GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, need_chroma_filtering ? GL_LINEAR : GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, internal_format,
                    frame.width / _input_yuv_chroma_width_divisor[index],
                    frame.height / _input_yuv_chroma_height_divisor[index],
                    0, GL_LUMINANCE, type, NULL);
        }
    }
    glGenTextures(2, _input_subtitle_tex);
    for (int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, _input_subtitle_tex[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    xglCheckError(HERE);
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
    xglCheckError(HERE);
    glDeleteBuffers(1, &_input_pbo);
    _input_pbo = 0;
    glDeleteFramebuffersEXT(1, &_input_fbo);
    _input_fbo = 0;
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
    if (_input_subtitle_tex[index] != 0) {
        glDeleteTextures(1, _input_subtitle_tex + index);
        _input_subtitle_tex[index] = 0;
        _input_subtitle_tex_current[index] = false;
        _input_subtitle[index] = subtitle_box();
    }
    _subtitle_updater->reset();
    _input_yuv_chroma_width_divisor[index] = 0;
    _input_yuv_chroma_height_divisor[index] = 0;
    _frame[index] = video_frame();
    xglCheckError(HERE);
}

static int next_multiple_of_4(int x)
{
    return (x / 4 + (x % 4 == 0 ? 0 : 1)) * 4;
}

void video_output::prepare_next_frame(const video_frame &frame, const subtitle_box &subtitle)
{
    // Initialization
    int index = (_active_index == 0 ? 1 : 0);
    if (!frame.is_valid())
    {
        _frame[index] = frame;
        return;
    }
    assert(xglCheckError(HERE));
    if (!input_is_compatible(index, frame))
    {
        input_deinit(index);
        input_init(index, frame);
    }

    // Start rendering the subtitle in a separate thread.
    // In the common case, the video display width and height do not change
    // between preparing a frame and rendering it, so it is benefical to update
    // to subtitle texture in this function.
    start_subtitle_updating(frame, subtitle, dispatch::parameters());

    // Upload the frame data
    _frame[index] = frame;
    int bytes_per_pixel = 4;
    GLenum format = GL_BGRA;
    GLenum type = GL_UNSIGNED_INT_8_8_8_8_REV;
    if (frame.layout != video_frame::bgra32)
    {
        bool type_u8 = (frame.value_range == video_frame::u8_full || frame.value_range == video_frame::u8_mpeg);
        bytes_per_pixel = type_u8 ? 1 : 2;
        format = GL_LUMINANCE;
        type = type_u8 ? GL_UNSIGNED_BYTE : GL_UNSIGNED_SHORT;
    }
    for (int i = 0; i < (frame.stereo_layout == parameters::layout_mono ? 1 : 2); i++)
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
                throw exc(_("Cannot create a PBO buffer."));
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
    assert(xglCheckError(HERE));

    /* Step 2: color-correction */

    if (!_color_prg[index] || !color_is_compatible(index, frame)) {
        color_deinit(index);
        color_init(index, frame);
        _color_last_frame[index] = frame;
    }
    int left = 0;
    int right = (frame.stereo_layout == parameters::layout_mono ? 0 : 1);

    // Backup GL state
    GLint framebuffer_bak;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &framebuffer_bak);
    GLint viewport_bak[4];
    glGetIntegerv(GL_VIEWPORT, viewport_bak);
    GLboolean scissor_bak = glIsEnabled(GL_SCISSOR_TEST);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();

    glEnable(GL_TEXTURE_2D);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glDisable(GL_SCISSOR_TEST);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glViewport(0, 0, frame.width, frame.height);
    glUseProgram(_color_prg[index]);
    if (frame.layout == video_frame::bgra32) {
        glUniform1i(glGetUniformLocation(_color_prg[index], "srgb_tex"), 0);
    } else {
        glUniform1i(glGetUniformLocation(_color_prg[index], "y_tex"), 0);
        glUniform1i(glGetUniformLocation(_color_prg[index], "u_tex"), 1);
        glUniform1i(glGetUniformLocation(_color_prg[index], "v_tex"), 2);
    }
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, _color_fbo);
    // left view: render into _color_tex[index][0]
    if (frame.layout == video_frame::bgra32) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, _input_bgra32_tex[index][left]);
    } else {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, _input_yuv_y_tex[index][left]);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, _input_yuv_u_tex[index][left]);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, _input_yuv_v_tex[index][left]);
    }
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,
            GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, _color_tex[index][0], 0);
    xglCheckFBO(HERE);
    draw_quad(-1.0f, +1.0f, +2.0f, -2.0f);
    // right view: render into _color_tex[index][1]
    if (left != right) {
        if (frame.layout == video_frame::bgra32) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, _input_bgra32_tex[index][right]);
        } else {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, _input_yuv_y_tex[index][right]);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, _input_yuv_u_tex[index][right]);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, _input_yuv_v_tex[index][right]);
        }
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,
                GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, _color_tex[index][1], 0);
        xglCheckFBO(HERE);
        draw_quad(-1.0f, +1.0f, +2.0f, -2.0f);
    }

    // Restore GL state
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, framebuffer_bak);
    if (scissor_bak)
        glEnable(GL_SCISSOR_TEST);
    glViewport(viewport_bak[0], viewport_bak[1], viewport_bak[2], viewport_bak[3]);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);

    // Finish the subtitle updating
    finish_subtitle_updating(subtitle, index);
    _input_subtitle[index] = subtitle;
}

int video_output::video_display_width() const
{
    assert(_viewport[0][2] > 0);
    return _viewport[0][2];
}

int video_output::video_display_height() const
{
    assert(_viewport[0][3] > 0);
    return _viewport[0][3];
}

void video_output::color_init(int index, const video_frame &frame)
{
    xglCheckError(HERE);
    glGenFramebuffersEXT(1, &_color_fbo);
    std::string layout_str;
    std::string color_space_str;
    std::string value_range_str;
    std::string storage_str;
    std::string chroma_offset_x_str;
    std::string chroma_offset_y_str;
    if (frame.layout == video_frame::bgra32)
    {
        layout_str = "layout_bgra32";
        color_space_str = "color_space_srgb";
        value_range_str = "value_range_8bit_full";
        storage_str = "storage_srgb";
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
            storage_str = "storage_srgb";
        }
        else if (frame.value_range == video_frame::u8_mpeg)
        {
            value_range_str = "value_range_8bit_mpeg";
            storage_str = "storage_srgb";
        }
        else if (frame.value_range == video_frame::u10_full)
        {
            value_range_str = "value_range_10bit_full";
            storage_str = "storage_linear_rgb";
        }
        else
        {
            value_range_str = "value_range_10bit_mpeg";
            storage_str = "storage_linear_rgb";
        }
        chroma_offset_x_str = "0.0";
        chroma_offset_y_str = "0.0";
        if (frame.layout == video_frame::yuv422p)
        {
            if (frame.chroma_location == video_frame::left)
            {
                chroma_offset_x_str = str::from(0.5f / static_cast<float>(frame.width
                            / _input_yuv_chroma_width_divisor[index]));
            }
            else if (frame.chroma_location == video_frame::topleft)
            {
                chroma_offset_x_str = str::from(0.5f / static_cast<float>(frame.width
                            / _input_yuv_chroma_width_divisor[index]));
                chroma_offset_y_str = str::from(0.5f / static_cast<float>(frame.height
                            / _input_yuv_chroma_height_divisor[index]));
            }
        }
        else if (frame.layout == video_frame::yuv420p)
        {
            if (frame.chroma_location == video_frame::left)
            {
                chroma_offset_x_str = str::from(0.5f / static_cast<float>(frame.width
                            / _input_yuv_chroma_width_divisor[index]));
            }
            else if (frame.chroma_location == video_frame::topleft)
            {
                chroma_offset_x_str = str::from(0.5f / static_cast<float>(frame.width
                            / _input_yuv_chroma_width_divisor[index]));
                chroma_offset_y_str = str::from(0.5f / static_cast<float>(frame.height
                            / _input_yuv_chroma_height_divisor[index]));
            }
        }
    }
    // XXX: Hack: work around broken SRGB texture implementations
    if (!srgb8_textures_are_color_renderable() || std::getenv("SRGB_TEXTURES_ARE_BROKEN"))
    {
        msg::dbg("Avoiding broken SRGB texture implementation.");
        storage_str = "storage_linear_rgb";
    }

    std::string color_fs_src(VIDEO_OUTPUT_COLOR_FS_GLSL_STR);
    str::replace(color_fs_src, "$layout", layout_str);
    str::replace(color_fs_src, "$color_space", color_space_str);
    str::replace(color_fs_src, "$value_range", value_range_str);
    str::replace(color_fs_src, "$chroma_offset_x", chroma_offset_x_str);
    str::replace(color_fs_src, "$chroma_offset_y", chroma_offset_y_str);
    str::replace(color_fs_src, "$storage", storage_str);
    _color_prg[index] = xglCreateProgram("video_output_color", "", color_fs_src);
    xglLinkProgram("video_output_color", _color_prg[index]);
    for (int i = 0; i < (frame.stereo_layout == parameters::layout_mono ? 1 : 2); i++)
    {
        glGenTextures(1, &(_color_tex[index][i]));
        glBindTexture(GL_TEXTURE_2D, _color_tex[index][i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexImage2D(GL_TEXTURE_2D, 0,
                storage_str == "storage_srgb" ? GL_SRGB8 : GL_RGB16,
                frame.width, frame.height, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
    }
    xglCheckError(HERE);
}

void video_output::color_deinit(int index)
{
    xglCheckError(HERE);
    glDeleteFramebuffersEXT(1, &_color_fbo);
    _color_fbo = 0;
    if (_color_prg[index] != 0) {
        xglDeleteProgram(_color_prg[index]);
        _color_prg[index] = 0;
    }
    for (int i = 0; i < 2; i++) {
        if (_color_tex[index][i] != 0) {
            glDeleteTextures(1, &(_color_tex[index][i]));
            _color_tex[index][i] = 0;
        }
    }
    _color_last_frame[index] = video_frame();
    xglCheckError(HERE);
}

bool video_output::color_is_compatible(int index, const video_frame &current_frame)
{
    return (_color_last_frame[index].width == current_frame.width
            && _color_last_frame[index].height == current_frame.height
            && _color_last_frame[index].layout == current_frame.layout
            && _color_last_frame[index].color_space == current_frame.color_space
            && _color_last_frame[index].value_range == current_frame.value_range
            && _color_last_frame[index].chroma_location == current_frame.chroma_location
            && _color_last_frame[index].stereo_layout == current_frame.stereo_layout);
}

void video_output::render_init()
{
    xglCheckError(HERE);
    std::string mode_str = (
            _params.stereo_mode() == parameters::mode_even_odd_rows ? "mode_even_odd_rows"
            : _params.stereo_mode() == parameters::mode_even_odd_columns ? "mode_even_odd_columns"
            : _params.stereo_mode() == parameters::mode_checkerboard ? "mode_checkerboard"
            : _params.stereo_mode() == parameters::mode_red_cyan_monochrome ? "mode_red_cyan_monochrome"
            : _params.stereo_mode() == parameters::mode_red_cyan_half_color ? "mode_red_cyan_half_color"
            : _params.stereo_mode() == parameters::mode_red_cyan_full_color ? "mode_red_cyan_full_color"
            : _params.stereo_mode() == parameters::mode_red_cyan_dubois ? "mode_red_cyan_dubois"
            : _params.stereo_mode() == parameters::mode_green_magenta_monochrome ? "mode_green_magenta_monochrome"
            : _params.stereo_mode() == parameters::mode_green_magenta_half_color ? "mode_green_magenta_half_color"
            : _params.stereo_mode() == parameters::mode_green_magenta_full_color ? "mode_green_magenta_full_color"
            : _params.stereo_mode() == parameters::mode_green_magenta_dubois ? "mode_green_magenta_dubois"
            : _params.stereo_mode() == parameters::mode_amber_blue_monochrome ? "mode_amber_blue_monochrome"
            : _params.stereo_mode() == parameters::mode_amber_blue_half_color ? "mode_amber_blue_half_color"
            : _params.stereo_mode() == parameters::mode_amber_blue_full_color ? "mode_amber_blue_full_color"
            : _params.stereo_mode() == parameters::mode_amber_blue_dubois ? "mode_amber_blue_dubois"
            : _params.stereo_mode() == parameters::mode_red_green_monochrome ? "mode_red_green_monochrome"
            : _params.stereo_mode() == parameters::mode_red_blue_monochrome ? "mode_red_blue_monochrome"
            : "mode_onechannel");
    std::string render_fs_src(VIDEO_OUTPUT_RENDER_FS_GLSL_STR);
    str::replace(render_fs_src, "$mode", mode_str);
    _render_prg = xglCreateProgram("video_output_render", "", render_fs_src);
    xglLinkProgram("video_output_render", _render_prg);
    uint32_t dummy_texture = 0;
    glGenTextures(1, &_render_dummy_tex);
    glBindTexture(GL_TEXTURE_2D, _render_dummy_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0,
            GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, &dummy_texture);
    if (_params.stereo_mode() == parameters::mode_even_odd_rows
            || _params.stereo_mode() == parameters::mode_even_odd_columns
            || _params.stereo_mode() == parameters::mode_checkerboard)
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
                _params.stereo_mode() == parameters::mode_even_odd_rows ? even_odd_rows_mask
                : _params.stereo_mode() == parameters::mode_even_odd_columns ? even_odd_columns_mask
                : checkerboard_mask);
    }
    xglCheckError(HERE);
}

void video_output::render_deinit()
{
    xglCheckError(HERE);
    if (_render_prg != 0)
    {
        xglDeleteProgram(_render_prg);
        _render_prg = 0;
    }
    if (_render_dummy_tex != 0)
    {
        glDeleteTextures(1, &_render_dummy_tex);
        _render_dummy_tex = 0;
    }
    if (_render_mask_tex != 0)
    {
        glDeleteTextures(1, &_render_mask_tex);
        _render_mask_tex = 0;
    }
    _render_last_params = parameters();
    _render_last_frame = video_frame();
    xglCheckError(HERE);
}

bool video_output::render_is_compatible()
{
    return (_render_last_params.stereo_mode() == _params.stereo_mode());
}

void video_output::activate_next_frame()
{
    _active_index = (_active_index == 0 ? 1 : 0);
}

void video_output::draw_quad(float x, float y, float w, float h,
        const float tex_coords[2][4][2],
        const float more_tex_coords[4][2]) const
{
    const float (*my_tex_coords)[4][2] = (tex_coords ? tex_coords : full_tex_coords);

    glBegin(GL_QUADS);
    glTexCoord2f(my_tex_coords[0][0][0], my_tex_coords[0][0][1]);
    glMultiTexCoord2f(GL_TEXTURE1, my_tex_coords[1][0][0], my_tex_coords[1][0][1]);
    if (more_tex_coords)
    {
        glMultiTexCoord2f(GL_TEXTURE2, more_tex_coords[0][0], more_tex_coords[0][1]);
    }
    glVertex2f(x, y);
    glTexCoord2f(my_tex_coords[0][1][0], my_tex_coords[0][1][1]);
    glMultiTexCoord2f(GL_TEXTURE1, my_tex_coords[1][1][0], my_tex_coords[1][1][1]);
    if (more_tex_coords)
    {
        glMultiTexCoord2f(GL_TEXTURE2, more_tex_coords[1][0], more_tex_coords[1][1]);
    }
    glVertex2f(x + w, y);
    glTexCoord2f(my_tex_coords[0][2][0], my_tex_coords[0][2][1]);
    glMultiTexCoord2f(GL_TEXTURE1, my_tex_coords[1][2][0], my_tex_coords[1][2][1]);
    if (more_tex_coords)
    {
        glMultiTexCoord2f(GL_TEXTURE2, more_tex_coords[2][0], more_tex_coords[2][1]);
    }
    glVertex2f(x + w, y + h);
    glTexCoord2f(my_tex_coords[0][3][0], my_tex_coords[0][3][1]);
    glMultiTexCoord2f(GL_TEXTURE1, my_tex_coords[1][3][0], my_tex_coords[1][3][1]);
    if (more_tex_coords)
    {
        glMultiTexCoord2f(GL_TEXTURE2, more_tex_coords[3][0], more_tex_coords[3][1]);
    }
    glVertex2f(x, y + h);
    glEnd();
}

#if HAVE_LIBXNVCTRL
void video_output::sdi_output(int64_t display_frameno)
{
    if (!_nv_sdi_output->isInitialized())
    {
        return;
    }

    if (_nv_sdi_output->getOutputFormat() != dispatch::parameters().sdi_output_format())
    {
        _nv_sdi_output->reinit(dispatch::parameters().sdi_output_format());
    }

    glEnable(GL_TEXTURE_2D);
    for (int i = 0; i < 2; ++i)
    {
        // Render each image to specified texture of SDI output
        _nv_sdi_output->startRenderingTo(i);

        parameters::stereo_mode_t tmp_stereo_mode =
                (i == 0 ? dispatch::parameters().sdi_output_left_stereo_mode() :
                          dispatch::parameters().sdi_output_right_stereo_mode());

        display_current_frame(display_frameno, _nv_sdi_output->width(), _nv_sdi_output->height(), tmp_stereo_mode);

        _nv_sdi_output->stopRenderingTo();
        assert(xglCheckError(HERE));
    }

    assert(xglCheckError(HERE));

    // Display both textures on SDI output
    _nv_sdi_output->sendTextures();
    assert(xglCheckError(HERE));
}
#endif // HAVE_LIBXNVCTRL

void video_output::display_current_frame(
        int64_t display_frameno,
        bool keep_viewport, bool mono_right_instead_of_left,
        float x, float y, float w, float h,
        const GLint viewport[2][4],
        const float tex_coords[2][4][2],
        int dst_width, int dst_height,
        parameters::stereo_mode_t stereo_mode)
{
    clear();
    const video_frame &frame = _frame[_active_index];
    if (!frame.is_valid())
    {
        return;
    }

    /* debug: make sure the current frame is always newer than the last one
    static int64_t last_timestamp = -1000;
    int64_t timestamp = frame.presentation_time;
    assert(timestamp >= last_timestamp);
    last_timestamp = timestamp;
    */

    _params = dispatch::parameters();
    _params.set_stereo_mode(stereo_mode);
    bool context_needs_stereo = (_params.stereo_mode() == parameters::mode_stereo);
    if (context_needs_stereo != context_is_stereo())
    {
        recreate_context(context_needs_stereo);
        return;
    }
    if (!keep_viewport
            && (frame.width != _render_last_frame.width
                || frame.height != _render_last_frame.height
                || frame.aspect_ratio < _render_last_frame.aspect_ratio
                || frame.aspect_ratio > _render_last_frame.aspect_ratio
                || _render_last_params.stereo_mode() != _params.stereo_mode()
                || _render_last_params.crop_aspect_ratio() < _params.crop_aspect_ratio()
                || _render_last_params.crop_aspect_ratio() > _params.crop_aspect_ratio()
                || _render_last_params.zoom() < _params.zoom()
                || _render_last_params.zoom() > _params.zoom()
                || video_display_width() != dst_width
                || video_display_height() != dst_height))
    {
        reshape(dst_width, dst_height, _params);
    }
    assert(xglCheckError(HERE));
    if (!_render_prg || !render_is_compatible())
    {                
        render_deinit();
        render_init();
    }
    _render_last_params = _params;
    _render_last_frame = frame;

    /* Use correct left and right view indices */

    int left = 0;
    int right = (frame.stereo_layout == parameters::layout_mono ? 0 : 1);
    if (_params.stereo_mode_swap())
    {
        std::swap(left, right);
    }
    if ((_params.stereo_mode() == parameters::mode_even_odd_rows
                || _params.stereo_mode() == parameters::mode_checkerboard)
            && (pos_y() + viewport[0][1]) % 2 == 0)
    {
        std::swap(left, right);
    }
    if ((_params.stereo_mode() == parameters::mode_even_odd_columns
                || _params.stereo_mode() == parameters::mode_checkerboard)
            && (pos_x() + viewport[0][0]) % 2 == 1)
    {
        std::swap(left, right);
    }

    /* Initialize GL things */

    glEnable(GL_TEXTURE_2D);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    /* Step 3: rendering */

    // Apply fullscreen flipping/flopping
    float my_tex_coords[2][4][2];
    std::memcpy(my_tex_coords, tex_coords, sizeof(my_tex_coords));
    if (dispatch::parameters().fullscreen())
    {
        if (_params.fullscreen_flip_left())
        {
            std::swap(my_tex_coords[0][0][0], my_tex_coords[0][3][0]);
            std::swap(my_tex_coords[0][0][1], my_tex_coords[0][3][1]);
            std::swap(my_tex_coords[0][1][0], my_tex_coords[0][2][0]);
            std::swap(my_tex_coords[0][1][1], my_tex_coords[0][2][1]);
        }
        if (_params.fullscreen_flop_left())
        {
            std::swap(my_tex_coords[0][0][0], my_tex_coords[0][1][0]);
            std::swap(my_tex_coords[0][0][1], my_tex_coords[0][1][1]);
            std::swap(my_tex_coords[0][3][0], my_tex_coords[0][2][0]);
            std::swap(my_tex_coords[0][3][1], my_tex_coords[0][2][1]);
        }
        if (_params.fullscreen_flip_right())
        {
            std::swap(my_tex_coords[1][0][0], my_tex_coords[1][3][0]);
            std::swap(my_tex_coords[1][0][1], my_tex_coords[1][3][1]);
            std::swap(my_tex_coords[1][1][0], my_tex_coords[1][2][0]);
            std::swap(my_tex_coords[1][1][1], my_tex_coords[1][2][1]);
        }
        if (_params.fullscreen_flop_right())
        {
            std::swap(my_tex_coords[1][0][0], my_tex_coords[1][1][0]);
            std::swap(my_tex_coords[1][0][1], my_tex_coords[1][1][1]);
            std::swap(my_tex_coords[1][3][0], my_tex_coords[1][2][0]);
            std::swap(my_tex_coords[1][3][1], my_tex_coords[1][2][1]);
        }
    }

    // XXX We may want to re-render the subtitle here, for the case that the
    // video display area was resized or subtitle parameters were changed
    // between the call to prepare_next_frame and now (e.g. in pause mode).
    // However, parameter changes don't work well with LibASS because it does
    // not forget overrides, and we would have to introduce additional logic
    // to not rerender an old subtitle while a new subtitle was already rendered
    // by prepare_next_frame. So for now, don't rerender subtitles here, even
    // if that means that subtitle changes don't take effect in pause mode.

    glUseProgram(_render_prg);
    glUniform1f(glGetUniformLocation(_render_prg, "contrast"), _params.contrast());
    glUniform1f(glGetUniformLocation(_render_prg, "brightness"), _params.brightness());
    glUniform1f(glGetUniformLocation(_render_prg, "saturation"), _params.saturation());
    glUniform1f(glGetUniformLocation(_render_prg, "cos_hue"), std::cos(_params.hue() * M_PI));
    glUniform1f(glGetUniformLocation(_render_prg, "sin_hue"), std::sin(_params.hue() * M_PI));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _color_tex[_active_index][left]);
    if (left != right)
    {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, _color_tex[_active_index][right]);
    }
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, (_input_subtitle[_active_index].is_valid()
                ? _input_subtitle_tex[_active_index] : _render_dummy_tex));
    glUniform1i(glGetUniformLocation(_render_prg, "rgb_l"), left);
    glUniform1i(glGetUniformLocation(_render_prg, "rgb_r"), right);
    glUniform1f(glGetUniformLocation(_render_prg, "parallax"), _params.parallax() * 0.05f);
    glUniform1i(glGetUniformLocation(_render_prg, "subtitle"), 2);
    glUniform1f(glGetUniformLocation(_render_prg, "subtitle_parallax"), _params.subtitle_parallax() * 0.05f);
    if (_params.stereo_mode() != parameters::mode_red_green_monochrome
            && _params.stereo_mode() != parameters::mode_red_cyan_half_color
            && _params.stereo_mode() != parameters::mode_red_cyan_full_color
            && _params.stereo_mode() != parameters::mode_red_cyan_dubois
            && _params.stereo_mode() != parameters::mode_green_magenta_monochrome
            && _params.stereo_mode() != parameters::mode_green_magenta_half_color
            && _params.stereo_mode() != parameters::mode_green_magenta_full_color
            && _params.stereo_mode() != parameters::mode_green_magenta_dubois
            && _params.stereo_mode() != parameters::mode_amber_blue_monochrome
            && _params.stereo_mode() != parameters::mode_amber_blue_half_color
            && _params.stereo_mode() != parameters::mode_amber_blue_full_color
            && _params.stereo_mode() != parameters::mode_amber_blue_dubois
            && _params.stereo_mode() != parameters::mode_red_blue_monochrome
            && _params.stereo_mode() != parameters::mode_red_cyan_monochrome)
    {
        glUniform3f(glGetUniformLocation(_render_prg, "crosstalk"),
                _params.crosstalk_r() * _params.ghostbust(),
                _params.crosstalk_g() * _params.ghostbust(),
                _params.crosstalk_b() * _params.ghostbust());
    }
    if (_params.stereo_mode() == parameters::mode_even_odd_rows
            || _params.stereo_mode() == parameters::mode_even_odd_columns
            || _params.stereo_mode() == parameters::mode_checkerboard)
    {
        glUniform1i(glGetUniformLocation(_render_prg, "mask_tex"), 3);
        glUniform1f(glGetUniformLocation(_render_prg, "step_x"), 1.0f / static_cast<float>(viewport[0][2]));
        glUniform1f(glGetUniformLocation(_render_prg, "step_y"), 1.0f / static_cast<float>(viewport[0][3]));
    }

    glViewport(viewport[0][0], viewport[0][1], viewport[0][2], viewport[0][3]);
    if (_params.stereo_mode() == parameters::mode_stereo)
    {
        glUniform1f(glGetUniformLocation(_render_prg, "channel"), 0.0f);
        glDrawBuffer(GL_BACK_LEFT);
        draw_quad(x, y, w, h, my_tex_coords);
        glUniform1f(glGetUniformLocation(_render_prg, "channel"), 1.0f);
        glDrawBuffer(GL_BACK_RIGHT);
        draw_quad(x, y, w, h, my_tex_coords);
    }
    else if (_params.stereo_mode() == parameters::mode_even_odd_rows
            || _params.stereo_mode() == parameters::mode_even_odd_columns
            || _params.stereo_mode() == parameters::mode_checkerboard)
    {
        float vpw = static_cast<float>(viewport[0][2]);
        float vph = static_cast<float>(viewport[0][3]);
        float more_tex_coords[4][2] =
        {
            { 0.0f, 0.0f }, { vpw / 2.0f, 0.0f }, { vpw / 2.0f, vph / 2.0f }, { 0.0f, vph / 2.0f }
        };
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, _render_mask_tex);
        draw_quad(x, y, w, h, my_tex_coords, more_tex_coords);
    }
    else if (_params.stereo_mode() == parameters::mode_red_cyan_monochrome
            || _params.stereo_mode() == parameters::mode_red_cyan_half_color
            || _params.stereo_mode() == parameters::mode_red_cyan_full_color
            || _params.stereo_mode() == parameters::mode_red_cyan_dubois
            || _params.stereo_mode() == parameters::mode_green_magenta_monochrome
            || _params.stereo_mode() == parameters::mode_green_magenta_half_color
            || _params.stereo_mode() == parameters::mode_green_magenta_full_color
            || _params.stereo_mode() == parameters::mode_green_magenta_dubois
            || _params.stereo_mode() == parameters::mode_amber_blue_monochrome
            || _params.stereo_mode() == parameters::mode_amber_blue_half_color
            || _params.stereo_mode() == parameters::mode_amber_blue_full_color
            || _params.stereo_mode() == parameters::mode_amber_blue_dubois
            || _params.stereo_mode() == parameters::mode_red_green_monochrome
            || _params.stereo_mode() == parameters::mode_red_blue_monochrome)
    {
        draw_quad(x, y, w, h, my_tex_coords);
    }
    else if ((_params.stereo_mode() == parameters::mode_mono_left && !mono_right_instead_of_left)
            || (_params.stereo_mode() == parameters::mode_alternating && display_frameno % 2 == 0))
    {
        glUniform1f(glGetUniformLocation(_render_prg, "channel"), 0.0f);
        draw_quad(x, y, w, h, my_tex_coords);
    }
    else if (_params.stereo_mode() == parameters::mode_mono_right
            || (_params.stereo_mode() == parameters::mode_mono_left && mono_right_instead_of_left)
            || (_params.stereo_mode() == parameters::mode_alternating && display_frameno % 2 == 1))
    {
        glUniform1f(glGetUniformLocation(_render_prg, "channel"), 1.0f);
        draw_quad(x, y, w, h, my_tex_coords);
    }
    else if (_params.stereo_mode() == parameters::mode_left_right
            || _params.stereo_mode() == parameters::mode_left_right_half
            || _params.stereo_mode() == parameters::mode_top_bottom
            || _params.stereo_mode() == parameters::mode_top_bottom_half
            || _params.stereo_mode() == parameters::mode_hdmi_frame_pack)
    {
        glUniform1f(glGetUniformLocation(_render_prg, "channel"), 0.0f);
        draw_quad(x, y, w, h, my_tex_coords);
        glViewport(viewport[1][0], viewport[1][1], viewport[1][2], viewport[1][3]);
        glUniform1f(glGetUniformLocation(_render_prg, "channel"), 1.0f);
        draw_quad(x, y, w, h, my_tex_coords);
    }
    assert(xglCheckError(HERE));
    glUseProgram(0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
}

void video_output::clear() const
{
    assert(xglCheckError(HERE));
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
    assert(xglCheckError(HERE));
}

static void compute_viewport_and_tex_coords(int vp[4], float tc[4][2],
        float src_ar, int w, int h, float dst_w, float dst_h, float dst_ar,
        float crop_ar, float zoom)
{
    std::memcpy(tc, full_tex_coords[0], sizeof(tc));
    if (crop_ar > 0.0f)
    {
        if (src_ar >= crop_ar)
        {
            float cutoff = (1.0f - crop_ar / src_ar) / 2.0f;
            tc[0][0] += cutoff;
            tc[1][0] -= cutoff;
            tc[2][0] -= cutoff;
            tc[3][0] += cutoff;
        }
        else
        {
            float cutoff = (1.0f - src_ar / crop_ar) / 2.0f;
            tc[0][1] += cutoff;
            tc[1][1] += cutoff;
            tc[2][1] -= cutoff;
            tc[3][1] -= cutoff;
        }
        src_ar = crop_ar;
    }
    if (src_ar >= dst_ar)
    {
        // need black borders top and bottom
        float zoom_src_ar = zoom * dst_ar + (1.0f - zoom) * src_ar;
        vp[2] = dst_w;
        vp[3] = dst_ar / zoom_src_ar * dst_h;
        vp[0] = (w - vp[2]) / 2;
        vp[1] = (h - vp[3]) / 2;
        float cutoff = (1.0f - zoom_src_ar / src_ar) / 2.0f;
        tc[0][0] += cutoff;
        tc[1][0] -= cutoff;
        tc[2][0] -= cutoff;
        tc[3][0] += cutoff;
    }
    else
    {
        // need black borders left and right
        vp[2] = src_ar / dst_ar * dst_w;
        vp[3] = dst_h;
        vp[0] = (w - vp[2]) / 2;
        vp[1] = (h - vp[3]) / 2;
    }
}

void video_output::reshape(int w, int h, const parameters& params)
{
    // Clear
    _viewport[0][0] = 0;
    _viewport[0][1] = 0;
    _viewport[0][2] = w;
    _viewport[0][3] = h;
    _viewport[1][0] = 0;
    _viewport[1][1] = 0;
    _viewport[1][2] = w;
    _viewport[1][3] = h;
    std::memcpy(_tex_coords, full_tex_coords, sizeof(_tex_coords));
    glViewport(0, 0, w, h);
    clear();
    if (!_frame[_active_index].is_valid())
    {
        return;
    }

    // Compute viewport with the right aspect ratio
    if (params.stereo_mode() == parameters::mode_left_right
            || params.stereo_mode() == parameters::mode_left_right_half)
    {
        float dst_w = w / 2;
        float dst_h = h;
        float dst_ar = dst_w * screen_pixel_aspect_ratio() / dst_h;
        float src_ar = _frame[_active_index].aspect_ratio;
        float crop_ar = params.crop_aspect_ratio();
        if (params.stereo_mode() == parameters::mode_left_right_half)
        {
            src_ar /= 2.0f;
            crop_ar /= 2.0f;
        }
        compute_viewport_and_tex_coords(_viewport[0], _tex_coords[0], src_ar,
                w / 2, h, dst_w, dst_h, dst_ar,
                crop_ar, params.zoom());
        std::memcpy(_viewport[1], _viewport[0], sizeof(_viewport[1]));
        _viewport[1][0] = _viewport[0][0] + w / 2;
        std::memcpy(_tex_coords[1], _tex_coords[0], sizeof(_tex_coords[1]));
    }
    else if (params.stereo_mode() == parameters::mode_top_bottom
            || params.stereo_mode() == parameters::mode_top_bottom_half)
    {
        float dst_w = w;
        float dst_h = h / 2;
        float dst_ar = dst_w * screen_pixel_aspect_ratio() / dst_h;
        float src_ar = _frame[_active_index].aspect_ratio;
        float crop_ar = params.crop_aspect_ratio();
        if (params.stereo_mode() == parameters::mode_top_bottom_half)
        {
            src_ar *= 2.0f;
            crop_ar *= 2.0f;
        }
        compute_viewport_and_tex_coords(_viewport[0], _tex_coords[0], src_ar,
                w, h / 2, dst_w, dst_h, dst_ar,
                crop_ar, params.zoom());
        std::memcpy(_viewport[1], _viewport[0], sizeof(_viewport[1]));
        _viewport[0][1] = _viewport[1][1] + h / 2;
        std::memcpy(_tex_coords[1], _tex_coords[0], sizeof(_tex_coords[1]));
    }
    else if (params.stereo_mode() == parameters::mode_hdmi_frame_pack)
    {
        // HDMI frame packing mode has left view top, right view bottom, plus a
        // blank area separating the two. 720p uses 30 blank lines (total: 720
        // + 30 + 720 = 1470), 1080p uses 45 (total: 10280 + 45 + 1080 = 2205).
        // In both cases, the blank area is 30/1470 = 45/2205 = 1/49 of the
        // total height. See the document "High-Definition Multimedia Interface
        // Specification Version 1.4a Extraction of 3D Signaling Portion" from
        // hdmi.org.
        int blank_lines = h / 49;
        float dst_w = w;
        float dst_h = (h - blank_lines) / 2;
        float dst_ar = dst_w * screen_pixel_aspect_ratio() / dst_h;
        float src_ar = _frame[_active_index].aspect_ratio;
        compute_viewport_and_tex_coords(_viewport[0], _tex_coords[0], src_ar,
                w, (h - blank_lines) / 2, dst_w, dst_h, dst_ar,
                params.crop_aspect_ratio(), params.zoom());
        std::memcpy(_viewport[1], _viewport[0], sizeof(_viewport[1]));
        _viewport[0][1] = _viewport[1][1] + (h - blank_lines) / 2 + blank_lines;
        std::memcpy(_tex_coords[1], _tex_coords[0], sizeof(_tex_coords[1]));
    }
    else
    {
        float dst_w = w;
        float dst_h = h;
        float dst_ar = dst_w * screen_pixel_aspect_ratio() / dst_h;
        float src_ar = _frame[_active_index].aspect_ratio;
        compute_viewport_and_tex_coords(_viewport[0], _tex_coords[0], src_ar,
                w, h, dst_w, dst_h, dst_ar,
                params.crop_aspect_ratio(), params.zoom());
        std::memcpy(_viewport[1], _viewport[0], sizeof(_viewport[1]));
        std::memcpy(_tex_coords[1], _tex_coords[0], sizeof(_tex_coords[1]));
    }
}

int64_t video_output::time_to_next_frame_presentation() const
{
    return 0;
}
