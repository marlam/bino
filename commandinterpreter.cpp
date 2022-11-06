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

#include <QCommandLineParser>
#include <QMediaDevices>

#include "commandinterpreter.hpp"
#include "modes.hpp"
#include "bino.hpp"
#include "gui.hpp"
#include "log.hpp"


CommandInterpreter::CommandInterpreter() :
    _file(),
    _lineBuf(),
    _lineIndex(-1),
    _waitForStop(false)
{
}

bool CommandInterpreter::init(const QString& fileName)
{
    _file.setFileName(fileName);
    if (!_file.open(QIODeviceBase::ReadOnly | QIODeviceBase::Text)) {
        LOG_FATAL("%s", qPrintable(tr("Cannot open %1: %2)").arg(fileName).arg(_file.errorString())));
        return false;
    }
    _lineBuf.resize(2048);
    _lineIndex = 0;
    _waitForStop = false;
    connect(&_timer, SIGNAL(timeout()), this, SLOT(processNextCommand()));
    _waitTimer.setSingleShot(true);
    return true;
}

void CommandInterpreter::start()
{
    _timer.start(20); // every 20 msecs = 50 times per second
}

static int getOnOff(const QString& s)
{
    if (s == "on")
        return 1;
    else if (s == "off")
        return 0;
    else
        return -1;
}

void CommandInterpreter::processNextCommand()
{
    if (_waitTimer.isActive())
        return;
    if (_waitForStop && !Bino::instance()->stopped())
        return;
    _waitForStop = false;

    _lineIndex++;
    _file.startTransaction();
    qint64 lineLen = _file.readLine(_lineBuf.data(), _lineBuf.size());
    if (lineLen < 0 || (lineLen == _lineBuf.size() - 1 && _lineBuf[lineLen - 1] != '\n')) {
        _timer.stop();
        LOG_FATAL("%s", qPrintable(tr("Cannot read command from %1").arg(_file.fileName())));
        return;
    }
    if (lineLen > 0 && _lineBuf[lineLen - 1] != '\n') {
        // an incomplete line was read; roll back and try again
        // on the next call to this function
        _file.rollbackTransaction();
        return;
    }
    _file.commitTransaction();
    if (lineLen == 0) {
        return;
    }
    QString cmd = QString(_lineBuf.data()).simplified();
    LOG_DEBUG("Command line: %s", qPrintable(cmd));

    // empty lines and comments
    if (cmd.length() == 0 || cmd[0] == '#')
        return;

    if (cmd == "quit") {
        Bino::instance()->quit();
    } else if (cmd.startsWith("wait ")) {
        if (cmd.mid(5) == "stop") {
            _waitForStop = true;
        } else {
            bool ok;
            float val = cmd.mid(5).toFloat(&ok);
            if (!ok) {
                LOG_FATAL("%s", qPrintable(tr("Invalid argument in %1 line %2").arg(_file.fileName()).arg(_lineIndex)));
            } else {
                _waitTimer.start(val * 1000);
            }
        }
    } else if (cmd.startsWith("open ")) {
        QCommandLineParser parser;
        parser.addPositionalArgument("URL", "");
        parser.addOption({ "input", "", "" });
        parser.addOption({ "360", "", "" });
        parser.addOption({ "video-track", "", "" });
        parser.addOption({ "audio-track", "", "" });
        parser.addOption({ "subtitle-track", "", "" });
        if (!parser.parse(cmd.split(' '))) {
            LOG_FATAL("%s", qPrintable(tr("Invalid argument in %1 line %2").arg(_file.fileName()).arg(_lineIndex)));
        } else if (parser.positionalArguments().length() == 0 || parser.positionalArguments().length() > 1) {
            LOG_FATAL("%s", qPrintable(tr("Invalid argument in %1 line %2").arg(_file.fileName()).arg(_lineIndex)));
        } else {
            QUrl url = parser.positionalArguments()[0];
            InputMode inputMode = Input_Unknown;
            ThreeSixtyMode threeSixtyMode = ThreeSixty_Unknown;
            int videoTrack = PlaylistEntry::DefaultTrack;
            int audioTrack = PlaylistEntry::DefaultTrack;
            int subtitleTrack = PlaylistEntry::NoTrack;
            bool ok = true;
            if (parser.isSet("input")) {
                inputMode = inputModeFromString(parser.value("input"), &ok);
                if (!ok)
                    LOG_FATAL("%s", qPrintable(QCommandLineParser::tr("Invalid argument for option %1").arg("--input")));
            }
            if (parser.isSet("360")) {
                threeSixtyMode = threeSixtyModeFromString(parser.value("360"), &ok);
                if (!ok)
                    LOG_FATAL("%s", qPrintable(tr("Invalid argument in %1 line %2").arg(_file.fileName()).arg(_lineIndex)));
            }
            if (parser.isSet("video-track")) {
                int t = parser.value("video-track").toInt(&ok);
                if (ok && t >= 0) {
                    videoTrack = t;
                } else {
                    LOG_FATAL("%s", qPrintable(tr("Invalid argument in %1 line %2").arg(_file.fileName()).arg(_lineIndex)));
                    ok = false;
                }
            }
            if (parser.isSet("audio-track")) {
                int t = parser.value("audio-track").toInt(&ok);
                if (ok && t >= 0) {
                    audioTrack = t;
                } else {
                    LOG_FATAL("%s", qPrintable(tr("Invalid argument in %1 line %2").arg(_file.fileName()).arg(_lineIndex)));
                    ok = false;
                }
            }
            if (parser.isSet("subtitle-track") && parser.value("subtitle-track").length() > 0) {
                int t = parser.value("subtitle-track").toInt(&ok);
                if (ok && t >= 0) {
                    subtitleTrack = t;
                } else {
                    LOG_FATAL("%s", qPrintable(tr("Invalid argument in %1 line %2").arg(_file.fileName()).arg(_lineIndex)));
                    ok = false;
                }
            }
            if (ok) {
                Bino::instance()->startPlaylistMode();
                Playlist::instance()->clear();
                Playlist::instance()->append(PlaylistEntry(url, inputMode, threeSixtyMode, videoTrack, audioTrack, subtitleTrack));
                Playlist::instance()->start();
            }
        }
    } else if (cmd.startsWith("capture") && (cmd[7] == '\0' || cmd[7] == ' ')) {
        QCommandLineParser parser;
        parser.addOption({ "audio-input", "", "" });
        parser.addOption({ "video-input", "", "" });
        if (!parser.parse(cmd.split(' '))) {
            LOG_FATAL("%s", qPrintable(tr("Invalid argument in %1 line %2").arg(_file.fileName()).arg(_lineIndex)));
        } else {
            bool ok;
            QList<QAudioDevice> audioInputDevices;
            QList<QCameraDevice> videoInputDevices;
            int audioInputDeviceIndex = -1;
            int videoInputDeviceIndex = -1;
            if (parser.isSet("audio-input")) {
                if (parser.value("audio-input").length() == 0) {
                    audioInputDeviceIndex = -2; // this means no audio input
                } else {
                    audioInputDevices = QMediaDevices::audioInputs();
                    int ai = parser.value("audio-input").toInt(&ok);
                    if (ok && ai >= 0 && ai < audioInputDevices.size()) {
                        audioInputDeviceIndex = ai;
                    } else {
                        LOG_FATAL("%s", qPrintable(tr("Invalid argument in %1 line %2").arg(_file.fileName()).arg(_lineIndex)));
                        ok = false;
                    }
                }
            }
            if (parser.isSet("video-input")) {
                videoInputDevices = QMediaDevices::videoInputs();
                int vi = parser.value("video-input").toInt(&ok);
                if (ok && vi >= 0 && vi < videoInputDevices.size()) {
                    videoInputDeviceIndex = vi;
                } else {
                    LOG_FATAL("%s", qPrintable(tr("Invalid argument in %1 line %2").arg(_file.fileName()).arg(_lineIndex)));
                    ok = false;
                }
            }
            if (ok) {
                Bino::instance()->startCaptureMode(audioInputDeviceIndex >= -1,
                        audioInputDeviceIndex >= 0
                        ? audioInputDevices[audioInputDeviceIndex]
                        : QMediaDevices::defaultAudioInput(),
                        videoInputDeviceIndex >= 0
                        ? videoInputDevices[videoInputDeviceIndex]
                        : QMediaDevices::defaultVideoInput());
            }
        }
    } else if (cmd.startsWith("set-output-mode ")) {
        bool ok;
        OutputMode outputMode = outputModeFromString(cmd.mid(16), &ok);
        if (!ok) {
            LOG_FATAL("%s", qPrintable(tr("Invalid argument in %1 line %2").arg(_file.fileName()).arg(_lineIndex)));
        } else {
            Gui* gui = Gui::instance();
            if (gui)
                gui->setOutputMode(outputMode);
        }
    } else if (cmd == "play") {
        Bino::instance()->play();
    } else if (cmd == "stop") {
        Bino::instance()->stop();
    } else if (cmd == "pause") {
        Bino::instance()->pause();
    } else if (cmd == "toggle-pause") {
        Bino::instance()->togglePause();
    } else if (cmd.startsWith("set-position ")) {
        bool ok;
        float val = cmd.mid(13).toFloat(&ok);
        if (!ok) {
            LOG_FATAL("%s", qPrintable(tr("Invalid argument in %1 line %2").arg(_file.fileName()).arg(_lineIndex)));
        } else {
            Bino::instance()->setPosition(val);
        }
    } else if (cmd.startsWith("seek ")) {
        bool ok;
        float val = cmd.mid(5).toFloat(&ok);
        if (!ok) {
            LOG_FATAL("%s", qPrintable(tr("Invalid argument in %1 line %2").arg(_file.fileName()).arg(_lineIndex)));
        } else {
            Bino::instance()->seek(val);
        }
    } else if (cmd.startsWith("set-swap-eyes ")) {
        int onoff = getOnOff(cmd.mid(14));
        if (onoff < 0) {
            LOG_FATAL("%s", qPrintable(tr("Invalid argument in %1 line %2").arg(_file.fileName()).arg(_lineIndex)));
        } else {
            Bino::instance()->setSwapEyes(onoff);
        }
    } else if (cmd == "toggle-swap-eyes") {
        Bino::instance()->toggleSwapEyes();
    } else if (cmd.startsWith("set-fullscreen ")) {
        int onoff = getOnOff(cmd.mid(15));
        if (onoff < 0) {
            LOG_FATAL("%s", qPrintable(tr("Invalid argument in %1 line %2").arg(_file.fileName()).arg(_lineIndex)));
        } else {
            Gui* gui = Gui::instance();
            if (gui)
                gui->setFullscreen(onoff);
        }
    } else if (cmd == "toggle-fullscreen") {
        Gui* gui = Gui::instance();
        if (gui)
            gui->viewToggleFullscreen();
    } else if (cmd.startsWith("set-mute ")) {
        int onoff = getOnOff(cmd.mid(9));
        if (onoff < 0) {
            LOG_FATAL("%s", qPrintable(tr("Invalid argument in %1 line %2").arg(_file.fileName()).arg(_lineIndex)));
        } else {
            Bino::instance()->setMute(onoff);
        }
    } else if (cmd == "toggle-mute") {
        Bino::instance()->toggleMute();
    } else if (cmd.startsWith("set-volume ")) {
        bool ok;
        float val = cmd.mid(11).toFloat(&ok);
        if (!ok) {
            LOG_FATAL("%s", qPrintable(tr("Invalid argument in %1 line %2").arg(_file.fileName()).arg(_lineIndex)));
        } else {
            Bino::instance()->setVolume(val);
        }
    } else if (cmd.startsWith("adjust-volume ")) {
        bool ok;
        float val = cmd.mid(14).toFloat(&ok);
        if (!ok) {
            LOG_FATAL("%s", qPrintable(tr("Invalid argument in %1 line %2").arg(_file.fileName()).arg(_lineIndex)));
        } else {
            Bino::instance()->changeVolume(val);
        }
    } else {
        LOG_FATAL("%s", qPrintable(tr("Invalid command %1").arg(cmd)));
    }
}
