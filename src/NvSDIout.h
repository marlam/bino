/*
 * Copyright (C) 2012
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

#ifndef NVSDIOUT_H
#define NVSDIOUT_H

#include <GL/glew.h>

#if defined __cplusplus
extern "C" {
#endif

typedef struct _XDisplay Display;

typedef struct {
  int video_format; 
  int data_format;
  int sync_mode;
  int sync_source;
  //int fsaa;
  int fql;
  int xscreen;
} OutputOptions;

class CNvSDIout
{
private:
    unsigned int* (*myGLXEnumerateVideoDevicesNV)(Display *dpy, int screen, int *nelements);
    int (*myGLXBindVideoDeviceNV)(Display *dpy, unsigned int video_slot, unsigned int video_device, const int *attrib_list);

	//X stuff
	Display *_display;          // Display	

	int m_videoWidth;				// Video format resolution in pixels
	int m_videoHeight;				// Video format resolution in lines

	OutputOptions m_outputOptions;

	bool m_bInitialized;

    GLuint _sdi_fbo;                    // framebuffer object to render into the sRGB texture
    GLuint _sdi_tex[2];                 // output: SRGB8 or linear RGB16 texture


public:

	CNvSDIout();
	~CNvSDIout();

	void init(int videoFormat);
	void deinit();
	void reinit(int videoFormat);

	void sendTextures();

	inline bool isInitialized() {return m_bInitialized; }

	void startRenderingTo(int textureIndex);
	void stopRenderingTo();

    int width() { return m_videoWidth; }
    int height() { return m_videoHeight; }

    int getOutputFormat() { return m_outputOptions.video_format; }

private:

	void setOutputOptions(Display *display, const OutputOptions &outputOptions);
	bool initOutputDeviceNVCtrl();
	bool destroyOutputDeviceNVCtrl();
};

#if defined __cplusplus
} /* extern "C" */
#endif

#endif
