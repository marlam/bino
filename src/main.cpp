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

#include <cstdio>

#include <QApplication>
#include <QTranslator>
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
#include <QWindowCapture>
#include <QtSystemDetection>
#include <QtProcessorDetection>

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
#include "gui.hpp"
#include "commandinterpreter.hpp"
#include "modes.hpp"
#include "tools.hpp"
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
    QTranslator qtTranslator;
    if (qtTranslator.load(QLocale(), "qt", "_", QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        app.installTranslator(&qtTranslator);
    }
    QTranslator binoTranslator;
    if (binoTranslator.load(QLocale(), "bino", "_", ":/i18n")) {
        app.installTranslator(&binoTranslator);
    }

    // Process command line
    QCommandLineParser parser;
    parser.setApplicationDescription(QCommandLineParser::tr("3D video player -- see https://bino3d.org"));
    parser.addPositionalArgument("[URL...]", QCommandLineParser::tr("Media to play."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({ "log-level",
            QCommandLineParser::tr("Set log level (%1).").arg("fatal, warning, info, debug, firehose"),
            "level" });
    parser.addOption({ "log-file",
            QCommandLineParser::tr("Set log file."),
            "file" });
    parser.addOption({ "read-commands",
            QCommandLineParser::tr("Read commands from a script file."), "script" });
    parser.addOption({ "opengles",
            QCommandLineParser::tr("Use OpenGL ES instead of Desktop OpenGL.") });
    parser.addOption({ "vr",
            QCommandLineParser::tr("Start in VR mode instead of GUI mode.")});
    parser.addOption({ "vr-screen",
            QCommandLineParser::tr("Set VR screen geometry, either as the special values 'united' or 'intersected', or "
            "as a comma-separated list of nine values representing three 3D coordinates that define a planar screen (bottom left, bottom right, top left), "
            "or as a an aspect ratio followed by the name of an OBJ file that contains the screen geometry with texture coordinates "
            "(example: '16:9,myscreen.obj')."),
            "screen" });
    parser.addOption({ "capture",
            QCommandLineParser::tr("Capture audio/video input from microphone and camera/screen/window.") });
    parser.addOption({ "list-audio-outputs",
            QCommandLineParser::tr("List audio outputs.") });
    parser.addOption({ "list-audio-inputs",
            QCommandLineParser::tr("List audio inputs.") });
    parser.addOption({ "list-video-inputs",
            QCommandLineParser::tr("List video inputs.") });
    parser.addOption({ "list-screen-inputs",
            QCommandLineParser::tr("List screen inputs.") });
    parser.addOption({ "list-window-inputs",
            QCommandLineParser::tr("List window inputs.") });
    parser.addOption({ "audio-output",
            QCommandLineParser::tr("Choose audio output via its index."),
            "ao" });
    parser.addOption({ "audio-input",
            QCommandLineParser::tr("Choose audio input via its index. Can be empty."),
            "ai" });
    parser.addOption({ "video-input",
            QCommandLineParser::tr("Choose video input via its index."),
            "vi" });
    parser.addOption({ "screen-input",
            QCommandLineParser::tr("Choose screen input via its index."),
            "si" });
    parser.addOption({ "window-input",
            QCommandLineParser::tr("Choose window input via its index."),
            "si" });
    parser.addOption({ "list-tracks",
            QCommandLineParser::tr("List all video, audio and subtitle tracks in the media.") });
    parser.addOption({ "preferred-audio",
            QCommandLineParser::tr("Set preferred audio track language (en, de, fr, ...)."),
            "lang" });
    parser.addOption({ "preferred-subtitle",
            QCommandLineParser::tr("Set preferred subtitle track language (en, de, fr, ...). Can be empty."),
            "lang" });
    parser.addOption({ "video-track",
            QCommandLineParser::tr("Choose video track via its index."), "track" });
    parser.addOption({ "audio-track",
            QCommandLineParser::tr("Choose audio track via its index."), "track" });
    parser.addOption({ "subtitle-track",
            QCommandLineParser::tr("Choose subtitle track via its index. Can be empty."),
            "track" });
    parser.addOption({ { "p", "playlist" },
            QCommandLineParser::tr("Load playlist."),
            "file" });
    parser.addOption({ { "l", "loop" },
            QCommandLineParser::tr("Set loop mode (%1).").arg("off, one, all"),
            "mode" });
    parser.addOption({ { "w", "wait" },
            QCommandLineParser::tr("Set wait mode (%1).").arg("off, on"),
            "mode" });
    parser.addOption({ { "i", "input" },
            QCommandLineParser::tr("Set input mode (%1).").arg("mono, "
            "top-bottom, top-bottom-half, bottom-top, bottom-top-half, "
            "left-right, left-right-half, right-left, right-left-half, "
            "alternating-left-right, alternating-right-left"),
            "mode" });
    parser.addOption({ { "o", "output" },
            QCommandLineParser::tr("Set output mode (%1).").arg("left, right, stereo, alternating, "
            "hdmi-frame-pack, "
            "left-right, left-right-half, right-left, right-left-half, "
            "top-bottom, top-bottom-half, bottom-top, bottom-top-half, "
            "even-odd-rows, even-odd-columns, checkerboard, "
            "red-cyan-dubois, red-cyan-full-color, red-cyan-half-color, red-cyan-monochrome, "
            "green-magenta-dubois, green-magenta-full-color, green-magenta-half-color, green-magenta-monochrome, "
            "amber-blue-dubois, amber-blue-full-color, amber-blue-half-color, amber-blue-monochrome, "
            "red-green-monochrome, red-blue-monochrome"),
            "mode" });
    parser.addOption({ "surround",
            QCommandLineParser::tr("Set surround mode (%1).").arg("360, 180, off"),
            "mode" });
    parser.addOption({ "surround-vfov",
            QCommandLineParser::tr("Set surround vertical field of view (default 50, range 5-115)."),
            "degrees" });
    parser.addOption({ { "S", "swap-eyes" },
            QCommandLineParser::tr("Swap left/right eye.") });
    parser.addOption({ { "f", "fullscreen" },
            QCommandLineParser::tr("Start in fullscreen mode.") });
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
            LOG_FATAL("%s", qPrintable(QCommandLineParser::tr("Invalid argument for option %1").arg("--log-level")));
            return 1;
        }
    }

    // Check if VR mode is available if requested
#ifndef WITH_QVR
    if (parser.isSet("vr")) {
        LOG_FATAL("%s", qPrintable(QCommandLineParser::tr("VR mode unavailable - recompile Bino with QVR support!")));
        return 1;
    }
#endif

    // Set modes
    SurroundMode surroundMode = Surround_Unknown;
    if (parser.isSet("surround")) {
        bool ok;
        surroundMode = surroundModeFromString(parser.value("surround"), &ok);
        if (!ok) {
            LOG_FATAL("%s", qPrintable(QCommandLineParser::tr("Invalid argument for option %1").arg("--surround")));
            return 1;
        }
    }
    float surroundVerticalFOV = 50.0f;
    if (parser.isSet("surround-vfov")) {
        bool ok;
        surroundVerticalFOV = parser.value("surround-vfov").toFloat(&ok);
        if (!ok || surroundVerticalFOV < 5.0f || surroundVerticalFOV > 115.0f) {
            LOG_FATAL("%s", qPrintable(QCommandLineParser::tr("Invalid argument for option %1").arg("--surround-vfov")));
            return 1;
        }
    }
    InputMode inputMode = Input_Unknown;
    if (parser.isSet("input")) {
        bool ok;
        inputMode = inputModeFromString(parser.value("input"), &ok);
        if (!ok) {
            LOG_FATAL("%s", qPrintable(QCommandLineParser::tr("Invalid argument for option %1").arg("--input")));
            return 1;
        }
    }
    OutputMode outputMode = Output_Red_Cyan_Dubois;
    if (parser.isSet("output")) {
        bool ok;
        outputMode = outputModeFromString(parser.value("output"), &ok);
        if (!ok) {
            LOG_FATAL("%s", qPrintable(QCommandLineParser::tr("Invalid argument for option %1").arg("--output")));
            return 1;
        }
    }

    // Lists of available devices. Initialize these lists only when necessary because
    // this can take some time!
    QList<QAudioDevice> audioOutputDevices;
    QList<QAudioDevice> audioInputDevices;
    QList<QCameraDevice> videoInputDevices;
    QList<QScreen*> screenInputDevices;
    QList<QCapturableWindow> windowInputDevices;

    // List devices
    bool deviceListRequested = false;
    if (parser.isSet("list-audio-outputs")) {
        audioOutputDevices = QMediaDevices::audioOutputs();
        if (audioOutputDevices.size() == 0) {
            LOG_REQUESTED("%s", qPrintable(QCommandLineParser::tr("No audio outputs available.")));
        } else {
            for (qsizetype i = 0; i < audioOutputDevices.size(); i++)
                LOG_REQUESTED("%s", qPrintable(QCommandLineParser::tr("Audio output %1: %2").arg(i).arg(audioOutputDevices[i].description())));
        }
        deviceListRequested = true;
    }
    if (parser.isSet("list-audio-inputs")) {
        audioInputDevices = QMediaDevices::audioInputs();
        if (audioInputDevices.size() == 0) {
            LOG_REQUESTED("%s", qPrintable(QCommandLineParser::tr("No audio inputs available.")));
        } else {
            for (qsizetype i = 0; i < audioInputDevices.size(); i++)
                LOG_REQUESTED("%s", qPrintable(QCommandLineParser::tr("Audio input %1: %2").arg(i).arg(audioInputDevices[i].description())));
        }
        deviceListRequested = true;
    }
    if (parser.isSet("list-video-inputs")) {
        videoInputDevices = QMediaDevices::videoInputs();
        if (videoInputDevices.size() == 0) {
            LOG_REQUESTED("%s", qPrintable(QCommandLineParser::tr("No video inputs available.")));
        } else {
            for (qsizetype i = 0; i < videoInputDevices.size(); i++)
                LOG_REQUESTED("%s", qPrintable(QCommandLineParser::tr("Video input %1: %2").arg(i).arg(videoInputDevices[i].description())));
        }
        deviceListRequested = true;
    }
    if (parser.isSet("list-screen-inputs")) {
        screenInputDevices = QGuiApplication::screens();
        if (screenInputDevices.size() == 0) {
            LOG_REQUESTED("%s", qPrintable(QCommandLineParser::tr("No screen inputs available.")));
        } else {
            for (qsizetype i = 0; i < screenInputDevices.size(); i++)
                LOG_REQUESTED("%s", qPrintable(QCommandLineParser::tr("Screen input %1: %2").arg(i).arg(screenInputDevices[i]->name())));
        }
        deviceListRequested = true;
    }
    if (parser.isSet("list-window-inputs")) {
        windowInputDevices = QWindowCapture::capturableWindows();
        if (windowInputDevices.size() == 0) {
            LOG_REQUESTED("%s", qPrintable(QCommandLineParser::tr("No window inputs available.")));
        } else {
            for (qsizetype i = 0; i < windowInputDevices.size(); i++)
                LOG_REQUESTED("%s", qPrintable(QCommandLineParser::tr("Window input %1: %2").arg(i).arg(windowInputDevices[i].description())));
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
    int screenInputDeviceIndex = -1;
    int windowInputDeviceIndex = -1;
    if (parser.isSet("audio-output")) {
        audioOutputDevices = QMediaDevices::audioOutputs();
        bool ok;
        int ao = parser.value("audio-output").toInt(&ok);
        if (ok && ao >= 0 && ao < audioOutputDevices.size()) {
            audioOutputDeviceIndex = ao;
        } else {
            LOG_FATAL("%s", qPrintable(QCommandLineParser::tr("Invalid argument for option %1").arg("--audio-output")));
            return 1;
        }
    }
    if (parser.isSet("capture")) {
        bool ok;
        if (parser.isSet("audio-input")) {
            if (parser.value("audio-input").length() == 0) {
                audioInputDeviceIndex = -2; // this means no audio input
            } else {
                if (audioInputDevices.size() == 0)
                    audioInputDevices = QMediaDevices::audioInputs();
                int ai = parser.value("audio-input").toInt(&ok);
                if (ok && ai >= 0 && ai < audioInputDevices.size()) {
                    audioInputDeviceIndex = ai;
                } else {
                    LOG_FATAL("%s", qPrintable(QCommandLineParser::tr("Invalid argument for option %1").arg("--audio-input")));
                    return 1;
                }
            }
        }
        if (parser.isSet("video-input")) {
            if (videoInputDevices.size() == 0)
                videoInputDevices = QMediaDevices::videoInputs();
            int vi = parser.value("video-input").toInt(&ok);
            if (ok && vi >= 0 && vi < videoInputDevices.size()) {
                videoInputDeviceIndex = vi;
            } else {
                LOG_FATAL("%s", qPrintable(QCommandLineParser::tr("Invalid argument for option %1").arg("--video-input")));
                return 1;
            }
        }
        if (parser.isSet("screen-input")) {
            if (screenInputDevices.size() == 0)
                screenInputDevices = QGuiApplication::screens();
            int vi = parser.value("screen-input").toInt(&ok);
            if (ok && vi >= 0 && vi < screenInputDevices.size()) {
                screenInputDeviceIndex = vi;
            } else {
                LOG_FATAL("%s", qPrintable(QCommandLineParser::tr("Invalid argument for option %1").arg("--screen-input")));
                return 1;
            }
        }
        if (parser.isSet("window-input")) {
            if (windowInputDevices.size() == 0)
                windowInputDevices = QWindowCapture::capturableWindows();
            int vi = parser.value("window-input").toInt(&ok);
            if (ok && vi >= 0 && vi < windowInputDevices.size()) {
                windowInputDeviceIndex = vi;
            } else {
                LOG_FATAL("%s", qPrintable(QCommandLineParser::tr("Invalid argument for option %1").arg("--window-input")));
                return 1;
            }
        }
    }

    // Get playlist.
    Playlist playlist;
    if (parser.isSet("preferred-audio")) {
        QLocale::Language lang = QLocale::codeToLanguage(parser.value("preferred-audio"));
        if (lang == QLocale::AnyLanguage) {
            LOG_FATAL("%s", qPrintable(QCommandLineParser::tr("Invalid argument for option %1").arg("--preferred-audio")));
            return 1;
        } else {
            playlist.setPreferredAudio(lang);
        }
    }
    if (parser.isSet("preferred-subtitle")) {
        if (parser.value("preferred-subtitle").length() == 0) {
            playlist.setWantSubtitle(false);
        } else {
            QLocale::Language lang = QLocale::codeToLanguage(parser.value("preferred-subtitle"));
            if (lang == QLocale::AnyLanguage) {
                LOG_FATAL("%s", qPrintable(QCommandLineParser::tr("Invalid argument for option %1").arg("--preferred-subtitle")));
                return 1;
            } else {
                playlist.setWantSubtitle(true);
                playlist.setPreferredSubtitle(lang);
            }
        }
    }
    if (parser.isSet("loop")) {
        bool ok;
        LoopMode loopMode = loopModeFromString(parser.value("loop"), &ok);
        if (!ok) {
            LOG_FATAL("%s", qPrintable(QCommandLineParser::tr("Invalid argument for option %1").arg("--loop")));
            return 1;
        }
        playlist.setLoopMode(loopMode);
    }
    if (parser.isSet("wait")) {
        bool ok;
        WaitMode waitMode = waitModeFromString(parser.value("wait"), &ok);
        if (!ok) {
            LOG_FATAL("%s", qPrintable(QCommandLineParser::tr("Invalid argument for option %1").arg("--wait")));
            return 1;
        }
        playlist.setWaitMode(waitMode);
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
            LOG_FATAL("%s", qPrintable(QCommandLineParser::tr("Invalid argument for option %1").arg("--video-track")));
            return 1;
        }
    }
    if (parser.isSet("audio-track")) {
        bool ok;
        int at = parser.value("audio-track").toInt(&ok);
        if (ok && at >= 0) {
            audioTrack = at;
        } else {
            LOG_FATAL("%s", qPrintable(QCommandLineParser::tr("Invalid argument for option %1").arg("--audio-track")));
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
                LOG_FATAL("%s", qPrintable(QCommandLineParser::tr("Invalid argument for option %1").arg("--subtitle-track")));
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
                LOG_WARNING("%s", qPrintable(QCommandLineParser::tr("File does not exist: %1").arg(parser.positionalArguments()[i])));
                continue;
            }
        }
        playlist.append(PlaylistEntry(url, inputMode, surroundMode,
                    videoTrack, audioTrack, subtitleTrack));
    }
    if (parser.positionalArguments().length() > 0 && playlist.length() == 0) {
        return 1;
    }
    if (parser.isSet("playlist")) {
        QString errStr;
        if (!playlist.load(parser.value("playlist"), errStr)) {
            LOG_FATAL("%s", qPrintable(QCommandLineParser::tr("%1: %2")
                        .arg(parser.value("playlist"))
                        .arg(errStr.isEmpty() ? QString("invalid playlist file") : errStr)));
            return 1;
        }
    }
    if (!parser.isSet("wait")) {
        playlist.setWaitModeAuto();
    }
    if (parser.isSet("capture") && playlist.length() > 0) {
        LOG_FATAL("%s", qPrintable(QCommandLineParser::tr("Cannot capture and play URL at the same time.")));
        return 1;
    }

    // List tracks
    if (parser.isSet("list-tracks")) {
        MetaData metaData;
        for (qsizetype i = 0; i < playlist.length(); i++) {
            if (!metaData.detectCached(playlist.entries()[i].url))
                return 1;
            LOG_REQUESTED("%s", qPrintable(metaData.url.toString()));
            for (qsizetype l = 0; l < metaData.global.keys().size(); l++) {
                LOG_REQUESTED("    %s: %s",
                        qPrintable(QMediaMetaData::metaDataKeyToString(metaData.global.keys()[l])),
                        qPrintable(metaData.global.stringValue(metaData.global.keys()[l])));
            }
            for (int k = 0; k < 3; k++) {
                QString trackType =
                    (k == 0 ? QCommandLineParser::tr("video")
                     : k == 1 ? QCommandLineParser::tr("audio")
                     : QCommandLineParser::tr("subtitle"));
                const QList<QMediaMetaData>& metaDataList =
                    (k == 0 ? metaData.videoTracks : k == 1 ? metaData.audioTracks : metaData.subtitleTracks);
                if (metaDataList.size() == 0) {
                    LOG_REQUESTED("  %s", qPrintable(QCommandLineParser::tr("no %1 tracks").arg(trackType)));
                } else {
                    for (qsizetype l = 0; l < metaDataList.size(); l++) {
                        LOG_REQUESTED("  %s", qPrintable(QCommandLineParser::tr("%1 track %2").arg(qPrintable(trackType)).arg(l)));
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
    Bino::ScreenType screenType = Bino::ScreenGeometry;
    Screen screen; // default screen for non-VR mode
    if (parser.isSet("vr")) {
        float screenCenterHeight = 1.76f - 0.15f; // does not matter, never used
#ifdef WITH_QVR
        screenCenterHeight = QVRObserverConfig::defaultEyeHeight;
#endif
        screen = Screen( // default screen for VR mode
                QVector3D(-16.0f / 9.0f, -1.0f + screenCenterHeight, -8.0f),
                QVector3D(+16.0f / 9.0f, -1.0f + screenCenterHeight, -8.0f),
                QVector3D(-16.0f / 9.0f, +1.0f + screenCenterHeight, -8.0f));
        if (parser.isSet("vr-screen")) {
            QStringList paramList = parser.value("vr-screen").split(',');
            float values[9];
            if (parser.value("vr-screen") == "united") {
                screenType = Bino::ScreenUnited;
            } else if (parser.value("vr-screen") == "intersected") {
                screenType = Bino::ScreenIntersected;
            } else if (paramList.length() == 9
                    && 9 == std::sscanf(qPrintable(parser.value("vr-screen")),
                        "%f,%f,%f,%f,%f,%f,%f,%f,%f",
                        values + 0, values + 1, values + 2,
                        values + 3, values + 4, values + 5,
                        values + 6, values + 7, values + 8)) {
                screenType = Bino::ScreenGeometry;
                screen = Screen(
                        QVector3D(values[0], values[1], values[2]),
                        QVector3D(values[3], values[4], values[5]),
                        QVector3D(values[6], values[7], values[8]));
            } else if (paramList.length() == 2) {
                screenType = Bino::ScreenGeometry;
                float ar;
                float ar2[2];
                if (2 == std::sscanf(qPrintable(paramList[0]), "%f:%f", ar2 + 0, ar2 + 1)) {
                    ar = ar2[0] / ar2[1];
                } else if (1 != std::sscanf(qPrintable(paramList[0]), "%f", &ar)) {
                    LOG_FATAL("%s", qPrintable(QCommandLineParser::tr("Invalid argument for option %1").arg("--vr-screen")));
                    return 1;
                }
                screen = Screen(paramList[1], "", ar);
                if (screen.indices.size() == 0)
                    return 1;
            } else {
                LOG_FATAL("%s", qPrintable(QCommandLineParser::tr("Invalid argument for option %1").arg("--vr-screen")));
                return 1;
            }
        } else {
            LOG_DEBUG("using default VR screen");
        }
    }

    // Initialize the command interpreter (but don't start it yet)
    CommandInterpreter cmdInterpreter;
    if (parser.isSet("read-commands") && !cmdInterpreter.init(parser.value("read-commands"))) {
        return 1;
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
    bool wantOpenGLES = parser.isSet("opengles");
#if defined Q_OS_LINUX
# if defined Q_PROCESSOR_ARM
    // Use OpenGL ES by default on Linux/ARM, for Raspberry Pi 5 which has crappy desktop GL.
    // But don't do this on other systems. For example, MacOS on ARM needs desktop GL.
    wantOpenGLES = true;
# endif
#endif
    if (wantOpenGLES)
        format.setRenderableType(QSurfaceFormat::OpenGLES);
    initializeIsOpenGLES(format);
    if (IsOpenGLES) {
        format.setVersion(3, 1);
    } else {
        format.setProfile(QSurfaceFormat::CoreProfile);
        format.setVersion(3, 3);
    }
    if (guiMode) {
        format.setStereo(true); // Try to get quad-buffered stereo; it's ok if this fails
    }
    QSurfaceFormat::setDefaultFormat(format);

    // Initialize Bino (in VR mode: only from the main process!)
    Bino bino(screenType, screen, parser.isSet("swap-eyes"));
    if (guiMode || !vrChildProcess) {
        bino.initializeOutput(audioOutputDeviceIndex >= 0
                ? audioOutputDevices[audioOutputDeviceIndex]
                : QMediaDevices::defaultAudioOutput());
        if (parser.isSet("capture")) {
            if (screenInputDeviceIndex < 0 && windowInputDeviceIndex < 0) {
                bino.startCaptureModeCamera(audioInputDeviceIndex >= -1,
                        audioInputDeviceIndex >= 0
                        ? audioInputDevices[audioInputDeviceIndex]
                        : QMediaDevices::defaultAudioInput(),
                        videoInputDeviceIndex >= 0
                        ? videoInputDevices[videoInputDeviceIndex]
                        : QMediaDevices::defaultVideoInput());
            } else if (screenInputDeviceIndex >= 0) {
                bino.startCaptureModeScreen(audioInputDeviceIndex >= -1,
                        audioInputDeviceIndex >= 0
                        ? audioInputDevices[audioInputDeviceIndex]
                        : QMediaDevices::defaultAudioInput(),
                        screenInputDevices[screenInputDeviceIndex]);
            } else {
                bino.startCaptureModeWindow(audioInputDeviceIndex >= -1,
                        audioInputDeviceIndex >= 0
                        ? audioInputDevices[audioInputDeviceIndex]
                        : QMediaDevices::defaultAudioInput(),
                        windowInputDevices[windowInputDeviceIndex]);
            }
        } else {
            bino.startPlaylistMode();
        }
    }

    // Start VR or GUI mode
    if (vrMode) {
#ifdef WITH_QVR
        BinoQVRApp qvrapp;
        if (!manager.init(&qvrapp)) {
            LOG_FATAL("%s", qPrintable(QCommandLineParser::tr("Cannot initialize QVR manager")));
            return 1;
        }
        playlist.start();
        cmdInterpreter.start();
        return app.exec();
#else
        return 1;
#endif
    } else {
        Gui gui(outputMode, surroundVerticalFOV, parser.isSet("fullscreen"));
        gui.show();
        // wait for several seconds to process all events before starting
        // the playlist, because otherwise playing might be finished before
        // the first frame rendering, e.g. if you just want to "play" an image
        QGuiApplication::processEvents(QEventLoop::AllEvents, 3000);
        playlist.start();
        cmdInterpreter.start();
        return app.exec();
    }
}
