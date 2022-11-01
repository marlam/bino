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

#include <QVideoSink>
#include <QMediaMetaData>

#include "videoframe.hpp"

class VideoSink : public QVideoSink
{
Q_OBJECT

public:
    VideoFrame* frame; // target video frame
    bool *frameIsNew;  // flag to set when the target frame represents a new frame
    bool srcIsNew;     // flag to set when a new video source is played
    VideoFrame::StereoLayout stereoLayout; // stereo layout of current media
    VideoFrame::ThreeSixtyMode threeSixtyMode; // 360° mode of the current media

    VideoSink(VideoFrame* frame, bool* frameIsNew);

    void newUrl(const QUrl& url, VideoFrame::StereoLayout stereoLayout, VideoFrame::ThreeSixtyMode mode);

    void setStereoLayout(VideoFrame::StereoLayout layout);
    void setThreeSixtyMode(VideoFrame::ThreeSixtyMode mode);

public Q_SLOTS:
    void processNewFrame(const QVideoFrame& frame);

signals:
    void newVideoFrame();
};
