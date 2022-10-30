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

#include "videoframe.hpp"
#include "log.hpp"


VideoFrame::VideoFrame()
{
    update(Layout_Unknown, ThreeSixty_Unknown, QVideoFrame(), false);
}

const char* VideoFrame::layoutToString(StereoLayout sl)
{
    switch (sl)
    {
    case Layout_Unknown:
        return "unknown";
        break;
    case Layout_Mono:
        return "mono";
        break;
    case Layout_Top_Bottom:
        return "top-bottom";
        break;
    case Layout_Top_Bottom_Half:
        return "top-bottom-half";
        break;
    case Layout_Bottom_Top:
        return "bottom-top";
        break;
    case Layout_Bottom_Top_Half:
        return "bottom-top-half";
        break;
    case Layout_Left_Right:
        return "left-right";
        break;
    case Layout_Left_Right_Half:
        return "left-right-half";
        break;
    case Layout_Right_Left:
        return "right-left";
        break;
    case Layout_Right_Left_Half:
        return "right-left-half";
        break;
    };
    return nullptr;
}

const char* VideoFrame::modeToString(ThreeSixtyMode ts)
{
    switch (ts)
    {
    case ThreeSixty_Unknown:
        return "unknown";
        break;
    case ThreeSixty_On:
        return "on";
        break;
    case ThreeSixty_Off:
        return "Off";
        break;
    }
    return nullptr;
}

void VideoFrame::update(StereoLayout sl, ThreeSixtyMode ts, const QVideoFrame& frame, bool newSrc)
{
    if (qframe.isMapped())
        qframe.unmap();
    qframe = frame;

    bool valid = (qframe.isValid() && qframe.pixelFormat() != QVideoFrameFormat::Format_Invalid);
    if (valid) {
        subtitle = qframe.subtitleText();
        width = qframe.width();
        height = qframe.height();
        aspectRatio = float(width) / height;
        if (sl == Layout_Unknown) {
            if (aspectRatio > 3.0f)
                sl = VideoFrame::Layout_Left_Right;
            else if (aspectRatio < 1.0f)
                sl = VideoFrame::Layout_Top_Bottom;
            else
                sl = VideoFrame::Layout_Mono;
            LOG_FIREHOSE("guessing stereo layout from aspect ratio %g: %s", aspectRatio, layoutToString(sl));
        }
        stereoLayout = sl;
        if (ts == ThreeSixty_Unknown) {
            if (width == height && (stereoLayout == Layout_Top_Bottom || stereoLayout == Layout_Bottom_Top))
                ts = ThreeSixty_On;
            else if (width == 2 * height && stereoLayout == Layout_Mono)
                ts = ThreeSixty_On;
            else if (width == 2 * height && (stereoLayout == Layout_Top_Bottom_Half || stereoLayout == Layout_Bottom_Top_Half))
                ts = ThreeSixty_On;
            else if (width == 2 * height && (stereoLayout == Layout_Left_Right_Half || stereoLayout == Layout_Right_Left_Half))
                ts = ThreeSixty_On;
            else if (width == 4 * height && (stereoLayout == Layout_Left_Right || stereoLayout == Layout_Right_Left))
                ts = ThreeSixty_On;
            else
                ts = ThreeSixty_Off;
            LOG_FIREHOSE("guessing 360°=%s from frame size", (ts == ThreeSixty_On ? "on" : "off"));
        }
        threeSixtyMode = ts;
        bool fallbackToImage = true;
        LOG_FIREHOSE("new frame with pixel format %s",
                qPrintable(QVideoFrameFormat::pixelFormatToString(qframe.pixelFormat())));
        if (       qframe.pixelFormat() == QVideoFrameFormat::Format_ARGB8888
                || qframe.pixelFormat() == QVideoFrameFormat::Format_ARGB8888_Premultiplied
                || qframe.pixelFormat() == QVideoFrameFormat::Format_XRGB8888
                || qframe.pixelFormat() == QVideoFrameFormat::Format_BGRA8888
                || qframe.pixelFormat() == QVideoFrameFormat::Format_BGRA8888_Premultiplied
                || qframe.pixelFormat() == QVideoFrameFormat::Format_BGRX8888
                || qframe.pixelFormat() == QVideoFrameFormat::Format_ABGR8888
                || qframe.pixelFormat() == QVideoFrameFormat::Format_XBGR8888
                || qframe.pixelFormat() == QVideoFrameFormat::Format_RGBA8888
                || qframe.pixelFormat() == QVideoFrameFormat::Format_RGBX8888
                || qframe.pixelFormat() == QVideoFrameFormat::Format_YUV420P
                || qframe.pixelFormat() == QVideoFrameFormat::Format_YUV422P
                || qframe.pixelFormat() == QVideoFrameFormat::Format_YV12
                || qframe.pixelFormat() == QVideoFrameFormat::Format_NV12
                || qframe.pixelFormat() == QVideoFrameFormat::Format_P010
                || qframe.pixelFormat() == QVideoFrameFormat::Format_P016) {
            fallbackToImage = false;
        }
        if (fallbackToImage) {
            if (newSrc) {
                LOG_WARNING("pixel format %s is not hardware accelerated!",
                        qPrintable(QVideoFrameFormat::pixelFormatToString(qframe.pixelFormat())));
            }
            storage = Storage_Image;
            pixelFormat = QVideoFrameFormat::pixelFormatFromImageFormat(QImage::Format_RGB32);
            yuvValueRangeSmall = false;
            yuvSpace = YUV_AdobeRgb;
            image = qframe.toImage();
            image.convertTo(QImage::Format_RGB32);
        } else {
            storage = Storage_Mapped;
            pixelFormat = qframe.pixelFormat();
            // TODO: update this for Qt6.4 (and require Qt6.4!) once
            // that version is in Debian unstable
            switch (qframe.surfaceFormat().yCbCrColorSpace()) {
            case QVideoFrameFormat::YCbCr_Undefined:
            case QVideoFrameFormat::YCbCr_BT601:
                yuvValueRangeSmall = true;
                yuvSpace = YUV_BT601;
                break;
            case QVideoFrameFormat::YCbCr_BT709:
                yuvValueRangeSmall = true;
                yuvSpace = YUV_BT709;
                break;
            case QVideoFrameFormat::YCbCr_xvYCC601:
                yuvValueRangeSmall = false;
                yuvSpace = YUV_BT601;
                break;
            case QVideoFrameFormat::YCbCr_xvYCC709:
                yuvValueRangeSmall = false;
                yuvSpace = YUV_BT709;
                break;
            case QVideoFrameFormat::YCbCr_JPEG:
                yuvValueRangeSmall = false;
                yuvSpace = YUV_AdobeRgb;
                break;
            case QVideoFrameFormat::YCbCr_BT2020:
                yuvValueRangeSmall = true;
                yuvSpace = YUV_AdobeRgb;
                break;
            }
            qframe.map(QVideoFrame::ReadOnly);
            planeCount = qframe.planeCount();
            for (int p = 0; p < planeCount; p++) {
                bytesPerLine[p] = qframe.bytesPerLine(p);
                bytesPerPlane[p] = qframe.mappedBytes(p);
                mappedBits[p] = qframe.bits(p);
            }
        }
        subtitle = qframe.subtitleText();
        subtitle.replace(QLatin1Char('\n'), QChar::LineSeparator); // qvideoframe.cpp does this
    } else {
        // Synthesize a black frame
        stereoLayout = Layout_Mono;
        threeSixtyMode = ThreeSixty_Off;
        subtitle = QString();
        width = 1;
        height = 1;
        storage = Storage_Image;
        image = QImage(width, height, QImage::Format_RGB32);
        image.fill(0);
        aspectRatio = 1.0f;
        subtitle = QString();
    }
}

QDataStream &operator<<(QDataStream& ds, const VideoFrame& f)
{
    ds << static_cast<int>(f.stereoLayout);
    ds << static_cast<int>(f.threeSixtyMode);
    ds << f.subtitle;
    ds << f.width;
    ds << f.height;
    ds << f.aspectRatio;
    switch (f.storage) {
    case VideoFrame::Storage_Mapped:
    case VideoFrame::Storage_Copied:
        ds << static_cast<int>(VideoFrame::Storage_Copied);
        ds << static_cast<int>(f.pixelFormat);
        ds << f.yuvValueRangeSmall;
        ds << static_cast<int>(f.yuvSpace);
        ds << f.planeCount;
        for (int p = 0; p < f.planeCount; p++) {
            ds << f.bytesPerLine[p];
            ds << f.bytesPerPlane[p];
            if (f.storage == VideoFrame::Storage_Mapped)
                ds.writeRawData(reinterpret_cast<const char*>(f.mappedBits[p]), f.bytesPerPlane[p]);
            else
                ds.writeRawData(reinterpret_cast<const char*>(f.bits[p].data()), f.bytesPerPlane[p]);
        }
        break;
    case VideoFrame::Storage_Image:
        ds.writeRawData(reinterpret_cast<const char*>(f.image.bits()), f.image.sizeInBytes());
        break;
    }
    return ds;
}

QDataStream &operator>>(QDataStream& ds, VideoFrame& f)
{
    int tmp;

    ds >> tmp;
    f.stereoLayout = static_cast<VideoFrame::StereoLayout>(tmp);
    ds >> tmp;
    f.threeSixtyMode = static_cast<VideoFrame::ThreeSixtyMode>(tmp);
    ds >> f.subtitle;
    ds >> f.width;
    ds >> f.height;
    ds >> f.aspectRatio;
    ds >> tmp;
    f.storage = static_cast<enum VideoFrame::Storage>(tmp);
    switch (f.storage) {
    case VideoFrame::Storage_Mapped: // cannot happen, see above
    case VideoFrame::Storage_Copied:
        f.image = QImage();
        ds >> tmp;
        f.pixelFormat = static_cast<QVideoFrameFormat::PixelFormat>(tmp);
        ds >> f.yuvValueRangeSmall;
        ds >> tmp;
        f.yuvSpace = static_cast<enum VideoFrame::YUVSpace>(tmp);
        ds >> f.planeCount;
        for (int p = 0; p < 3; p++) {
            if (p < f.planeCount) {
                ds >> f.bytesPerLine[p];
                ds >> f.bytesPerPlane[p];
                f.bits[p].resize(f.bytesPerPlane[p]);
                ds.readRawData(reinterpret_cast<char*>(f.bits[p].data()), f.bytesPerPlane[p]);
            } else {
                f.bytesPerLine[p] = 0;
                f.bytesPerPlane[p] = 0;
                f.bits[p].clear();
            }
            f.mappedBits[p] = nullptr;
        }
        break;
    case VideoFrame::Storage_Image:
        f.pixelFormat = QVideoFrameFormat::pixelFormatFromImageFormat(QImage::Format_RGB32);
        f.yuvValueRangeSmall = false;
        f.yuvSpace = VideoFrame::YUV_AdobeRgb;
        f.planeCount = 0;
        for (int p = 0; p < 3; p++) {
            f.bytesPerLine[p] = 0;
            f.bytesPerPlane[p] = 0;
            f.mappedBits[p] = nullptr;
            f.bits[p].clear();
        }
        f.image = QImage(f.width, f.height, QImage::Format_RGB32);
        ds.readRawData(reinterpret_cast<char*>(f.image.bits()), f.image.sizeInBytes());
        break;
    }
    return ds;
}