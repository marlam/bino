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

#include <QUrl>

#include "videosink.hpp"
#include "log.hpp"


VideoSink::VideoSink(VideoFrame* frame, VideoFrame* extFrame, bool* frameIsNew) :
    frameCounter(0),
    frame(frame),
    extFrame(extFrame),
    frameIsNew(frameIsNew),
    needExtFrame(false),
    inputMode(Input_Unknown),
    surroundMode(Surround_Unknown),
    lastFrameWasValid(false)
{
    connect(this, SIGNAL(videoFrameChanged(const QVideoFrame&)), this, SLOT(processNewFrame(const QVideoFrame&)));
}

// called whenever a new media URL is played:
void VideoSink::newUrl(const QUrl& url, InputMode im, SurroundMode sm)
{
    frameCounter = 0;
    lastFrameWasValid = false;

    LOG_DEBUG("initial input mode for %s: %s", qPrintable(url.toString()), inputModeToString(im));
    inputMode = im;
    if (inputMode == Input_Unknown) {
        /* TODO: we should set the input mode from media meta data,
         * but QMediaMetaData currently does not provide that information */
    }
    if (inputMode == Input_Unknown) {
        /* Try to guess from the file name extension */
        QString fileName = url.fileName();
        QString extension;
        if (fileName.lastIndexOf('.') > 0)
            extension = fileName.right(fileName.length() - fileName.lastIndexOf('.'));
        extension = extension.toLower();
        if (extension == ".jps" || extension == ".pns") {
            inputMode = Input_Right_Left;
        } else if (extension == ".mpo") {
            inputMode = Input_Top_Bottom;       // this was converted; see digestiblemedia
        }
        if (inputMode != Input_Unknown)
            LOG_DEBUG("setting input mode %s from file name extension %s", inputModeToString(inputMode), qPrintable(extension));
    }
    if (inputMode == Input_Unknown) {
        /* Try to guess the input mode from a marker contained in the file name.
         * This should be compatible to the Bino 1.x naming conventions. */
        QString fileName = url.fileName();
        QString marker = fileName.left(fileName.lastIndexOf('.'));
        marker = marker.right(marker.length() - marker.lastIndexOf('-') - 1);
        marker = marker.toLower();
        if (marker == "lr")
            inputMode = Input_Left_Right;
        else if (marker == "rl")
            inputMode = Input_Right_Left;
        else if (marker == "lrh" || marker == "lrq")
            inputMode = Input_Left_Right_Half;
        else if (marker == "rlh" || marker == "rlq")
            inputMode = Input_Right_Left_Half;
        else if (marker == "tb" || marker == "ab")
            inputMode = Input_Top_Bottom;
        else if (marker == "bt" || marker == "ba")
            inputMode = Input_Bottom_Top;
        else if (marker == "tbh" || marker == "abq")
            inputMode = Input_Top_Bottom_Half;
        else if (marker == "bth" || marker == "baq")
            inputMode = Input_Bottom_Top_Half;
        else if (marker == "2d")
            inputMode = Input_Mono;
        if (inputMode != Input_Unknown)
            LOG_DEBUG("setting input mode %s from file name marker %s", inputModeToString(inputMode), qPrintable(marker));
    }
    surroundMode = sm;
    if (surroundMode == Surround_Unknown) {
        /* TODO: we should set the 180°/360° mode from media meta data,
         * but QMediaMetaData currently does not provide that information */
    }
    if (surroundMode == Surround_Unknown) {
        /* Try to guess the mode from the URL. */
        QString fileName = url.fileName();
        int i360 = fileName.indexOf("360");
        if (i360 >= 0 && (i360 == 0 || !fileName[i360 - 1].isLetterOrNumber()) && !fileName[i360 + 3].isLetterOrNumber()) {
            surroundMode = Surround_360;
        } else {
            int i180 = fileName.indexOf("180");
            if (i180 >= 0 && (i180 == 0 || !fileName[i180 - 1].isLetterOrNumber()) && !fileName[i180 + 3].isLetterOrNumber())
                surroundMode = Surround_180;
        }
        if (surroundMode != Surround_Unknown)
            LOG_DEBUG("guessing surround mode %s from file name %s", surroundModeToString(surroundMode), qPrintable(fileName));
    }
}

void VideoSink::processNewFrame(const QVideoFrame& frame)
{
    if (!frame.isValid() && lastFrameWasValid) {
        // keep showing the last frame of the current media; ignore that QtMultiMedia
        // with the FFmpeg backend gives us an invalid frame as soon as the media stops
        // (it does not do this with the GStreamer backend)
        LOG_DEBUG("video sink gets invalid frame and ignores it since last frame of current media was valid");
        return;
    }
    if (frame.isValid()) {
        LOG_DEBUG("video sink gets a valid frame");
        lastFrameWasValid = true;
    }

    bool updateExtFrame;
    if (inputMode == Input_Alternating_LR || inputMode == Input_Alternating_RL) {
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
        this->extFrame->update(inputMode, surroundMode, frame, frameCounter == 0);
    } else {
        this->frame->update(inputMode, surroundMode, frame, frameCounter == 0);
        this->extFrame->invalidate();
    }
    if (!needExtFrame) {
        LOG_FIREHOSE("video sink signals that new frame is complete");
        *frameIsNew = true;
        emit newVideoFrame();
    }
    frameCounter++;
}
