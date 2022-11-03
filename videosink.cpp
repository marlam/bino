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

#include <QUrl>

#include "videosink.hpp"
#include "log.hpp"


VideoSink::VideoSink(VideoFrame* frame, VideoFrame* extFrame, bool* frameIsNew) :
    frameCounter(0),
    fileFormatIsMPO(false),
    frame(frame),
    extFrame(extFrame),
    frameIsNew(frameIsNew),
    needExtFrame(false),
    stereoLayout(VideoFrame::Layout_Unknown),
    threeSixtyMode(VideoFrame::ThreeSixty_Unknown)
{
    connect(this, SIGNAL(videoFrameChanged(const QVideoFrame&)), this, SLOT(processNewFrame(const QVideoFrame&)));
}

// called whenever a new media URL is played:
void VideoSink::newUrl(const QUrl& url, VideoFrame::StereoLayout layout, VideoFrame::ThreeSixtyMode mode)
{
    frameCounter = 0;
    fileFormatIsMPO = false;

    LOG_DEBUG("initial stereo layout for %s: %s", qPrintable(url.toString()), VideoFrame::layoutToString(stereoLayout));
    stereoLayout = layout;
    if (stereoLayout == VideoFrame::Layout_Unknown) {
        /* TODO: we should set the stereoscopic layout from media meta data,
         * but QMediaMetaData currently does not provide that information */
    }
    if (stereoLayout == VideoFrame::Layout_Unknown) {
        /* Try to guess from the file name extension */
        QString fileName = url.fileName();
        QString extension;
        if (fileName.lastIndexOf('.') > 0)
            extension = fileName.right(fileName.length() - fileName.lastIndexOf('.'));
        extension = extension.toLower();
        if (extension == ".jps" || extension == ".pns") {
            stereoLayout = VideoFrame::Layout_Right_Left;
        } else if (extension == ".mpo") {
            stereoLayout = VideoFrame::Layout_Alternating_LR;
            fileFormatIsMPO = true;
        }
        if (stereoLayout != VideoFrame::Layout_Unknown)
            LOG_DEBUG("setting stereo layout from file name extension %s: %s", qPrintable(extension), VideoFrame::layoutToString(stereoLayout));
    }
    if (stereoLayout == VideoFrame::Layout_Unknown) {
        /* Try to guess the layout from a marker contained in the file name.
         * This should be compatible to the Bino 1.x naming conventions. */
        QString fileName = url.fileName();
        QString marker = fileName.left(fileName.lastIndexOf('.'));
        marker = marker.right(marker.length() - marker.lastIndexOf('-') - 1);
        marker = marker.toLower();
        if (marker == "lr")
            stereoLayout = VideoFrame::Layout_Left_Right;
        else if (marker == "rl")
            stereoLayout = VideoFrame::Layout_Right_Left;
        else if (marker == "lrh" || marker == "lrq")
            stereoLayout = VideoFrame::Layout_Left_Right_Half;
        else if (marker == "rlh" || marker == "rlq")
            stereoLayout = VideoFrame::Layout_Right_Left_Half;
        else if (marker == "tb" || marker == "ab")
            stereoLayout = VideoFrame::Layout_Top_Bottom;
        else if (marker == "bt" || marker == "ba")
            stereoLayout = VideoFrame::Layout_Bottom_Top;
        else if (marker == "tbh" || marker == "abq")
            stereoLayout = VideoFrame::Layout_Top_Bottom_Half;
        else if (marker == "bth" || marker == "baq")
            stereoLayout = VideoFrame::Layout_Bottom_Top_Half;
        else if (marker == "2d")
            stereoLayout = VideoFrame::Layout_Mono;
        if (stereoLayout != VideoFrame::Layout_Unknown)
            LOG_DEBUG("setting stereo layout from file name marker %s: %s", qPrintable(marker), VideoFrame::layoutToString(stereoLayout));
    }
    threeSixtyMode = mode;
    if (threeSixtyMode == VideoFrame::ThreeSixty_Unknown) {
        /* TODO: we should set the 360° mode from media meta data,
         * but QMediaMetaData currently does not provide that information */
    }
    if (threeSixtyMode == VideoFrame::ThreeSixty_Unknown) {
        /* Try to guess the mode from the URL. */
        QString fileName = url.fileName();
        if (fileName.startsWith("360") || fileName.left(fileName.lastIndexOf('.')).endsWith("360"))
            threeSixtyMode = VideoFrame::ThreeSixty_On;
        if (threeSixtyMode != VideoFrame::ThreeSixty_Unknown)
            LOG_DEBUG("guessing 360°=%s from file name %s", (threeSixtyMode == VideoFrame::ThreeSixty_On ? "on" : "off"),  qPrintable(fileName));
    }
}

void VideoSink::processNewFrame(const QVideoFrame& frame)
{
    // Workaround a glitch in the MPO file format:
    // Gstreamer reads three frames from such files, the first two being the left
    // view and the last one being the right view.
    // We therefore ignore the first frame to get a proper left-right pair.
    if (fileFormatIsMPO && frameCounter == 0) {
        LOG_DEBUG("video sink ignores first frame from MPO file to work around glitch");
        frameCounter++;
        return;
    }

    bool updateExtFrame;
    if (stereoLayout == VideoFrame::Layout_Alternating_LR || stereoLayout == VideoFrame::Layout_Alternating_RL) {
        if (needExtFrame) {
            LOG_FIREHOSE("video sink updates extended frame for alternating mode");
            updateExtFrame = true;
            needExtFrame = false;
        } else {
            LOG_FIREHOSE("video sink updates standard frame for alternating mode");
            updateExtFrame = false;
            needExtFrame = true;
        }
    } else {
        LOG_FIREHOSE("video sink updates standard frame for non-alternating mode");
        updateExtFrame = false;
        needExtFrame = false;
    }
    if (updateExtFrame) {
        this->extFrame->update(stereoLayout, threeSixtyMode, frame, frameCounter == 0);
    } else {
        this->frame->update(stereoLayout, threeSixtyMode, frame, frameCounter == 0);
        this->extFrame->invalidate();
    }
    if (!needExtFrame) {
        LOG_FIREHOSE("video sink signals that new frame is complete");
        *frameIsNew = true;
        emit newVideoFrame();
    }
    frameCounter++;
}
