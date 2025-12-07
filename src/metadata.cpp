/*
 * This file is part of Bino, a 3D video player.
 *
 * Copyright (C) 2022, 2023, 2024, 2025
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

#include <QMediaPlayer>
#include <QGuiApplication>
#include <QThread>
#include <QMap>
#include <QJsonDocument>
#include <QJsonValue>
#ifndef Q_OS_WASM
# include <QProcess>
#endif

#include "metadata.hpp"
#include "tools.hpp"
#include "log.hpp"


MetaData::MetaData()
{
}

static int haveFFprobe = -1;
static QMap<QUrl, MetaData> cache;

bool MetaData::detectCached(const QUrl& url, QString* errMsg)
{
    /* Try to find the url in the cache */
    *this = cache[url];
    if (!this->url.isEmpty())
        return true;

    /* Detection via file name hints - this sets default modes that can be overridden per stream */
    InputMode defaultInputMode = Input_Unknown;
    SurroundMode defaultSurroundMode = Surround_Unknown;
    QString fileName = url.fileName();
    QString extension = getExtension(fileName);
    if (extension == "jps" || extension == "pns") {
        defaultInputMode = Input_Right_Left;
    } else if (extension == "mpo") {
        defaultInputMode = Input_Top_Bottom;       // this was converted; see digestiblemedia
    } else {
        /* Try to guess the input mode from a marker contained in the file name.
         * This should be compatible to the Bino 1.x naming conventions. */
        QString marker = fileName.left(fileName.lastIndexOf('.'));
        marker = marker.right(marker.length() - marker.lastIndexOf('-') - 1);
        marker = marker.toLower();
        if (marker == "lr")
            defaultInputMode = Input_Left_Right;
        else if (marker == "rl")
            defaultInputMode = Input_Right_Left;
        else if (marker == "lrh" || marker == "lrq")
            defaultInputMode = Input_Left_Right_Half;
        else if (marker == "rlh" || marker == "rlq")
            defaultInputMode = Input_Right_Left_Half;
        else if (marker == "tb" || marker == "ab")
            defaultInputMode = Input_Top_Bottom;
        else if (marker == "bt" || marker == "ba")
            defaultInputMode = Input_Bottom_Top;
        else if (marker == "tbh" || marker == "abq")
            defaultInputMode = Input_Top_Bottom_Half;
        else if (marker == "bth" || marker == "baq")
            defaultInputMode = Input_Bottom_Top_Half;
        else if (marker == "2d")
            defaultInputMode = Input_Mono;
    }
    if (defaultInputMode != Input_Unknown) {
        LOG_DEBUG("guessing input mode %s from file name %s", inputModeToString(defaultInputMode), qPrintable(fileName));
    }
    int i360 = fileName.indexOf("360");
    if (i360 >= 0 && (i360 == 0 || !fileName[i360 - 1].isLetterOrNumber()) && !fileName[i360 + 3].isLetterOrNumber()) {
        defaultSurroundMode = Surround_360;
    } else {
        int i180 = fileName.indexOf("180");
        if (i180 >= 0 && (i180 == 0 || !fileName[i180 - 1].isLetterOrNumber()) && !fileName[i180 + 3].isLetterOrNumber())
            defaultSurroundMode = Surround_180;
    }
    if (defaultSurroundMode != Surround_Unknown) {
        LOG_DEBUG("guessing surround mode %s from file name %s", surroundModeToString(defaultSurroundMode), qPrintable(fileName));
    }

    /* Detection via QMediaPlayer */
    QMediaPlayer player;
    bool failure = false;
    bool available = false;
    bool didFFprobe = false;
    QString errorMessage;
    player.connect(&player, &QMediaPlayer::errorOccurred,
            [&](QMediaPlayer::Error, const QString& errorString) {
            errorMessage = errorString;
            LOG_WARNING("%s", qPrintable(tr("Cannot get meta data from %1: %2").arg(player.source().toString()).arg(errorString)));
            failure = true;
            });
    player.connect(&player, &QMediaPlayer::metaDataChanged, [&]() { available = true; });
    player.setSource(url);
    do {
        // while waiting for the QMediaPlayer meta data to arrive, run ffprobe in parallel
        if (!didFFprobe) {
            detectViaFFprobe(url, defaultInputMode, defaultSurroundMode);
            didFFprobe = true;
        }
        QGuiApplication::processEvents();
    }
    while (!failure && !available);
    if (failure) {
        if (errMsg)
            *errMsg = errorMessage;
        return false;
    }

    this->url = url;
    global = player.metaData();
    videoTracks = player.videoTracks();
    audioTracks = player.audioTracks();
    subtitleTracks = player.subtitleTracks();
    inputModes.resize(videoTracks.size(), defaultInputMode);
    surroundModes.resize(videoTracks.size(), defaultSurroundMode);

    cache.insert(url, *this);
    return true;
}

void MetaData::detectViaFFprobe(const QUrl& url, InputMode& defaultInputMode, SurroundMode& defaultSurroundMode)
{
#ifndef Q_OS_WASM
    /* Detection via ffprobe (if available) */
    if (haveFFprobe == -1) {
        // check once whether we can run ffprobe
        QProcess prc;
        prc.startCommand("ffprobe --help");
        prc.waitForFinished(1000);
        haveFFprobe = (prc.exitStatus() == QProcess::NormalExit && prc.exitCode() == 0);
        LOG_DEBUG("ffprobe available: %d (exit status %d, exit code %d)", haveFFprobe,
                prc.exitStatus() == QProcess::NormalExit ? 0 : 1, prc.exitCode());
    }
    if (haveFFprobe) {
        QProcess prc;
        prc.setProgram("ffprobe");
        QStringList args;
        args.append("-of");
        args.append("json");
        args.append("-show_format");
        args.append("-show_streams");
        args.append(url.toString());
        prc.setArguments(args);
        prc.start();
        prc.waitForFinished(5000);
        if (prc.exitStatus() == QProcess::NormalExit && prc.exitCode() == 0) {
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(prc.readAllStandardOutput(), &err);
            if (doc.isNull()) {
                LOG_DEBUG("cannot parse ffprobe json output: %s", qPrintable(err.errorString()));
            } else {
                // First check 3dtv.at conventions. These are not interpreted by FFmpeg.
                // See https://www.3dtv.at/Knowhow/StereoWmvSpec_en.aspx
                QString layout = doc["format"]["tags"]["StereoscopicLayout"].toString();
                QString halfWidth = doc["format"]["tags"]["StereoscopicHalfWidth"].toString();
                QString halfHeight = doc["format"]["tags"]["StereoscopicHalfHeight"].toString();
                if (layout == "SideBySideRF")
                    defaultInputMode = (halfWidth == "1") ? Input_Right_Left_Half : Input_Right_Left;
                else if (layout == "SideBySideLF")
                    defaultInputMode = (halfWidth == "1") ? Input_Left_Right_Half : Input_Left_Right;
                else if (layout == "OverUnderRT")
                    defaultInputMode = (halfHeight == "1") ? Input_Bottom_Top_Half : Input_Bottom_Top;
                else if (layout == "OverUnderLT")
                    defaultInputMode = (halfHeight == "1") ? Input_Top_Bottom_Half : Input_Top_Bottom;
                // FFmpeg metadata, per stream, see libavutil/stereo3d.h and libavutil/spherical.h
                QJsonValue streamArray = doc["streams"];
                if (!streamArray.isNull() && !streamArray.isUndefined()) {
                    for (int i = 0; ; i++) {
                        QString codecType = streamArray[i]["codec_type"].toString();
                        if (codecType.isEmpty()) {
                            break;
                        } else if (codecType == "video") {
                            inputModes.append(defaultInputMode);
                            surroundModes.append(defaultSurroundMode);
                            QJsonValue sideDataArray = streamArray[i]["side_data_list"];
                            if (!sideDataArray.isNull() && !sideDataArray.isUndefined()) {
                                for (int j = 0; ; j++) {
                                    QString type = sideDataArray[j]["side_data_type"].toString();
                                    if (type.isEmpty()) {
                                        break;
                                    } else if (type == "Stereo 3D") {
                                        QString layout = sideDataArray[j]["type"].toString();
                                        QString inverted = sideDataArray[j]["inverted"].toString();
                                        if (layout == "top and bottom") {
                                            inputModes.last() = (inverted == "1" ? Input_Bottom_Top : Input_Top_Bottom);
                                        } else if (layout == "side by side") {
                                            inputModes.last() = (inverted == "1" ? Input_Right_Left : Input_Left_Right);
                                        } else if (layout == "frame alternate") {
                                            inputModes.last() = (inverted == "1" ? Input_Alternating_RL : Input_Alternating_LR);
                                        } else if (layout == "2D") {
                                            inputModes.last() = Input_Mono;
                                        }
                                    } else if (type == "Spherical Mapping") {
                                        QString proj = sideDataArray[j]["projection"].toString();
                                        if (proj == "equirectangular") {
                                            surroundModes.last() = Surround_360;
                                        } else if (proj == "half equirectangular") {
                                            surroundModes.last() = Surround_180;
                                        }
                                    }
                                }
                            }
                            LOG_DEBUG("meta data from ffprobe for video stream %d: input mode %s, surround mode %s",
                                    int(inputModes.size() - 1),
                                    inputModeToString(inputModes.last()), surroundModeToString(surroundModes.last()));
                        }
                    }
                }
            }
        } else {
            LOG_DEBUG("cannot run ffprobe: exit status %d, exit code %d",
                    prc.exitStatus() == QProcess::NormalExit ? 0 : 1, prc.exitCode());
        }
    }
#endif
}
