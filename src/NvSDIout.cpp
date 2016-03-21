/*
 * Copyright (C) 2012, 2015
 * Binocle <http://binocle.com/>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Written by Olivier Letz <oletz@binocle.com>.
 */

#include "config.h"

#define GL_GLEXT_PROTOTYPES
#define GLX_GLXEXT_PROTOTYPES
#include "NvSDIout.h"

#include <GL/glx.h>

#include <NVCtrl/NVCtrlLib.h>

#include "base/msg.h"
#include "base/dbg.h"

#include "base/gettext.h"
#define _(string) gettext(string)

#include "NvSDIutils.h"

#define MAX_GPUS 4

#if defined __cplusplus
extern "C" {
#endif


CNvSDIout::CNvSDIout() : m_bInitialized(false)
{
    for (int i = 0; i < 2; i++)
    {
        _sdi_tex[i] = 0;
    }
    _sdi_fbo = 0;
}

CNvSDIout::~CNvSDIout()
{


}

void CNvSDIout::init(int videoFormat) {

    assert(!m_bInitialized);

    // Open X display
    Display *dpy = glXGetCurrentDisplay();

    //scan the systems for GPUs
    HGPUNV gpuList[MAX_GPUS];
    int num_gpus = ScanHW(dpy, gpuList);

    if (num_gpus < 1) {
        msg::err("No GPU available for sdi output");
        return;
    }

    //grab the first GPU for now for DVP
    HGPUNV *gpu = &gpuList[0];

    OutputOptions outputOptions;

    //Set the output to be the same frame rate as the input here
    outputOptions.video_format = videoFormat;
    outputOptions.xscreen = gpu->deviceXScreen;
    outputOptions.data_format = NV_CTRL_GVO_DATA_FORMAT_DUAL_R8G8B8_TO_DUAL_YCRCB422;
    outputOptions.fql = 5;
    outputOptions.sync_source = NV_CTRL_GVO_SYNC_SOURCE_SDI;
    outputOptions.sync_mode = NV_CTRL_GVO_SYNC_MODE_FREE_RUNNING;

    setOutputOptions(dpy, outputOptions);
    bool ret = initOutputDeviceNVCtrl();
    if (!ret) {
        msg::err("Unable to init sdi output");
        return;
    }

    // Get GLX functions
    myGLXEnumerateVideoDevicesNV = reinterpret_cast<unsigned int* (*)(Display*, int, int*)>(
            glXGetProcAddress(reinterpret_cast<const GLubyte*>("glXEnumerateVideoDevicesNV")));
    myGLXBindVideoDeviceNV = reinterpret_cast<int (*)(Display*, unsigned int, unsigned int, const int*)>(
            glXGetProcAddress(reinterpret_cast<const GLubyte*>("glXBindVideoDeviceNV")));
    if (!myGLXEnumerateVideoDevicesNV || !myGLXBindVideoDeviceNV) {
        msg::err("Error: cannot get GLX functions");
        return;
    }

    // Enumerate available video devices.
    unsigned int *videoDevices;
    int numDevices;

    videoDevices = myGLXEnumerateVideoDevicesNV(dpy, outputOptions.xscreen, &numDevices);
    if (!videoDevices || numDevices <= 0) {
        XFree(videoDevices);
        msg::err("Error: could not enumerate video devices");
        return;
    }

    msg::inf("Number of sdi devices: %d\n", numDevices);

    // Bind first video device
    int retCode = myGLXBindVideoDeviceNV(dpy, 1, videoDevices[0], NULL);
    if (retCode != Success) {
        XFree(videoDevices);
        msg::wrn("Error: could not bind SDI video device");
        return;
    }
    // Free list of available video devices, don't need it anymore.
    XFree(videoDevices);


    glGenFramebuffersEXT(1, &_sdi_fbo);
    assert(glGetError() == GL_NO_ERROR);

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, _sdi_fbo);
    assert(glGetError() == GL_NO_ERROR);

    for (int i = 0; i < 2; i++)
    {
        glGenTextures(1, &(_sdi_tex[i]));
        glBindTexture(GL_TEXTURE_2D, _sdi_tex[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16,
                m_videoWidth, m_videoHeight, 0, GL_RGB, GL_SHORT, NULL);

        glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,
                GL_COLOR_ATTACHMENT0_EXT+i, GL_TEXTURE_2D, _sdi_tex[i], 0);
        assert(glGetError() == GL_NO_ERROR);
    }

    GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
    if (status != status == GL_FRAMEBUFFER_COMPLETE_EXT) {
        switch (status) {
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT:
            printf("FBO: Error: Framebuffer incomplete, incomplete attachment!\n");
            break;
        case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
            printf("FBO: Error: Unsupported framebuffer format!\n");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT:
            printf("FBO: Error: Framebuffer incomplete, missing attachment!\n");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
            printf("FBO: Error: Framebuffer incomplete, attached images must have same dimensions!\n");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT:
            printf("FBO: Error: Framebuffer incomplete, attached images must have same format!\n");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT:
            printf("FBO: Error: Framebuffer incomplete, missing draw buffer!\n");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT:
            printf("FBO: Error: Framebuffer incomplete, missing read buffer!\n");
            break;
        default:
            printf("FBO: Unknown error 0x%X (see glext.h)!\n", status);
            break;
        }
    }

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    assert(glGetError() == GL_NO_ERROR);

    m_bInitialized = true;
}

void CNvSDIout::deinit() {
    m_bInitialized = false;

    glDeleteFramebuffersEXT(1, &_sdi_fbo);
    _sdi_fbo = 0;

    for (int i = 0; i < 2; i++)
    {
        if (_sdi_tex[i] != 0)
        {
            glDeleteTextures(1, &(_sdi_tex[i]));
            _sdi_tex[i] = 0;
        }
    }

    if (Success != myGLXBindVideoDeviceNV(_display, 1, 0, NULL))
    {
        msg::wrn("Error: could not release video device");
    }
    destroyOutputDeviceNVCtrl();
}

void CNvSDIout::reinit(int videoFormat) {
    deinit();
    init(videoFormat);
}

void CNvSDIout::setOutputOptions(Display *display, const OutputOptions &outputOptions)
{
    _display = display;
    m_outputOptions = outputOptions;
}

bool CNvSDIout::initOutputDeviceNVCtrl()
{
    int value;

    // Query NV_CTRL_GVO support.
    if (!XNVCTRLQueryAttribute(_display, m_outputOptions.xscreen, 0,
            NV_CTRL_GVO_SUPPORTED, &value)) {
        return false;
    }

    // Set video format
    XNVCTRLSetAttribute(_display, m_outputOptions.xscreen, 0,
            NV_CTRL_GVO_OUTPUT_VIDEO_FORMAT, m_outputOptions.video_format);

    // Set data format format.
    XNVCTRLSetAttribute(_display, m_outputOptions.xscreen, 0,
            NV_CTRL_GVO_DATA_FORMAT,
            m_outputOptions.data_format);
    // Set sync mode
    XNVCTRLSetAttribute(_display, m_outputOptions.xscreen, 0,
            NV_CTRL_GVO_SYNC_MODE, m_outputOptions.sync_mode);

    // Set sync source
    XNVCTRLSetAttribute(_display, m_outputOptions.xscreen, 0,
            NV_CTRL_GVO_SYNC_SOURCE, m_outputOptions.sync_source);
    // Set flip queue length
    XNVCTRLSetAttribute(_display, m_outputOptions.xscreen, 0,
            NV_CTRL_GVO_FLIP_QUEUE_SIZE, m_outputOptions.fql);

    // Enable full-range color output ([4-1019] instead of [64-940])
    XNVCTRLSetAttribute(_display, m_outputOptions.xscreen, 0, NV_CTRL_GVO_FULL_RANGE_COLOR,
            NV_CTRL_GVO_FULL_RANGE_COLOR_ENABLED);

    XNVCTRLQueryAttribute (_display,
            m_outputOptions.xscreen,
            m_outputOptions.video_format,
            NV_CTRL_GVIO_VIDEO_FORMAT_WIDTH,
            &m_videoWidth);
    XNVCTRLQueryAttribute (_display,
            m_outputOptions.xscreen,
            m_outputOptions.video_format,
            NV_CTRL_GVIO_VIDEO_FORMAT_HEIGHT,
            &m_videoHeight);

    XFlush(_display);
    return true;
}

bool CNvSDIout::destroyOutputDeviceNVCtrl()
{
    return true;
}


void CNvSDIout::sendTextures()
{
    assert(glGetError() == GL_NO_ERROR);
    assert(m_bInitialized);
    glPresentFrameDualFillNV(1, 0, 0, 0, GL_FRAME_NV,
            GL_TEXTURE_2D, _sdi_tex[0],
            GL_NONE, 0,
            GL_TEXTURE_2D, _sdi_tex[1],
            GL_NONE, 0);
    assert(glGetError() == GL_NO_ERROR);
}

void CNvSDIout::startRenderingTo(int textureIndex)
{
    assert(m_bInitialized);
    assert(glGetError() == GL_NO_ERROR);

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, _sdi_fbo);
    assert(glGetError() == GL_NO_ERROR);

    glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT + textureIndex);
    assert(glGetError() == GL_NO_ERROR);
}

void CNvSDIout::stopRenderingTo()
{
    assert(m_bInitialized);

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    assert(glGetError() == GL_NO_ERROR);
}

#if defined __cplusplus
} /* extern "C" */
#endif
