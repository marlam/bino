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

// for isatty():
#if __has_include(<unistd.h>)
# include <unistd.h>
#elif __has_include(<io.h>)
# include <io.h>
#endif

#include <cstdio>
#include <string>

#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QUrl>
#include <QMessageBox>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QCameraDevice>
#include <QAudioOutput>
#include <QAudioInput>
#include <QCamera>
#include <QSurfaceFormat>
#include <QOpenGLContext>

#ifdef WITH_QVR
#  include <qvr/manager.hpp>
#endif

#include "version.hpp"
#include "log.hpp"
#include "videosink.hpp"
#include "playlist.hpp"
#include "metadata.hpp"
#include "screen.hpp"
#include "qvrapp.hpp"
#include "mainwindow.hpp"
#include "bino.hpp"


void logQtMsg(QtMsgType type, const QMessageLogContext&, const QString& msg)
{
    switch (type) {
    case QtDebugMsg:
        LOG_DEBUG("Qt debug: %s", qPrintable(msg));
        break;
    case QtInfoMsg:
        LOG_INFO("Qt info: %s", qPrintable(msg));
        break;
    case QtWarningMsg:
        LOG_WARNING("Qt warning: %s", qPrintable(msg));
        break;
    case QtCriticalMsg:
        LOG_FATAL("Qt critical: %s", qPrintable(msg));
        break;
    case QtFatalMsg:
        LOG_FATAL("Qt fatal: %s", qPrintable(msg));
        break;
    }
}

int main(int argc, char* argv[])
{
    // Early check before command line options are consumed: is this process a QVR child process?
    bool vrChildProcess = false;
#ifdef WITH_QVR
    for (int i = 1; i < argc; i++) {
        if (QString(argv[i]).startsWith("--qvr-process")) {
            vrChildProcess = true;
            break;
        }
    }
#endif

    // Initialize Qt
    qInstallMessageHandler(logQtMsg);
    QApplication app(argc, argv);
    QApplication::setApplicationName("Bino");
    QApplication::setApplicationVersion(BINO_VERSION);
#ifdef WITH_QVR
    QVRManager manager(argc, argv);
#endif

    // Process command line
    QCommandLineParser parser;
    parser.setApplicationDescription("3D video player -- see https://bino3d.org");
    parser.addPositionalArgument("[URL...]", "Media to play.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({ "log-level", "Set log level (fatal, warning, info, debug, firehose).", "level" });
    parser.addOption({ "log-file", "Set log file.", "file" });
    parser.addOption({ "opengles", "Use OpenGL ES instead of Desktop OpenGL." });
    parser.addOption({ "stereo", "Use OpenGL quad-buffered stereo in GUI mode."});
    parser.addOption({ "vr", "Start in VR mode instead of GUI mode."});
    parser.addOption({ "vr-screen", "Set VR screen geometry, either as a comma-separated list of "
            "nine values representing three 3D coordinates that define a planar screen (bottom left, bottom right, top left) "
            "or as a name of an OBJ file that contains the screen geometry with texture coordinates.", "screen" });
    parser.addOption({ "capture", "Capture audio/video input from camera and microphone." });
    parser.addOption({ "list-audio-outputs", "List audio outputs." });
    parser.addOption({ "list-audio-inputs", "List audio inputs." });
    parser.addOption({ "list-video-inputs", "List video inputs." });
    parser.addOption({ "audio-output", "Choose audio output via its index.", "ao" });
    parser.addOption({ "audio-input", "Choose audio input via its index. Can be empty.", "ai" });
    parser.addOption({ "video-input", "Choose video input via its index.", "vi" });
    parser.addOption({ "preferred-audio", "Set preferred audio track language via ISO639 code (en, de, fr, ...).", "lang" });
    parser.addOption({ "preferred-subtitle", "Set preferred subtitle track language via ISO639 code (en, de, fr, ...). Can be empty.", "lang" });
    parser.addOption({ "list-tracks", "List all video, audio and subtitle tracks in the media." });
    parser.addOption({ "video-track", "Choose video track via its index.", "track" });
    parser.addOption({ "audio-track", "Choose audio track via its index.", "track" });
    parser.addOption({ "subtitle-track", "Choose subtitle track via its index. Can be empty.", "track" });
    parser.addOption({ { "S", "swap-eyes" }, "Swap left/right eye." });
    parser.addOption({ { "f", "fullscreen" }, "Start in fullscreen mode." });
    parser.addOption({ { "i", "input" }, "Set input mode (mono, "
            "top-bottom, top-bottom-half, bottom-top, bottom-top-half, "
            "left-right, left-right-half, right-left, right-left-half, "
            "alternating-left-right, alternating-right-left).", "mode" });
    parser.addOption({ { "o", "output" }, "Set output mode (left, right, stereo, alternating, "
            "red-cyan-dubois, red-cyan-full-color, red-cyan-half-color, red-cyan-monochrome, "
            "green-magenta-dubois, green-magenta-full-color, green-magenta-half-color, green-magenta-monochrome, "
            "amber-blue-dubois, amber-blue-full-color, amber-blue-half-color, amber-blue-monochrome, "
            "red-green-monochrome, red-blue-monochrome).", "mode" });
    parser.addOption({ "360", "Set 360° mode (on, off).", "mode" });
    parser.process(app);

    // Initialize logging
    SetLogLevel(Log_Level_Warning);
    if (parser.isSet("log-file")) {
        SetLogFile(qPrintable(parser.value("log-file")), true);
    }
    if (parser.isSet("log-level")) {
        if (parser.value("log-level") == "fatal")
            SetLogLevel(Log_Level_Fatal);
        else if (parser.value("log-level") == "warning")
            SetLogLevel(Log_Level_Warning);
        else if (parser.value("log-level") == "info")
            SetLogLevel(Log_Level_Info);
        else if (parser.value("log-level") == "debug")
            SetLogLevel(Log_Level_Debug);
        else if (parser.value("log-level") == "firehose")
            SetLogLevel(Log_Level_Firehose);
        else {
            LOG_FATAL("invalid log level %s", qPrintable(parser.value("log-level")));
            return 1;
        }
    }

    // Check if VR mode is available if requested
#ifndef WITH_QVR
    if (parser.isSet("vr")) {
        LOG_FATAL("VR mode unavailable - recompile Bino with QVR support!");
        return 1;
    }
#endif

    // Set modes
    VideoFrame::ThreeSixtyMode threeSixtyMode = VideoFrame::ThreeSixty_Unknown;
    if (parser.isSet("360")) {
        if (parser.value("360") == "on") {
            threeSixtyMode = VideoFrame::ThreeSixty_On;
        } else if (parser.value("360") == "off") {
            threeSixtyMode = VideoFrame::ThreeSixty_Off;
        } else {
            LOG_FATAL("invalid argument for option %s", "--360");
            return 1;
        }
    }
    VideoFrame::StereoLayout inputMode = VideoFrame::Layout_Unknown;
    if (parser.isSet("input")) {
        if (parser.value("input") == "mono")
            inputMode = VideoFrame::Layout_Mono;
        else if (parser.value("input") == "top-bottom")
            inputMode = VideoFrame::Layout_Top_Bottom;
        else if (parser.value("input") == "top-bottom-half")
            inputMode = VideoFrame::Layout_Top_Bottom_Half;
        else if (parser.value("input") == "bottom-top")
            inputMode = VideoFrame::Layout_Bottom_Top;
        else if (parser.value("input") == "bottom-top-half")
            inputMode = VideoFrame::Layout_Bottom_Top_Half;
        else if (parser.value("input") == "left-right")
            inputMode = VideoFrame::Layout_Left_Right;
        else if (parser.value("input") == "left-right-half")
            inputMode = VideoFrame::Layout_Left_Right_Half;
        else if (parser.value("input") == "right-left")
            inputMode = VideoFrame::Layout_Right_Left;
        else if (parser.value("input") == "right-left-half")
            inputMode = VideoFrame::Layout_Right_Left_Half;
        else if (parser.value("input") == "alternating-left-right")
            inputMode = VideoFrame::Layout_Alternating_LR;
        else if (parser.value("input") == "alternating-right-left")
            inputMode = VideoFrame::Layout_Alternating_RL;
        else {
            LOG_FATAL("invalid input mode");
            return 1;
        }
    }
    Widget::StereoMode outputMode = Widget::Mode_Red_Cyan_Dubois;
    if (parser.isSet("output")) {
        if (parser.value("output") == "left")
            outputMode = Widget::Mode_Left;
        else if (parser.value("output") == "right")
            outputMode = Widget::Mode_Right;
        else if (parser.value("output") == "stereo")
            outputMode = Widget::Mode_OpenGL_Stereo;
        else if (parser.value("output") == "alternating")
            outputMode = Widget::Mode_Alternating;
        else if (parser.value("output") == "red-cyan-dubois")
            outputMode = Widget::Mode_Red_Cyan_Dubois;
        else if (parser.value("output") == "red-cyan-full-color")
            outputMode = Widget::Mode_Red_Cyan_FullColor;
        else if (parser.value("output") == "red-cyan-half-color")
            outputMode = Widget::Mode_Red_Cyan_HalfColor;
        else if (parser.value("output") == "red-cyan-monochrome")
            outputMode = Widget::Mode_Red_Cyan_Monochrome;
        else if (parser.value("output") == "green-magenta-dubois")
            outputMode = Widget::Mode_Green_Magenta_Dubois;
        else if (parser.value("output") == "green-magenta-full-color")
            outputMode = Widget::Mode_Green_Magenta_FullColor;
        else if (parser.value("output") == "green-magenta-half-color")
            outputMode = Widget::Mode_Green_Magenta_HalfColor;
        else if (parser.value("output") == "green-magenta-monochrome")
            outputMode = Widget::Mode_Green_Magenta_Monochrome;
        else if (parser.value("output") == "amber-blue-dubois")
            outputMode = Widget::Mode_Amber_Blue_Dubois;
        else if (parser.value("output") == "amber-blue-full-color")
            outputMode = Widget::Mode_Amber_Blue_FullColor;
        else if (parser.value("output") == "amber-blue-half-color")
            outputMode = Widget::Mode_Amber_Blue_HalfColor;
        else if (parser.value("output") == "amber-blue-monochrome")
            outputMode = Widget::Mode_Amber_Blue_Monochrome;
        else if (parser.value("output") == "red-green-monochrome")
            outputMode = Widget::Mode_Red_Green_Monochrome;
        else if (parser.value("output") == "red-blue-monochrome")
            outputMode = Widget::Mode_Red_Blue_Monochrome;
        else {
            LOG_FATAL("invalid output mode");
            return 1;
        }
    }

    // Lists of available devices. Initialize these lists only when necessary because
    // this can take some time!
    QList<QAudioDevice> audioOutputDevices;
    QList<QAudioDevice> audioInputDevices;
    QList<QCameraDevice> videoInputDevices;

    // List devices
    bool deviceListRequested = false;
    if (parser.isSet("list-audio-outputs")) {
        audioOutputDevices = QMediaDevices::audioOutputs();
        if (audioOutputDevices.size() == 0) {
            LOG_REQUESTED("no audio outputDevices available.");
        } else {
            for (qsizetype i = 0; i < audioOutputDevices.size(); i++)
                LOG_REQUESTED("no audio output %d: %s", int(i), qPrintable(audioOutputDevices[i].description()));
        }
        deviceListRequested = true;
    }
    if (parser.isSet("list-audio-inputs")) {
        audioInputDevices = QMediaDevices::audioInputs();
        if (audioInputDevices.size() == 0) {
            LOG_REQUESTED("no audio inputs available.");
        } else {
            for (qsizetype i = 0; i < audioInputDevices.size(); i++)
                LOG_REQUESTED("audio input %d: %s", int(i), qPrintable(audioInputDevices[i].description()));
        }
        deviceListRequested = true;
    }
    if (parser.isSet("list-video-inputs")) {
        videoInputDevices = QMediaDevices::videoInputs();
        if (videoInputDevices.size() == 0) {
            LOG_REQUESTED("no video inputs available.");
        } else {
            for (qsizetype i = 0; i < videoInputDevices.size(); i++)
                LOG_REQUESTED("video input %d: %s", int(i), qPrintable(videoInputDevices[i].description()));
        }
        deviceListRequested = true;
    }
    if (deviceListRequested) {
        return 0;
    }

    // Get requested device indices; -1 means default
    int audioOutputDeviceIndex = -1;
    int audioInputDeviceIndex = -1;
    int videoInputDeviceIndex = -1;
    if (parser.isSet("audio-output")) {
        audioOutputDevices = QMediaDevices::audioOutputs();
        bool ok;
        int ao = parser.value("audio-output").toInt(&ok);
        if (ok && ao >= 0 && ao < audioOutputDevices.size()) {
            audioOutputDeviceIndex = ao;
        } else {
            LOG_FATAL("invalid argument for option %s", "--audio-output");
            return 1;
        }
    }
    if (parser.isSet("capture")) {
        bool ok;
        if (parser.isSet("audio-input")) {
            if (parser.value("audio-input").length() == 0) {
                audioInputDeviceIndex = -2; // this means no audio input
            } else {
                audioInputDevices = QMediaDevices::audioInputs();
                int ai = parser.value("audio-input").toInt(&ok);
                if (ok && ai >= 0 && ai < audioInputDevices.size()) {
                    audioInputDeviceIndex = ai;
                } else {
                    LOG_FATAL("invalid argument for option %s", "--audio-input");
                    return 1;
                }
            }
        }
        if (parser.isSet("video-input")) {
            videoInputDevices = QMediaDevices::videoInputs();
            int vi = parser.value("video-input").toInt(&ok);
            if (ok && vi >= 0 && vi < videoInputDevices.size()) {
                videoInputDeviceIndex = vi;
            } else {
                LOG_FATAL("invalid argument for option %s", "--video-input");
                return 1;
            }
        }
    }

    // Get playlist.
    Playlist playlist;
    if (parser.isSet("preferred-audio")) {
        QLocale::Language lang = QLocale::codeToLanguage(parser.value("preferred-audio"));
        if (lang == QLocale::AnyLanguage) {
            LOG_FATAL("invalid argument for option %s", "--preferred-audio");
            return 1;
        } else {
            playlist.preferredAudio = lang;
        }
    }
    if (parser.isSet("preferred-subtitle")) {
        if (parser.value("preferred-subtitle").length() == 0) {
            playlist.wantSubtitle = false;
        } else {
            QLocale::Language lang = QLocale::codeToLanguage(parser.value("preferred-subtitle"));
            if (lang == QLocale::AnyLanguage) {
                LOG_FATAL("invalid argument for option %s", "--preferred-subtitle");
                return 1;
            } else {
                playlist.preferredSubtitle = lang;
            }
        }
    }
    int videoTrack = PlaylistEntry::DefaultTrack;
    int audioTrack = PlaylistEntry::DefaultTrack;
    int subtitleTrack = PlaylistEntry::DefaultTrack;
    if (parser.isSet("video-track")) {
        bool ok;
        int vt = parser.value("video-track").toInt(&ok);
        if (ok && vt >= 0) {
            videoTrack = vt;
        } else {
            LOG_FATAL("invalid argument for option %s", "--video-track");
            return 1;
        }
    }
    if (parser.isSet("audio-track")) {
        bool ok;
        int at = parser.value("audio-track").toInt(&ok);
        if (ok && at >= 0) {
            audioTrack = at;
        } else {
            LOG_FATAL("invalid argument for option %s", "--audio-track");
            return 1;
        }
    }
    if (parser.isSet("subtitle-track")) {
        if (parser.value("subtitle-track").length() == 0) {
            subtitleTrack = PlaylistEntry::NoTrack;
        } else {
            bool ok;
            int st = parser.value("subtitle-track").toInt(&ok);
            if (ok && st >= 0) {
                subtitleTrack = st;
            } else {
                LOG_FATAL("invalid argument for option %s", "--subtitle-track");
                return 1;
            }
        }
    }
    for (qsizetype i = 0; i < parser.positionalArguments().length(); i++) {
        QUrl url = parser.positionalArguments()[i];
        if (url.isRelative()) {
            QFileInfo fileInfo(parser.positionalArguments()[i]);
            if (fileInfo.exists()) {
                url = QUrl::fromLocalFile(fileInfo.canonicalFilePath());
            } else {
                LOG_WARNING("file does not exist: %s", qPrintable(parser.positionalArguments()[i]));
                continue;
            }
        }
        playlist.append(PlaylistEntry(url, inputMode, threeSixtyMode,
                    videoTrack, audioTrack, subtitleTrack));
    }
    if (parser.positionalArguments().length() > 0 && playlist.length() == 0) {
        return 1;
    }
    if (parser.isSet("capture") && playlist.length() > 0) {
        LOG_FATAL("cannot capture and play URL at the same time.");
        return 1;
    }

    // List tracks
    if (parser.isSet("list-tracks")) {
        MetaData metaData;
        for (qsizetype i = 0; i < playlist.length(); i++) {
            if (!metaData.detectCached(playlist.entries[i].url))
                return 1;
            LOG_REQUESTED("%s", qPrintable(metaData.url.toString()));
            for (qsizetype l = 0; l < metaData.global.keys().size(); l++) {
                LOG_REQUESTED("    %s: %s",
                        qPrintable(QMediaMetaData::metaDataKeyToString(metaData.global.keys()[l])),
                        qPrintable(metaData.global.stringValue(metaData.global.keys()[l])));
            }
            for (int k = 0; k < 3; k++) {
                QString trackType =
                    (k == 0 ? "video" : k == 1 ? "audio" : "subtitle");
                const QList<QMediaMetaData>& metaDataList =
                    (k == 0 ? metaData.videoTracks : k == 1 ? metaData.audioTracks : metaData.subtitleTracks);
                if (metaDataList.size() == 0) {
                    LOG_REQUESTED("  no %s tracks", qPrintable(trackType));
                } else {
                    for (qsizetype l = 0; l < metaDataList.size(); l++) {
                        LOG_REQUESTED("  %s track %d", qPrintable(trackType), int(l));
                        QMediaMetaData lmd = metaDataList[l];
                        for (qsizetype m = 0; m < lmd.keys().size(); m++) {
                            QMediaMetaData::Key key = lmd.keys()[m];
                            if (metaData.global.stringValue(key) == lmd.stringValue(key))
                                continue;
                            LOG_REQUESTED("    %s: %s",
                                    qPrintable(QMediaMetaData::metaDataKeyToString(key)),
                                    qPrintable(lmd.stringValue(key)));
                        }
                    }
                }
            }
        }
        return 0;
    }

    // Handle the VR Screen
    Screen screen;
    if (parser.isSet("vr")) {
        float screenCenterHeight = 1.76f - 0.15f; // does not matter, never used
#ifdef WITH_QVR
        screenCenterHeight = QVRObserverConfig::defaultEyeHeight;
#endif
        screen = Screen(
                QVector3D(-16.0f / 9.0f, -1.0f + screenCenterHeight, -8.0f),
                QVector3D(+16.0f / 9.0f, -1.0f + screenCenterHeight, -8.0f),
                QVector3D(-16.0f / 9.0f, +1.0f + screenCenterHeight, -8.0f));
        if (parser.isSet("vr-screen")) {
            QStringList paramList = parser.value("vr-screen").split(',');
            float values[9];
            if (paramList.length() == 9
                    && 9 == std::sscanf(qPrintable(parser.value("vr-screen")),
                        "%f,%f,%f,%f,%f,%f,%f,%f,%f",
                        values + 0, values + 1, values + 2,
                        values + 3, values + 4, values + 5,
                        values + 6, values + 7, values + 8)) {
                screen = Screen(
                        QVector3D(values[0], values[1], values[2]),
                        QVector3D(values[3], values[4], values[5]),
                        QVector3D(values[6], values[7], values[8]));
            } else if (paramList.length() == 2) {
                float ar;
                float ar2[2];
                if (2 == std::sscanf(qPrintable(paramList[0]), "%f:%f", ar2 + 0, ar2 + 1)) {
                    ar = ar2[0] / ar2[1];
                } else if (1 != std::sscanf(qPrintable(paramList[0]), "%f", &ar)) {
                    LOG_FATAL("invalid VR screen aspect ratio %s", qPrintable(paramList[0]));
                    return 1;
                }
                screen = Screen(paramList[1], "", ar);
                if (screen.indices.size() == 0)
                    return 1;
            } else {
                LOG_FATAL("invalid VR screen definition: %s", qPrintable(parser.value("screen")));
                return 1;
            }
        } else {
            LOG_INFO("using default VR screen");
        }
    }

    // Determine VR or GUI mode
    bool vrMainProcess = parser.isSet("vr");
    bool vrMode = (vrMainProcess || vrChildProcess);
    bool guiMode = !vrMode;

    // Set the OpenGL context parameters
    QSurfaceFormat format;
    format.setRedBufferSize(10);
    format.setGreenBufferSize(10);
    format.setBlueBufferSize(10);
    format.setAlphaBufferSize(0);
    format.setStencilBufferSize(0);
    if (parser.isSet("opengles"))
        format.setRenderableType(QSurfaceFormat::OpenGLES);
    if (QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGLES
            || format.renderableType() == QSurfaceFormat::OpenGLES) {
        format.setVersion(3, 2);
    } else {
        format.setProfile(QSurfaceFormat::CoreProfile);
        format.setVersion(3, 3);
    }
    if (guiMode && (parser.isSet("stereo") || parser.value("output") == "stereo")) {
        // The user has to explicitly request stereo mode, we cannot simply
        // try it and fall back to normal mode if it is not available:
        // Somehow Qt messes up something in the OpenGL context or widget setup
        // when stereo was requested but is not available. This was a problem at
        // least from Qt 5.7-5.9 (see comments in Bino 1.x src/video_output_qt.cpp),
        // and now again with Qt 6.3.
        // So now we only try to use it when explicitly requested, and we immediately
        // quit when we don't get it (see Bino::initializeGL).
        format.setStereo(true);
        if (!parser.isSet("output"))
            outputMode = Widget::Mode_OpenGL_Stereo;
    }
    QSurfaceFormat::setDefaultFormat(format);

    // Initialize Bino (in VR mode: only from the main process!)
    Bino bino(screen, parser.isSet("swap-eyes"));
    if (guiMode || !vrChildProcess) {
        bino.initializeOutput(audioOutputDeviceIndex >= 0
                ? audioOutputDevices[audioOutputDeviceIndex]
                : QMediaDevices::defaultAudioOutput());
        if (parser.isSet("capture")) {
            bino.startCaptureMode(audioInputDeviceIndex >= -1,
                    audioInputDeviceIndex >= 0
                    ? audioInputDevices[audioInputDeviceIndex]
                    : QMediaDevices::defaultAudioInput(),
                    videoInputDeviceIndex >= 0
                    ? videoInputDevices[videoInputDeviceIndex]
                    : QMediaDevices::defaultVideoInput());
        } else {
            bino.startPlaylistMode();
        }
    }

    // Start VR or GUI mode
    if (vrMode) {
#ifdef WITH_QVR
        BinoQVRApp qvrapp(&bino);
        if (!manager.init(&qvrapp)) {
            LOG_FATAL("cannot initialize QVR manager");
            return 1;
        }
        playlist.start();
        return app.exec();
#endif
    } else {
        MainWindow mainwindow(&bino, outputMode, parser.isSet("fullscreen"));
        mainwindow.show();
        // wait for up to 10 seconds to process all events before starting
        // the playlist, because otherwise playing might be finished before
        // the first frame rendering, e.g. if you just want to "play" an image
        QGuiApplication::processEvents(QEventLoop::AllEvents, 3000);
        playlist.start();
        return app.exec();
    }
}
