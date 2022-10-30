/*
 * This file is part of Bino, a 3D video player.
 *
 * Copyright (C) 2022
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

#pragma once

#include <QVideoFrame>
#include <QImage>

class VideoFrame
{
public:
    enum StereoLayout {
        Layout_Unknown,         // unknown; needs to be guessed
        Layout_Mono,            // monoscopic video
        Layout_Top_Bottom,      // stereoscopic video, left eye top, right eye bottom
        Layout_Top_Bottom_Half, // stereoscopic video, left eye top, right eye bottom, both half height
        Layout_Bottom_Top,      // stereoscopic video, left eye bottom, right eye top
        Layout_Bottom_Top_Half, // stereoscopic video, left eye bottom, right eye top, both half height
        Layout_Left_Right,      // stereoscopic video, left eye left, right eye right
        Layout_Left_Right_Half, // stereoscopic video, left eye left, right eye right, both half width
        Layout_Right_Left,      // stereoscopic video, left eye right, right eye left
        Layout_Right_Left_Half  // stereoscopic video, left eye right, right eye left, both half width
    };
    enum ThreeSixtyMode {
        ThreeSixty_Unknown,     // unknown; needs to be guessed
        ThreeSixty_On,          // 360° video
        ThreeSixty_Off          // conventional video
    };

    // The pixel data can be represented in one of three ways:
    // 1. mapped data, via bytesPerLine and mappedBits
    // 2. copied data, via bytesPerLine and bits
    // 3. as a fallback: QImage
    // The QImage fallback is used when the frame pixel format or some other
    // of its properties cannot be handled in our texture upload / color conversion
    // pipeline. In that case, we let the frame convert itself to QImage and convert
    // that to a fallback pixel format that can always be handled.
    // The mapped data is the preferred option since it is the fastest. On single-process
    // instances, it is all we need.
    // The copied data is used to represent mapped data after it has been serialized on the
    // main process and deserialized on a child process.
    enum Storage {
        Storage_Mapped, // mapped data
        Storage_Copied, // copied data
        Storage_Image   // QImage
    };

    enum YUVSpace {
        // see shader-color.frag.glsl
        YUV_BT601 = 1,
        YUV_BT709 = 2,
        YUV_AdobeRgb = 3,
        YUV_BT2020 = 4
    };

    /* This is a shallow copy of the original QVideoFrame: */
    QVideoFrame qframe;
    /* The stereo layout of this frame: */
    StereoLayout stereoLayout;
    /* Whether this is a 360° video: */
    ThreeSixtyMode threeSixtyMode;
    /* The subtitle: */
    QString subtitle;
    /* The following can mirror the data of QVideoFrame: */
    int width;
    int height;
    float aspectRatio;
    enum Storage storage;
    // for mapped and copied data:
    QVideoFrameFormat::PixelFormat pixelFormat;
    bool yuvValueRangeSmall;
    enum YUVSpace yuvSpace;
    int planeCount; // 1-3
    int bytesPerLine[3];
    int bytesPerPlane[3];
    // for mapped data:
    uchar* mappedBits[3];
    // for copied data:
    std::vector<uchar> bits[3];
    // for QImage data:
    QImage image;

    VideoFrame();

    void update(StereoLayout sl, ThreeSixtyMode ts, const QVideoFrame& frame, bool newSrc);
    static const char* layoutToString(StereoLayout sl);
    static const char* modeToString(ThreeSixtyMode ts);
};

QDataStream &operator<<(QDataStream& ds, const VideoFrame& frame);
QDataStream &operator>>(QDataStream& ds, VideoFrame& frame);
