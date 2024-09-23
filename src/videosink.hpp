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

#include <QVideoSink>
#include <QMediaMetaData>

#include "modes.hpp"
#include "videoframe.hpp"


class VideoSink : public QVideoSink
{
Q_OBJECT

public:
    unsigned long long frameCounter; // number of frames seen for this URL
    VideoFrame* frame;    // target video frame
    VideoFrame* extFrame; // extension to target video frame, for alternating stereo
    bool *frameIsNew;     // flag to set when the target frame represents a new frame
    bool needExtFrame;    // flag to set in alternating stereo when extFrame is not filled yet
    InputMode inputMode;  // input mode of current media
    SurroundMode surroundMode; // surround mode of the current media
    bool lastFrameWasValid; // last frame of current media was valid

    VideoSink(VideoFrame* frame, VideoFrame* extFrame, bool* frameIsNew);

    void newUrl(const QUrl& url, InputMode inputMode, SurroundMode surroundMode);

public Q_SLOTS:
    void processNewFrame(const QVideoFrame& frame);

signals:
    void newVideoFrame();
};
