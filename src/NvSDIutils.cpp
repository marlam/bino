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

#include "config.h"

#include "NvSDIutils.h"

#include <GL/glx.h>

#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "NVCtrlLib.h"
#include "NVCtrl.h"

#include "msg.h"
#include "dbg.h"

int
ScanHW (Display * dpy, HGPUNV * gpuList)
{
    HGPUNV gpuDevice;
    int num_gpus ;
    int gpu ;
    int *pData;
    int len ;
    char *str;
    bool ret;

    /* Get the number of gpus in the system */
    ret = XNVCTRLQueryTargetCount (dpy, NV_CTRL_TARGET_TYPE_GPU, &num_gpus);
    if (!ret) {
        msg::err("Failed to query number of gpus");
        return 1;
    }
    msg::inf("Number of GPUs: %d", num_gpus);
    int num_gpusWithXScreen = 0;

    for (gpu = 0; gpu < num_gpus; gpu++) {
        msg::inf("GPU %d information:", gpu);
        /* GPU name */
        ret = XNVCTRLQueryTargetStringAttribute (dpy, NV_CTRL_TARGET_TYPE_GPU, gpu,	// target_id
                0,	// display_mask
                NV_CTRL_STRING_PRODUCT_NAME,
                &str);
        if (!ret) {
            msg::err("Failed to query gpu product name");
            return 1;
        }
        msg::inf("Product Name                    : %s", str);
        /* X Screens driven by this GPU */
        ret = XNVCTRLQueryTargetBinaryData (dpy, NV_CTRL_TARGET_TYPE_GPU, gpu,	// target_id
                0,	// display_mask
                NV_CTRL_BINARY_DATA_XSCREENS_USING_GPU,
                (unsigned char **) &pData, &len);
        if (!ret) {
            msg::err("Failed to query list of X Screens");
            return 1;
        }
        msg::inf("Number of X Screens on GPU %d    : %d", gpu, pData[0]);
        //only return GPUs that have XScreens
        if (pData[0]) {
            gpuDevice.deviceXScreen = pData[1];	//chose the first screen
            strcpy (gpuDevice.deviceName, str);
            gpuList[gpu] = gpuDevice;
            num_gpusWithXScreen++;
        }
        XFree (pData);
    }
    return num_gpusWithXScreen;
}

//
// Calculate fps
//
GLfloat
CalcFPS ()
{
    static int t0 = -1;

    static int count = 0;

    struct timeval tv;

    struct timezone tz;

    static GLfloat __fps = 0.0;

    int t;

    gettimeofday (&tv, &tz);
    t = (int) tv.tv_sec;
    if (t0 < 0)
        t0 = t;
    count++;
    if (t - t0 >= 5.0) {
        GLfloat seconds = t - t0;

        __fps = count / seconds;
        t0 = t;
        count = 0;
    }
    return (__fps);
}

//
// Decode SDI input value returned.
//
const char *
decodeSDISyncInputDetected (int _value)
{
    switch (_value) {
    case NV_CTRL_GVO_SDI_SYNC_INPUT_DETECTED_HD:
        return ("NV_CTRL_GVO_SDI_SYNC_INPUT_DETECTED_HD");
        break;
    case NV_CTRL_GVO_SDI_SYNC_INPUT_DETECTED_SD:
        return ("NV_CTRL_GVO_SDI_SYNC_INPUT_DETECTED_SD");
        break;
    case NV_CTRL_GVO_SDI_SYNC_INPUT_DETECTED_NONE:
    default:
        return ("NV_CTRL_GVO_SDI_SYNC_INPUT_DETECTED_NONE");
        break;
    }				// switch
}

//
// Decode provided signal format.
//
const char *
decodeSignalFormat (int _value)
{
    switch (_value) {
    case NV_CTRL_GVIO_VIDEO_FORMAT_487I_59_94_SMPTE259_NTSC:
        return ("480i 59.94Hz (SMPTE259 - NTSC)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_576I_50_00_SMPTE259_PAL:
        return ("576i 50.00Hz (SMPTE259 - PAL)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_720P_59_94_SMPTE296:
        return ("720p 59.94Hz (SMPTE296)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_720P_60_00_SMPTE296:
        return ("720p 60.00Hz (SMPTE296)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_1035I_59_94_SMPTE260:
        return ("1035i 59.94Hz (SMPTE260)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_1035I_60_00_SMPTE260:
        return ("1035i 60.00Hz (SMPTE260)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_1080I_50_00_SMPTE295:
        return ("1080i 50.00Hz (SMPTE295)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_1080I_50_00_SMPTE274:
        return ("1080i 50.00Hz (SMPTE274)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_1080I_59_94_SMPTE274:
        return ("1080i 59.94Hz (SMPTE274)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_1080I_60_00_SMPTE274:
        return ("1080i 60.00Hz (SMPTE274)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_1080P_23_976_SMPTE274:
        return ("1080p 23.976Hz (SMPTE274)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_1080P_24_00_SMPTE274:
        return ("1080p 24.00Hz (SMPTE274)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_1080P_25_00_SMPTE274:
        return ("1080p 25.00Hz (SMPTE274)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_1080P_29_97_SMPTE274:
        return ("1080p 29.97Hz (SMPTE274)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_1080P_30_00_SMPTE274:
        return ("1080p 30.00Hz (SMPTE274)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_720P_50_00_SMPTE296:
        return ("720p 50.00Hz (SMPTE296)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_1080I_48_00_SMPTE274:
        return ("1080i 48.00Hz (SMPTE274)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_1080I_47_96_SMPTE274:
        return ("1080i 47.96Hz (SMPTE274)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_720P_30_00_SMPTE296:
        return ("720p 30.00Hz (SMPTE296)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_720P_29_97_SMPTE296:
        return ("720p 29.97Hz (SMPTE296)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_720P_25_00_SMPTE296:
        return ("720p 25.00Hz (SMPTE296)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_720P_24_00_SMPTE296:
        return ("720p 24.00Hz (SMPTE296)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_720P_23_98_SMPTE296:
        return ("720p 23.98Hz (SMPTE296)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_1080PSF_25_00_SMPTE274:
        return ("1080PsF 25.00Hz (SMPTE274)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_1080PSF_29_97_SMPTE274:
        return ("1080PsF 29.97Hz (SMPTE274)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_1080PSF_30_00_SMPTE274:
        return ("1080PsF 30.00Hz (SMPTE274)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_1080PSF_24_00_SMPTE274:
        return ("1080PsF 24.00Hz (SMPTE274)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_1080PSF_23_98_SMPTE274:
        return ("1080PsF 23.98Hz (SMPTE274)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_2048P_30_00_SMPTE372:
        return ("2048p 30.00Hz (SMPTE372)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_2048P_29_97_SMPTE372:
        return ("2048p 29.97Hz (SMPTE372)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_2048I_60_00_SMPTE372:
        return ("2048i 60.00Hz (SMPTE372)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_2048I_59_94_SMPTE372:
        return ("2048i 59.94Hz (SMPTE372)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_2048P_25_00_SMPTE372:
        return ("2048p 25.00Hz (SMPTE372)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_2048I_50_00_SMPTE372:
        return ("2048i 50.00Hz (SMPTE372)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_2048P_24_00_SMPTE372:
        return ("2048p 24.00Hz (SMPTE372)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_2048P_23_98_SMPTE372:
        return ("2048p 23.98Hz (SMPTE372)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_2048I_48_00_SMPTE372:
        return ("2048i 48.00Hz (SMPTE372)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_2048I_47_96_SMPTE372:
        return ("2048i 47.96Hz (SMPTE372)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_1080P_50_00_3G_LEVEL_A_SMPTE274:
        return ("1080p 50.00Hz (3G Level A - SMPTE274)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_1080P_59_94_3G_LEVEL_A_SMPTE274:
        return ("1080p 59.94Hz (3G Level A - SMPTE274)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_1080P_60_00_3G_LEVEL_A_SMPTE274:
        return ("1080p 60.00Hz (3G Level A - SMPTE274)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_1080P_60_00_3G_LEVEL_B_SMPTE274:
        return ("1080p 60.00Hz (3G Level B - SMPTE274)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_1080I_60_00_3G_LEVEL_B_SMPTE274:
        return ("1080i 60.00Hz (3G Level B - SMPTE274)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_2048I_60_00_3G_LEVEL_B_SMPTE372:
        return ("2048i 60.00Hz (3G Level B - SMPTE372)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_1080P_50_00_3G_LEVEL_B_SMPTE274:
        return ("1080p 50.00Hz (3G Level B - SMPTE274)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_1080I_50_00_3G_LEVEL_B_SMPTE274:
        return ("1080i 50.00Hz (3G Level B - SMPTE274)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_2048I_50_00_3G_LEVEL_B_SMPTE372:
        return ("2048i 50.00Hz (3G Level B - SMPTE372)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_1080P_30_00_3G_LEVEL_B_SMPTE274:
        return ("1080p 30.00Hz (3G Level B - SMPTE274)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_2048P_30_00_3G_LEVEL_B_SMPTE372:
        return ("2048p 30.00Hz (3G Level B - SMPTE372)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_1080P_25_00_3G_LEVEL_B_SMPTE274:
        return ("1080p 25.00Hz (3G Level B - SMPTE274)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_2048P_25_00_3G_LEVEL_B_SMPTE372:
        return ("2048p 25.00Hz (3G Level B - SMPTE372)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_1080P_24_00_3G_LEVEL_B_SMPTE274:
        return ("1080p 24.00Hz (3G Level B - SMPTE274)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_2048P_24_00_3G_LEVEL_B_SMPTE372:
        return ("2048p 24.00Hz (3G Level B - SMPTE372)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_1080I_48_00_3G_LEVEL_B_SMPTE274:
        return ("1080i 48.00Hz (3G Level B - SMPTE274)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_2048I_48_00_3G_LEVEL_B_SMPTE372:
        return ("2048i 48.00Hz (3G Level B - SMPTE372)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_1080P_59_94_3G_LEVEL_B_SMPTE274:
        return ("1080p 59.94Hz (3G Level B - SMPTE274)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_1080I_59_94_3G_LEVEL_B_SMPTE274:
        return ("1080i 59.94Hz (3G Level B - SMPTE274)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_2048I_59_94_3G_LEVEL_B_SMPTE372:
        return ("2048i 59.94Hz (3G Level B - SMPTE372)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_1080P_29_97_3G_LEVEL_B_SMPTE274:
        return ("1080p 29.97Hz (3G Level B - SMPTE274)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_2048P_29_97_3G_LEVEL_B_SMPTE372:
        return ("2048p 29.97Hz (3G Level B - SMPTE372)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_1080P_23_98_3G_LEVEL_B_SMPTE274:
        return ("1080p 23.98Hz (3G Level B - SMPTE274)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_2048P_23_98_3G_LEVEL_B_SMPTE372:
        return ("2048p 23.98Hz (3G Level B - SMPTE372)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_1080I_47_96_3G_LEVEL_B_SMPTE274:
        return ("1080i 47.96Hz (3G Level B - SMPTE274)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_2048I_47_96_3G_LEVEL_B_SMPTE372:
        return ("2048i 47.96Hz (3G Level B - SMPTE372)");
        break;
    case NV_CTRL_GVIO_VIDEO_FORMAT_NONE:
    default:
        return ("None");
        break;
    }				// switch
}

//
// Decode provided component sampling.
//
const char *
decodeComponentSampling (int _value)
{
    switch (_value) {
    case NV_CTRL_GVI_COMPONENT_SAMPLING_UNKNOWN:
        return ("NV_CTRL_GVI_COMPONENT_SAMPLING_UNKNOWN");
        break;
    case NV_CTRL_GVI_COMPONENT_SAMPLING_4444:
        return ("NV_CTRL_GVI_COMPONENT_SAMPLING_4444");
        break;
    case NV_CTRL_GVI_COMPONENT_SAMPLING_4224:
        return ("NV_CTRL_GVI_COMPONENT_SAMPLING_4224");
        break;
    case NV_CTRL_GVI_COMPONENT_SAMPLING_444:
        return ("NV_CTRL_GVI_COMPONENT_SAMPLING_444");
        break;
    case NV_CTRL_GVI_COMPONENT_SAMPLING_422:
        return ("NV_CTRL_GVI_COMPONENT_SAMPLING_422");
        break;
    default:
        return ("NV_CTRL_GVI_COMPONENT_SAMPLING_UNKNOWN");
        break;
    }				// switch
}

//
// Decode provided color space
//
const char *
decodeColorSpace (int _value)
{
    switch (_value) {
    case NV_CTRL_GVI_COLOR_SPACE_GBR:
        return ("NV_CTRL_GVI_COLOR_SPACE_GBR");
        break;
    case NV_CTRL_GVI_COLOR_SPACE_GBRA:
        return ("NV_CTRL_GVI_COLOR_SPACE_GBRA");
        break;
    case NV_CTRL_GVI_COLOR_SPACE_GBRD:
        return ("NV_CTRL_GVI_COLOR_SPACE_GBRD");
        break;
    case NV_CTRL_GVI_COLOR_SPACE_YCBCR:
        return ("NV_CTRL_GVI_COLOR_SPACE_YCBCR");
        break;
    case NV_CTRL_GVI_COLOR_SPACE_YCBCRA:
        return ("NV_CTRL_GVI_COLOR_SPACE_YCBCRA");
        break;
    case NV_CTRL_GVI_COLOR_SPACE_YCBCRD:
        return ("NV_CTRL_GVI_COLOR_SPACE_YCBCRD");
        break;
    case NV_CTRL_GVI_COLOR_SPACE_UNKNOWN:
    default:
        return ("NV_CTRL_GVI_COLOR_SPACE_UNKNOWN");
        break;
    }				// switch
}

//
// Decode bits per component
//
const char *
decodeBitsPerComponent (int _value)
{
    switch (_value) {
    case NV_CTRL_GVI_BITS_PER_COMPONENT_UNKNOWN:
        return ("NV_CTRL_GVI_BITS_PER_COMPONENT_UNKNOWN");
        break;
    case NV_CTRL_GVI_BITS_PER_COMPONENT_8:
        return ("NV_CTRL_GVI_BITS_PER_COMPONENT_8");
        break;
    case NV_CTRL_GVI_BITS_PER_COMPONENT_10:
        return ("NV_CTRL_GVI_BITS_PER_COMPONENT_10");
        break;
    case NV_CTRL_GVI_BITS_PER_COMPONENT_12:
        return ("NV_CTRL_GVI_BITS_PER_COMPONENT_12");
        break;
    default:
        return ("NV_CTRL_GVI_BITS_PER_COMPONENT_UNKNOWN");
        break;
    }				// switch
}

//
// Decode chroma expand
//
const char *
decodeChromaExpand (int _value)
{
    switch (_value) {
    case NV_CTRL_GVI_CHROMA_EXPAND_FALSE:
        return ("NV_CTRL_GVI_CHROMA_EXPAND_FALSE");
        break;
    case NV_CTRL_GVI_CHROMA_EXPAND_TRUE:
        return ("NV_CTRL_GVI_CHROMA_EXPAND_TRUE");
        break;
    default:
        return ("NV_CTRL_GVI_CHROMA_EXPAND_UNKNOWN");
        break;
    }				// switch
}
