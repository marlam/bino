/*
 * This file is part of Bino, a 3D video player.
 *
 * Copyright (C) 2022, 2023, 2024
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

#include <QtCore>
#include <QVideoFrame>
#include <QImage>

#include "modes.hpp"


class VideoFrame
{
Q_DECLARE_TR_FUNCTIONS(VideoFrame)

public:
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
    /* The input mode of this frame: */
    InputMode inputMode;
    /* The surround mode of this frame: */
    SurroundMode surroundMode;
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

    bool isValid() const;
    void update(InputMode im, SurroundMode ts, const QVideoFrame& frame, bool newSrc);
    void reUpdate();
    void invalidate();
};

QDataStream &operator<<(QDataStream& ds, const VideoFrame& frame);
QDataStream &operator>>(QDataStream& ds, VideoFrame& frame);
