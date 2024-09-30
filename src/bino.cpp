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

#include <QFont>
#include <QFontMetrics>
#include <QTextLayout>
#include <QPainter>

#include "bino.hpp"
#include "log.hpp"
#include "tools.hpp"
#include "digestiblemedia.hpp"
#include "metadata.hpp"


static Bino* binoSingleton = nullptr;

Bino::Bino(ScreenType screenType, const Screen& screen, bool swapEyes) :
    _wantExit(false),
    _videoSink(nullptr),
    _audioOutput(nullptr),
    _player(nullptr),
    _audioInput(nullptr),
    _videoInput(nullptr),
#if (QT_VERSION >= QT_VERSION_CHECK(6, 5, 0))
    _screenInput(nullptr),
#endif
#if (QT_VERSION >= QT_VERSION_CHECK(6, 6, 0))
    _windowInput(nullptr),
#endif
    _captureSession(nullptr),
    _lastFrameInputMode(Input_Unknown),
    _lastFrameSurroundMode(Surround_Unknown),
    _screenType(screenType),
    _screen(screen),
    _frameIsNew(false),
    _frameWasSerialized(true),
    _swapEyes(swapEyes)
{
    Q_ASSERT(!binoSingleton);
    binoSingleton = this;
}

Bino::~Bino()
{
    delete _videoSink;
    delete _audioOutput;
    delete _player;
    delete _audioInput;
    delete _videoInput;
#if (QT_VERSION >= QT_VERSION_CHECK(6, 5, 0))
    delete _screenInput;
#endif
#if (QT_VERSION >= QT_VERSION_CHECK(6, 6, 0))
    delete _windowInput;
#endif
    delete _captureSession;
    binoSingleton = nullptr;
}

Bino* Bino::instance()
{
    return binoSingleton;
}

void Bino::initializeOutput(const QAudioDevice& audioOutputDevice)
{
    _videoSink = new VideoSink(&_frame, &_extFrame, &_frameIsNew);
    connect(_videoSink, &VideoSink::newVideoFrame, [=]() { emit newVideoFrame(); });
    connect(_videoSink, &VideoSink::newVideoFrame, [=]() { _frameWasSerialized = false; });
    _audioOutput = new QAudioOutput;
    _audioOutput->setDevice(audioOutputDevice);
}

void Bino::startPlaylistMode()
{
    if (playlistMode()) {
        return;
    }
    if (captureMode()) {
        stopCaptureMode();
    }

    connect(Playlist::instance(), SIGNAL(mediaChanged(PlaylistEntry)), this, SLOT(mediaChanged(PlaylistEntry)));

    _player = new QMediaPlayer;
    _player->setVideoOutput(_videoSink);
    _player->setAudioOutput(_audioOutput);
    _player->connect(_player, &QMediaPlayer::errorOccurred,
            [=](QMediaPlayer::Error /* error */, const QString& errorString) {
            LOG_WARNING("%s", qPrintable(tr("Media player error: %1").arg(errorString)));
            });
    _player->connect(_player, &QMediaPlayer::playbackStateChanged,
            [=](QMediaPlayer::PlaybackState state) {
            LOG_DEBUG("Playback state changed to %s",
                    state == QMediaPlayer::StoppedState ? "stopped"
                    : state == QMediaPlayer::PlayingState ? "playing"
                    : state == QMediaPlayer::PausedState ? "paused"
                    : "unknown");
            if (state == QMediaPlayer::StoppedState)
                Playlist::instance()->mediaEnded();
            });

    emit stateChanged();
}

void Bino::stopPlaylistMode()
{
    if (_player) {
        delete _player;
        _player = nullptr;
        emit stateChanged();
    }
}

void Bino::startCaptureMode(bool withAudioInput, const QAudioDevice& audioInputDevice)
{
    if (playlistMode())
        stopPlaylistMode();
    if (captureMode())
        stopCaptureMode();

    _captureSession = new QMediaCaptureSession;
    _captureSession->setAudioOutput(_audioOutput);
    _captureSession->setVideoSink(_videoSink);
    if (withAudioInput) {
        _audioInput = new QAudioInput;
        _audioInput->setDevice(audioInputDevice);
        _captureSession->setAudioInput(_audioInput);
    }
}

void Bino::startCaptureModeCamera(
        bool withAudioInput,
        const QAudioDevice& audioInputDevice,
        const QCameraDevice& videoInputDevice)
{
    startCaptureMode(withAudioInput, audioInputDevice);
    _videoInput = new QCamera;
    _videoInput->setCameraDevice(videoInputDevice);
    _captureSession->setCamera(_videoInput);
    _videoInput->setActive(true);
    emit stateChanged();
}

#if (QT_VERSION >= QT_VERSION_CHECK(6, 5, 0))
void Bino::startCaptureModeScreen(
        bool withAudioInput,
        const QAudioDevice& audioInputDevice,
        QScreen* screenInputDevice)
{
    startCaptureMode(withAudioInput, audioInputDevice);
    _screenInput = new QScreenCapture;
    _screenInput->setScreen(screenInputDevice);
    _captureSession->setScreenCapture(_screenInput);
    _screenInput->setActive(true);
    emit stateChanged();
}
#endif

#if (QT_VERSION >= QT_VERSION_CHECK(6, 6, 0))
void Bino::startCaptureModeWindow(
        bool withAudioInput,
        const QAudioDevice& audioInputDevice,
        const QCapturableWindow& windowInputDevice)
{
    startCaptureMode(withAudioInput, audioInputDevice);
    _windowInput = new QWindowCapture;
    _windowInput->setWindow(windowInputDevice);
    _captureSession->setWindowCapture(_windowInput);
    _windowInput->setActive(true);
    emit stateChanged();
}
#endif

void Bino::stopCaptureMode()
{
    if (_captureSession) {
        delete _captureSession;
        _captureSession = nullptr;
        delete _videoInput;
        _videoInput = nullptr;
#if (QT_VERSION >= QT_VERSION_CHECK(6, 5, 0))
        delete _screenInput;
        _screenInput = nullptr;
#endif
#if (QT_VERSION >= QT_VERSION_CHECK(6, 6, 0))
        delete _windowInput;
        _windowInput = nullptr;
#endif
        if (_audioInput) {
            delete _audioInput;
            _audioInput = nullptr;
        }
        emit stateChanged();
    }
}

bool Bino::playlistMode() const
{
    return _player;
}

bool Bino::captureMode() const
{
    return _captureSession;
}

void Bino::mediaChanged(PlaylistEntry entry)
{
    if (!playlistMode())
        return;
    if (entry.noMedia()) {
        _player->stop();
    } else {
        // Get meta data
        MetaData metaData;
        metaData.detectCached(entry.url);
        // Special handling of files that cannot be digested by QtMultimedia directly
        QUrl digestibleUrl = digestibleMediaUrl(entry.url);
        // Set new source
        _player->setSource(digestibleUrl);
        if (entry.videoTrack >= 0) {
            _player->setActiveVideoTrack(entry.videoTrack);
        }
        if (entry.audioTrack >= 0) {
            _player->setActiveAudioTrack(entry.audioTrack);
        } else if (Playlist::instance()->preferredAudio() != QLocale::AnyLanguage) {
            int audioTrack = -1;
            for (int i = 0; i < int(metaData.audioTracks.length()); i++) {
                QLocale audioLanguage = metaData.audioTracks[i].value(QMediaMetaData::Language).toLocale();
                if (audioLanguage == Playlist::instance()->preferredAudio()) {
                    audioTrack = i;
                    break;
                }
            }
            if (audioTrack >= 0) {
                _player->setActiveVideoTrack(entry.audioTrack);
            }
        }
        if (entry.subtitleTrack >= 0) {
            _player->setActiveSubtitleTrack(entry.subtitleTrack);
        } else if (entry.subtitleTrack == PlaylistEntry::NoTrack) {
            // do nothing
        } else if (metaData.subtitleTracks.size() > 0 && Playlist::instance()->wantSubtitle()) {
            int subtitleTrack = 0;
            for (int i = 0; i < int(metaData.subtitleTracks.length()); i++) {
                QLocale subtitleLanguage = metaData.subtitleTracks[i].value(QMediaMetaData::Language).toLocale();
                if (subtitleLanguage == Playlist::instance()->preferredSubtitle()) {
                    subtitleTrack = i;
                    break;
                }
            }
            _player->setActiveSubtitleTrack(subtitleTrack);
        }
        _player->play();
        _videoSink->newUrl(entry.url, entry.inputMode, entry.surroundMode);
    }
    emit stateChanged();
}

void Bino::quit()
{
    stop();
    _wantExit = true;
    emit wantQuit();
}

void Bino::seek(qint64 milliseconds)
{
    if (!playlistMode())
        return;
    _player->setPosition(_player->position() + milliseconds);
}

void Bino::setPosition(float pos)
{
    if (!playlistMode())
        return;
    _player->setPosition(pos * _player->duration());
}

void Bino::togglePause()
{
    if (!playlistMode())
        return;
    if (_player->playbackState() == QMediaPlayer::PlayingState) {
        _player->pause();
        emit stateChanged();
    } else if (_player->playbackState() == QMediaPlayer::PausedState) {
        _player->play();
        emit stateChanged();
    }
}

void Bino::pause()
{
    if (!playlistMode())
        return;
    if (_player->playbackState() == QMediaPlayer::PlayingState) {
        _player->pause();
        emit stateChanged();
    }
}

void Bino::play()
{
    if (!playlistMode())
        return;
    if (_player->playbackState() != QMediaPlayer::PlayingState) {
        _player->play();
        emit stateChanged();
    }
}

void Bino::setMute(bool m)
{
    bool isMuted = _player->audioOutput()->isMuted();
    if (m != isMuted) {
        _audioOutput->setMuted(m);
        emit stateChanged();
    }
}

void Bino::toggleMute()
{
    _audioOutput->setMuted(!_player->audioOutput()->isMuted());
    emit stateChanged();
}

void Bino::setVolume(float vol)
{
    _audioOutput->setVolume(vol);
}

void Bino::changeVolume(float offset)
{
    _audioOutput->setVolume(_player->audioOutput()->volume() + offset);
}

void Bino::stop()
{
    if (!playlistMode())
        return;
    if (_player->playbackState() == QMediaPlayer::StoppedState) {
        _player->stop();
        emit stateChanged();
    }
}

void Bino::setSwapEyes(bool s)
{
    if (_swapEyes != s) {
        _swapEyes = s;
        emit stateChanged();
    }
}

void Bino::toggleSwapEyes()
{
    _swapEyes = !_swapEyes;
    emit stateChanged();
}

void Bino::setVideoTrack(int i)
{
    LOG_DEBUG("changing video track to %d", i);
    if (playlistMode()) {
        _player->setActiveVideoTrack(i);
        emit stateChanged();
    }
}

void Bino::setAudioTrack(int i)
{
    LOG_DEBUG("changing audio track to %d", i);
    if (playlistMode()) {
        _player->setActiveAudioTrack(i);
        emit stateChanged();
    }
}

void Bino::setSubtitleTrack(int i)
{
    LOG_DEBUG("changing subtitle track to %d", i);
    if (playlistMode()) {
        _player->setActiveSubtitleTrack(i);
        emit stateChanged();
    }
}

void Bino::setInputMode(InputMode mode)
{
    _videoSink->inputMode = mode;
    _frame.inputMode = mode;
    _frame.reUpdate();
    _frameIsNew = true;
    LOG_DEBUG("setting input mode to %s", inputModeToString(mode));
}

void Bino::setSurroundMode(SurroundMode mode)
{
    _videoSink->surroundMode = mode;
    _frame.surroundMode = mode;
    _frame.reUpdate();
    _frameIsNew = true;
    LOG_DEBUG("setting surround mode to %s", surroundModeToString(mode));
}

bool Bino::swapEyes() const
{
    return _swapEyes;
}

bool Bino::muted() const
{
    return _audioOutput->isMuted();
}

bool Bino::paused() const
{
    return (playlistMode() && _player->playbackState() == QMediaPlayer::PausedState);
}

bool Bino::playing() const
{
    return (playlistMode() && _player->playbackState() == QMediaPlayer::PlayingState);
}

bool Bino::stopped() const
{
    return (playlistMode() && _player->playbackState() == QMediaPlayer::StoppedState);
}

QUrl Bino::url() const
{
    QUrl url;
    if (playing() || paused())
        url = _player->source();
    return url;
}

int Bino::videoTrack() const
{
    int t = -1;
    if (captureMode())
        t = 0;
    else if (playlistMode())
        t = _player->activeVideoTrack();
    return t;
}

int Bino::audioTrack() const
{
    int t = -1;
    if (captureMode())
        t = 0;
    else if (playlistMode())
        t = _player->activeAudioTrack();
    return t;
}

int Bino::subtitleTrack() const
{
    int t = -1;
    if (captureMode())
        t = 0;
    else if (playlistMode())
        t = _player->activeSubtitleTrack();
    return t;
}

InputMode Bino::inputMode() const
{
    return _videoSink->inputMode;
}

InputMode Bino::assumeInputMode() const
{
    return _frame.inputMode;
}

bool Bino::assumeStereoInputMode() const
{
    return (assumeInputMode() != Input_Mono);
}

SurroundMode Bino::surroundMode() const
{
    return _videoSink->surroundMode;
}

SurroundMode Bino::assumeSurroundMode() const
{
    return _frame.surroundMode;
}

void Bino::serializeStaticData(QDataStream& ds) const
{
    ds << _screenType << _screen;
}

void Bino::deserializeStaticData(QDataStream& ds)
{
    ds >> _screenType >> _screen;
}

void Bino::serializeDynamicData(QDataStream& ds)
{
    ds << _frameWasSerialized;
    if (!_frameWasSerialized) {
        ds << _frame;
        if (_frame.inputMode == Input_Alternating_LR
                || _frame.inputMode == Input_Alternating_RL) {
            ds << _extFrame;
        }
        _frameWasSerialized = true;
    }
    ds << _swapEyes;
}

void Bino::deserializeDynamicData(QDataStream& ds)
{
    bool noNewFrame;
    ds >> noNewFrame;
    if (!noNewFrame) {
        ds >> _frame;
        if (_frame.inputMode == Input_Alternating_LR
                || _frame.inputMode == Input_Alternating_RL) {
            ds >> _extFrame;
        }
        _frameIsNew = true;
    }
    ds >> _swapEyes;
}

bool Bino::wantExit() const
{
    return _wantExit;
}

bool Bino::initProcess()
{
    bool haveAnisotropicFiltering = checkTextureAnisotropicFilterAvailability();
    LOG_DEBUG("Using OpenGL in the %s variant", IsOpenGLES ? "ES" : "Desktop");

    // Qt-based OpenGL initialization
    initializeOpenGLFunctions();

    // FBO and PBO
    glGenFramebuffers(1, &_viewFbo);
    glGenFramebuffers(1, &_frameFbo);
    glGenTextures(1, &_depthTex);
    glBindTexture(GL_TEXTURE_2D, _depthTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, 1, 1,
            0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
    glBindFramebuffer(GL_FRAMEBUFFER, _viewFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, _depthTex, 0);
    CHECK_GL();

    // Quad geometry
    const float quadPositions[] = {
        -1.0f, +1.0f, 0.0f,
        +1.0f, +1.0f, 0.0f,
        +1.0f, -1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f
    };
    const float quadTexCoords[] = {
        0.0f, 1.0f,
        1.0f, 1.0f,
        1.0f, 0.0f,
        0.0f, 0.0f
    };
    static const unsigned short quadIndices[] = {
        0, 3, 1, 1, 3, 2
    };
    glGenVertexArrays(1, &_quadVao);
    glBindVertexArray(_quadVao);
    GLuint quadPositionBuf;
    glGenBuffers(1, &quadPositionBuf);
    glBindBuffer(GL_ARRAY_BUFFER, quadPositionBuf);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadPositions), quadPositions, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    GLuint quadTexCoordBuf;
    glGenBuffers(1, &quadTexCoordBuf);
    glBindBuffer(GL_ARRAY_BUFFER, quadTexCoordBuf);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadTexCoords), quadTexCoords, GL_STATIC_DRAW);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(1);
    GLuint quadIndexBuf;
    glGenBuffers(1, &quadIndexBuf);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadIndexBuf);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quadIndices), quadIndices, GL_STATIC_DRAW);
    CHECK_GL();

    // Cube geometry
    const float cubePositions[] = {
        -10.0f, -10.0f, +10.0f,
        +10.0f, -10.0f, +10.0f,
        -10.0f, +10.0f, +10.0f,
        +10.0f, +10.0f, +10.0f,
        +10.0f, -10.0f, -10.0f,
        -10.0f, -10.0f, -10.0f,
        +10.0f, +10.0f, -10.0f,
        -10.0f, +10.0f, -10.0f,
        -10.0f, -10.0f, -10.0f,
        -10.0f, -10.0f, +10.0f,
        -10.0f, +10.0f, -10.0f,
        -10.0f, +10.0f, +10.0f,
        +10.0f, -10.0f, +10.0f,
        +10.0f, -10.0f, -10.0f,
        +10.0f, +10.0f, +10.0f,
        +10.0f, +10.0f, -10.0f,
        -10.0f, +10.0f, -10.0f,
        -10.0f, +10.0f, +10.0f,
        +10.0f, +10.0f, -10.0f,
        +10.0f, +10.0f, +10.0f,
        +10.0f, -10.0f, -10.0f,
        +10.0f, -10.0f, +10.0f,
        -10.0f, -10.0f, -10.0f,
        -10.0f, -10.0f, +10.0f
    };
    const float cubeTexCoords[] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f
    };
    static const unsigned short cubeIndices[] = {
        0, 1, 2, 1, 3, 2,
        4, 5, 6, 5, 7, 6,
        8, 9, 10, 9, 11, 10,
        12, 13, 14, 13, 15, 14,
        16, 17, 18, 17, 19, 18,
        20, 21, 22, 21, 23, 22
    };
    glGenVertexArrays(1, &_cubeVao);
    glBindVertexArray(_cubeVao);
    GLuint cubePositionBuf;
    glGenBuffers(1, &cubePositionBuf);
    glBindBuffer(GL_ARRAY_BUFFER, cubePositionBuf);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubePositions), cubePositions, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    GLuint cubeTexCoordBuf;
    glGenBuffers(1, &cubeTexCoordBuf);
    glBindBuffer(GL_ARRAY_BUFFER, cubeTexCoordBuf);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeTexCoords), cubeTexCoords, GL_STATIC_DRAW);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(1);
    GLuint cubeIndexBuf;
    glGenBuffers(1, &cubeIndexBuf);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeIndexBuf);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);
    CHECK_GL();

    // Plane textures
    glGenTextures(3, _planeTexs);
    for (int p = 0; p < 3; p++) {
        glBindTexture(GL_TEXTURE_2D, _planeTexs[p]);
        unsigned int black = 0;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, &black);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        if (p == 0) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        } else {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        }
    }
    CHECK_GL();

    // Frame textures
    glGenTextures(1, &_frameTex);
    glBindTexture(GL_TEXTURE_2D, _frameTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    if (haveAnisotropicFiltering)
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, 4.0f);
    glGenTextures(1, &_extFrameTex);
    glBindTexture(GL_TEXTURE_2D, _extFrameTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    if (haveAnisotropicFiltering)
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, 4.0f);
    CHECK_GL();

    // Subtitle texture
    glGenTextures(1, &_subtitleTex);
    glBindTexture(GL_TEXTURE_2D, _subtitleTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    if (haveAnisotropicFiltering)
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, 4.0f);
    CHECK_GL();

    // Screen geometry
    glGenVertexArrays(1, &_screenVao);
    glBindVertexArray(_screenVao);
    glGenBuffers(1, &_positionBuf);
    glBindBuffer(GL_ARRAY_BUFFER, _positionBuf);
    glBufferData(GL_ARRAY_BUFFER, _screen.positions.size() * sizeof(float),
            _screen.positions.constData(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    glGenBuffers(1, &_texcoordBuf);
    glBindBuffer(GL_ARRAY_BUFFER, _texcoordBuf);
    glBufferData(GL_ARRAY_BUFFER, _screen.texcoords.size() * sizeof(float),
            _screen.texcoords.constData(), GL_STATIC_DRAW);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(1);
    glGenBuffers(1, &_indexBuf);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _indexBuf);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
            _screen.indices.length() * sizeof(unsigned int),
            _screen.indices.constData(), GL_STATIC_DRAW);
    CHECK_GL();

    return true;
}

void Bino::rebuildColorPrgIfNecessary(int planeFormat, bool yuvValueRangeSmall, int yuvSpace)
{
    if (_colorPrg.isLinked()
            && _colorPrgPlaneFormat == planeFormat
            && _colorPrgYuvValueRangeSmall == yuvValueRangeSmall
            && _colorPrgYuvSpace == yuvSpace)
        return;

    LOG_DEBUG("rebuilding color conversion program for plane format %d, value range %s, yuv space %s",
            planeFormat, yuvValueRangeSmall ? "small" : "full", yuvSpace ? "true" : "false");
    QString colorVS = readFile(":src/shader-color.vert.glsl");
    QString colorFS = readFile(":src/shader-color.frag.glsl");
    colorFS.replace("$PLANE_FORMAT", QString::number(planeFormat));
    colorFS.replace("$VALUE_RANGE_SMALL", yuvValueRangeSmall ? "true" : "false");
    colorFS.replace("$YUV_SPACE", QString::number(yuvSpace));
    if (IsOpenGLES) {
        colorVS.prepend("#version 300 es\n");
        colorFS.prepend("#version 300 es\n"
                "precision mediump float;\n");
    } else {
        colorVS.prepend("#version 330\n");
        colorFS.prepend("#version 330\n");
    }
    _colorPrg.removeAllShaders();
    _colorPrg.addShaderFromSourceCode(QOpenGLShader::Vertex, colorVS);
    _colorPrg.addShaderFromSourceCode(QOpenGLShader::Fragment, colorFS);
    _colorPrg.link();
    _colorPrgPlaneFormat = planeFormat;
    _colorPrgYuvValueRangeSmall = yuvValueRangeSmall;
    _colorPrgYuvSpace = yuvSpace;
}

void Bino::rebuildViewPrgIfNecessary(SurroundMode surroundMode, bool nonLinearOutput)
{
    if (_viewPrg.isLinked()
            && _viewPrgSurroundMode == surroundMode
            && _viewPrgNonlinearOutput == nonLinearOutput)
        return;

    LOG_DEBUG("rebuilding view program for surround mode %s, non linear output %s",
            surroundModeToString(surroundMode), nonLinearOutput ? "true" : "false");
    QString viewVS = readFile(":src/shader-view.vert.glsl");
    QString viewFS = readFile(":src/shader-view.frag.glsl");
    viewFS.replace("$SURROUND_DEGREES",
              surroundMode == Surround_360 ? "360"
            : surroundMode == Surround_180 ? "180"
            : "0");
    viewFS.replace("$NONLINEAR_OUTPUT", nonLinearOutput ? "true" : "false");
    if (IsOpenGLES) {
        viewVS.prepend("#version 300 es\n");
        viewFS.prepend("#version 300 es\n"
                "precision mediump float;\n");
    } else {
        viewVS.prepend("#version 330\n");
        viewFS.prepend("#version 330\n");
    }
    _viewPrg.removeAllShaders();
    _viewPrg.addShaderFromSourceCode(QOpenGLShader::Vertex, viewVS);
    _viewPrg.addShaderFromSourceCode(QOpenGLShader::Fragment, viewFS);
    _viewPrg.link();
    _viewPrgSurroundMode = surroundMode;
    _viewPrgNonlinearOutput = nonLinearOutput;
}

bool Bino::drawSubtitleToImage(int w, int h, const QString& string)
{
    if (_subtitleImg.width() == w && _subtitleImg.height() == h && _subtitleImgString == string)
        return false;

    if (_subtitleImg.width() != w || _subtitleImg.height() != h)
        _subtitleImg = QImage(w, h, QImage::Format_ARGB32_Premultiplied);
    _subtitleImgString = string;

    QColor bgColor = Qt::black;
    bgColor.setAlpha(0);
    _subtitleImg.fill(bgColor);
    if (string.isEmpty())
        return true;

    // this tries to reproduce what qvideotexturehelper.cpp does since it is entirely
    // unclear and undocumented how subtitles are expected to be handled

    QFont font;
    float fontSize = h * 0.045f;
    font.setPointSize(fontSize);
    QTextLayout layout;
    layout.setText(string);
    layout.setFont(font);
    QTextOption option;
    option.setUseDesignMetrics(true);
    option.setAlignment(Qt::AlignCenter);
    layout.setTextOption(option);
    QFontMetrics metrics(font);
    float lineWidth = w * 0.9f;
    float margin = w * 0.05f;
    float height = 0.0f;
    float textWidth = 0.0f;
    layout.beginLayout();
    for (;;) {
        QTextLine line = layout.createLine();
        if (!line.isValid())
            break;
        line.setLineWidth(lineWidth);
        height += metrics.leading();
        line.setPosition(QPointF(margin, height));
        height += line.height();
        textWidth = qMax(textWidth, line.naturalTextWidth());
    }
    layout.endLayout();
    int bottomMargin = h / 20;
    float y = h - bottomMargin - height;
    layout.setPosition(QPointF(0.0f, y));
    textWidth += fontSize / 4.0f;
    //QRectF bounds = QRectF((w - textWidth) * 0.5f, y, textWidth, height);

    QPainter painter(&_subtitleImg);
    QTextLayout::FormatRange range;
    range.start = 0;
    range.length = layout.text().size();
    range.format.setForeground(Qt::white);
    layout.draw(&painter, {}, { range });

    return true;
}

static int alignmentFromBytesPerLine(const void* data, int bpl)
{
    int alignment = 1;
    if (uint64_t(data) % 8 == 0 && bpl % 8 == 0)
        alignment = 8;
    else if (uint64_t(data) % 4 == 0 && bpl % 4 == 0)
        alignment = 4;
    else if (uint64_t(data) % 2 == 0 && bpl % 2 == 0)
        alignment = 2;
    LOG_FIREHOSE("convertFrameToTexture: alignment is %d (from data %p, bpl %d)", alignment, data, bpl);
    return alignment;
}

void Bino::convertFrameToTexture(const VideoFrame& frame, unsigned int frameTex)
{
    // 1. Get the frame data into plane textures
    int w = frame.width;
    int h = frame.height;
    int planeFormat; // see shader-color.frag.glsl
    int planeCount;
    // reset swizzling for plane0; might be changed below depending in the format
    glBindTexture(GL_TEXTURE_2D, _planeTexs[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_BLUE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);
    if (frame.storage == VideoFrame::Storage_Image) {
        LOG_FIREHOSE("convertFrameToTexture: format is image");
        glPixelStorei(GL_UNPACK_ALIGNMENT, alignmentFromBytesPerLine(frame.image.constBits(), frame.image.bytesPerLine()));
        glPixelStorei(GL_UNPACK_ROW_LENGTH, frame.image.bytesPerLine() / 4);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, frame.image.constBits());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);
        planeFormat = 1;
        planeCount = 1;
    } else {
        std::array<const void*, 3> planeData;
        if (frame.storage == VideoFrame::Storage_Mapped) {
            planeData = { frame.mappedBits[0], frame.mappedBits[1], frame.mappedBits[2] };
        } else {
            planeData = { frame.bits[0].data(), frame.bits[1].data(), frame.bits[2].data() };
        }
        if (frame.pixelFormat == QVideoFrameFormat::Format_ARGB8888
                || frame.pixelFormat == QVideoFrameFormat::Format_ARGB8888_Premultiplied
                || frame.pixelFormat == QVideoFrameFormat::Format_XRGB8888) {
            LOG_FIREHOSE("convertFrameToTexture: format argb8888");
            glPixelStorei(GL_UNPACK_ALIGNMENT, alignmentFromBytesPerLine(planeData[0], frame.qframe.bytesPerLine(0)));
            glPixelStorei(GL_UNPACK_ROW_LENGTH, frame.qframe.bytesPerLine(0) / 4);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, planeData[0]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_ALPHA);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_GREEN);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_BLUE);
            planeFormat = 1;
            planeCount = 1;
        } else if (frame.pixelFormat == QVideoFrameFormat::Format_BGRA8888
                || frame.pixelFormat == QVideoFrameFormat::Format_BGRA8888_Premultiplied
                || frame.pixelFormat == QVideoFrameFormat::Format_BGRX8888) {
            LOG_FIREHOSE("convertFrameToTexture: format bgra8888");
            glPixelStorei(GL_UNPACK_ALIGNMENT, alignmentFromBytesPerLine(planeData[0], frame.qframe.bytesPerLine(0)));
            glPixelStorei(GL_UNPACK_ROW_LENGTH, frame.qframe.bytesPerLine(0) / 4);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, planeData[0]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);
            planeFormat = 1;
            planeCount = 1;
        } else if (frame.pixelFormat == QVideoFrameFormat::Format_ABGR8888
                || frame.pixelFormat == QVideoFrameFormat::Format_XBGR8888) {
            LOG_FIREHOSE("convertFrameToTexture: format abgr8888");
            glPixelStorei(GL_UNPACK_ALIGNMENT, alignmentFromBytesPerLine(planeData[0], frame.qframe.bytesPerLine(0)));
            glPixelStorei(GL_UNPACK_ROW_LENGTH, frame.qframe.bytesPerLine(0) / 4);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, planeData[0]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_ALPHA);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_BLUE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_GREEN);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_RED);
            planeFormat = 1;
            planeCount = 1;
        } else if (frame.pixelFormat == QVideoFrameFormat::Format_RGBA8888
                || frame.pixelFormat == QVideoFrameFormat::Format_RGBX8888) {
            LOG_FIREHOSE("convertFrameToTexture: format rgba8888");
            glPixelStorei(GL_UNPACK_ALIGNMENT, alignmentFromBytesPerLine(planeData[0], frame.qframe.bytesPerLine(0)));
            glPixelStorei(GL_UNPACK_ROW_LENGTH, frame.qframe.bytesPerLine(0) / 4);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, planeData[0]);
            planeFormat = 1;
            planeCount = 1;
        } else if (frame.pixelFormat == QVideoFrameFormat::Format_YUV420P) {
            LOG_FIREHOSE("convertFrameToTexture: format yuv420p");
            glPixelStorei(GL_UNPACK_ALIGNMENT, alignmentFromBytesPerLine(planeData[0], frame.qframe.bytesPerLine(0)));
            glPixelStorei(GL_UNPACK_ROW_LENGTH, frame.qframe.bytesPerLine(0));
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, planeData[0]);
            glBindTexture(GL_TEXTURE_2D, _planeTexs[1]);
            glPixelStorei(GL_UNPACK_ALIGNMENT, alignmentFromBytesPerLine(planeData[1], frame.qframe.bytesPerLine(1)));
            glPixelStorei(GL_UNPACK_ROW_LENGTH, frame.qframe.bytesPerLine(1));
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w / 2, h / 2, 0, GL_RED, GL_UNSIGNED_BYTE, planeData[1]);
            glBindTexture(GL_TEXTURE_2D, _planeTexs[2]);
            glPixelStorei(GL_UNPACK_ALIGNMENT, alignmentFromBytesPerLine(planeData[2], frame.qframe.bytesPerLine(2)));
            glPixelStorei(GL_UNPACK_ROW_LENGTH, frame.qframe.bytesPerLine(2));
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w / 2, h / 2, 0, GL_RED, GL_UNSIGNED_BYTE, planeData[2]);
            planeFormat = 2;
            planeCount = 3;
        } else if (frame.pixelFormat == QVideoFrameFormat::Format_YUV422P) {
            LOG_FIREHOSE("convertFrameToTexture: format yuv422p");
            glPixelStorei(GL_UNPACK_ALIGNMENT, alignmentFromBytesPerLine(planeData[0], frame.qframe.bytesPerLine(0)));
            glPixelStorei(GL_UNPACK_ROW_LENGTH, frame.qframe.bytesPerLine(0));
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, planeData[0]);
            glBindTexture(GL_TEXTURE_2D, _planeTexs[1]);
            glPixelStorei(GL_UNPACK_ALIGNMENT, alignmentFromBytesPerLine(planeData[1], frame.qframe.bytesPerLine(1)));
            glPixelStorei(GL_UNPACK_ROW_LENGTH, frame.qframe.bytesPerLine(1));
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w / 2, h, 0, GL_RED, GL_UNSIGNED_BYTE, planeData[1]);
            glBindTexture(GL_TEXTURE_2D, _planeTexs[2]);
            glPixelStorei(GL_UNPACK_ALIGNMENT, alignmentFromBytesPerLine(planeData[2], frame.qframe.bytesPerLine(2)));
            glPixelStorei(GL_UNPACK_ROW_LENGTH, frame.qframe.bytesPerLine(2));
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w / 2, h, 0, GL_RED, GL_UNSIGNED_BYTE, planeData[2]);
            planeFormat = 2;
            planeCount = 3;
        } else if (frame.pixelFormat == QVideoFrameFormat::Format_YV12) {
            LOG_FIREHOSE("convertFrameToTexture: format yv12");
            glPixelStorei(GL_UNPACK_ALIGNMENT, alignmentFromBytesPerLine(planeData[0], frame.qframe.bytesPerLine(0)));
            glPixelStorei(GL_UNPACK_ROW_LENGTH, frame.qframe.bytesPerLine(0));
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, planeData[0]);
            glBindTexture(GL_TEXTURE_2D, _planeTexs[1]);
            glPixelStorei(GL_UNPACK_ALIGNMENT, alignmentFromBytesPerLine(planeData[1], frame.qframe.bytesPerLine(1)));
            glPixelStorei(GL_UNPACK_ROW_LENGTH, frame.qframe.bytesPerLine(1));
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w / 2, h / 2, 0, GL_RED, GL_UNSIGNED_BYTE, planeData[1]);
            glBindTexture(GL_TEXTURE_2D, _planeTexs[2]);
            glPixelStorei(GL_UNPACK_ALIGNMENT, alignmentFromBytesPerLine(planeData[2], frame.qframe.bytesPerLine(2)));
            glPixelStorei(GL_UNPACK_ROW_LENGTH, frame.qframe.bytesPerLine(2));
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w / 2, h / 2, 0, GL_RED, GL_UNSIGNED_BYTE, planeData[2]);
            planeFormat = 3;
            planeCount = 3;
        } else if (frame.pixelFormat == QVideoFrameFormat::Format_NV12) {
            LOG_FIREHOSE("convertFrameToTexture: format nv12");
            glPixelStorei(GL_UNPACK_ALIGNMENT, alignmentFromBytesPerLine(planeData[0], frame.qframe.bytesPerLine(0)));
            glPixelStorei(GL_UNPACK_ROW_LENGTH, frame.qframe.bytesPerLine(0));
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, planeData[0]);
            glBindTexture(GL_TEXTURE_2D, _planeTexs[1]);
            glPixelStorei(GL_UNPACK_ALIGNMENT, alignmentFromBytesPerLine(planeData[1], frame.qframe.bytesPerLine(1)));
            glPixelStorei(GL_UNPACK_ROW_LENGTH, frame.qframe.bytesPerLine(1) / 2);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, w / 2, h / 2, 0, GL_RG, GL_UNSIGNED_BYTE, planeData[1]);
            planeFormat = 4;
            planeCount = 2;
        } else if (frame.pixelFormat == QVideoFrameFormat::Format_P010
                || frame.pixelFormat == QVideoFrameFormat::Format_P016) {
            LOG_FIREHOSE("convertFrameToTexture: format p010/p016");
            glPixelStorei(GL_UNPACK_ALIGNMENT, alignmentFromBytesPerLine(planeData[0], frame.qframe.bytesPerLine(0)));
            glPixelStorei(GL_UNPACK_ROW_LENGTH, frame.qframe.bytesPerLine(0) / 2);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R16, w, h, 0, GL_RED, GL_UNSIGNED_SHORT, planeData[0]);
            glBindTexture(GL_TEXTURE_2D, _planeTexs[1]);
            glPixelStorei(GL_UNPACK_ALIGNMENT, alignmentFromBytesPerLine(planeData[1], frame.qframe.bytesPerLine(1)));
            glPixelStorei(GL_UNPACK_ROW_LENGTH, frame.qframe.bytesPerLine(1) / 4);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16, w / 2, h / 2, 0, GL_RG, GL_UNSIGNED_SHORT, planeData[1]);
            planeFormat = 4;
            planeCount = 2;
        } else if (frame.pixelFormat == QVideoFrameFormat::Format_Y8) {
            glPixelStorei(GL_UNPACK_ALIGNMENT, alignmentFromBytesPerLine(planeData[0], frame.qframe.bytesPerLine(0)));
            glPixelStorei(GL_UNPACK_ROW_LENGTH, frame.qframe.bytesPerLine(0));
            LOG_FIREHOSE("convertFrameToTexture: format y8");
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, planeData[0]);
            planeFormat = 5;
            planeCount = 1;
        } else if (frame.pixelFormat == QVideoFrameFormat::Format_Y16) {
            glPixelStorei(GL_UNPACK_ALIGNMENT, alignmentFromBytesPerLine(planeData[0], frame.qframe.bytesPerLine(0)));
            glPixelStorei(GL_UNPACK_ROW_LENGTH, frame.qframe.bytesPerLine(0) / 2);
            LOG_FIREHOSE("convertFrameToTexture: format y16");
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R16, w, h, 0, GL_RED, GL_UNSIGNED_SHORT, planeData[0]);
            planeFormat = 5;
            planeCount = 1;
        } else {
            LOG_FATAL("Unhandled pixel format");
            std::exit(1);
        }
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    // 2. Convert plane textures into linear RGB in the frame texture
    glBindTexture(GL_TEXTURE_2D, frameTex);
    if (IsOpenGLES)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB10_A2, w, h, 0, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, nullptr);
    else
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16, w, h, 0, GL_BGRA, GL_UNSIGNED_SHORT, nullptr);
    glBindFramebuffer(GL_FRAMEBUFFER, _frameFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, frameTex, 0);
    glViewport(0, 0, w, h);
    glDisable(GL_DEPTH_TEST);
    rebuildColorPrgIfNecessary(planeFormat, frame.yuvValueRangeSmall, frame.yuvSpace);
    glUseProgram(_colorPrg.programId());
    for (int p = 0; p < planeCount; p++) {
        _colorPrg.setUniformValue(qPrintable(QString("plane") + QString::number(p)), p);
        glActiveTexture(GL_TEXTURE0 + p);
        glBindTexture(GL_TEXTURE_2D, _planeTexs[p]);
    }
    glBindVertexArray(_quadVao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
    glBindTexture(GL_TEXTURE_2D, frameTex);
    glGenerateMipmap(GL_TEXTURE_2D);
}

void Bino::preRenderProcess(int screenWidth, int screenHeight,
        int* viewCountPtr, int* viewWidthPtr, int* viewHeightPtr, float* frameDisplayAspectRatioPtr, bool* surroundPtr)
{
    Q_ASSERT(_frame.inputMode != Input_Unknown);

    int viewCount = 2;
    int viewWidth = _frame.width;
    int viewHeight = _frame.height;
    float frameDisplayAspectRatio = _frame.aspectRatio;
    switch (_frame.inputMode) {
    case Input_Unknown: // cannot happen, update() sets a known mode
    case Input_Mono:
        viewCount = 1;
        break;
    case Input_Top_Bottom:
    case Input_Bottom_Top:
        frameDisplayAspectRatio *= 2.0f;
        [[fallthrough]];
    case Input_Top_Bottom_Half:
    case Input_Bottom_Top_Half:
        viewHeight /= 2;
        break;
    case Input_Left_Right:
    case Input_Right_Left:
        frameDisplayAspectRatio /= 2.0f;
        [[fallthrough]];
    case Input_Left_Right_Half:
    case Input_Right_Left_Half:
        viewWidth /= 2;
        break;
    case Input_Alternating_LR:
    case Input_Alternating_RL:
        break;
    }
    switch (_frame.surroundMode) {
    case Surround_Unknown: // cannot happen, update() sets a known mode
    case Surround_Off:
        break;
    case Surround_180:
        frameDisplayAspectRatio *= 2.0f;
        break;
    case Surround_360:
        break;
    }
    if (subtitleTrack() >= 0 && (screenWidth > viewWidth || screenHeight > viewHeight)) {
        if (screenWidth / viewWidth > screenHeight / viewHeight) {
            viewWidth = screenWidth;
            viewHeight = viewWidth / frameDisplayAspectRatio;
        } else {
            viewHeight = screenHeight;
            viewWidth = viewHeight * frameDisplayAspectRatio;
        }
    }

    if (viewCountPtr)
        *viewCountPtr = viewCount;
    if (viewWidthPtr)
        *viewWidthPtr = viewWidth;
    if (viewHeightPtr)
        *viewHeightPtr = viewHeight;
    if (frameDisplayAspectRatioPtr)
        *frameDisplayAspectRatioPtr = frameDisplayAspectRatio;
    if (surroundPtr)
        *surroundPtr = (_frame.surroundMode != Surround_Off);

    /* We need to get new frame data into a texture that is suitable for
     * rendering the screen: _frameTex. */

    if (_frameIsNew) {
        // Convert _frame into _frameTex and, if needed, _extFrame into _extFrameTex.
        convertFrameToTexture(_frame, _frameTex);
        if (_frame.inputMode == Input_Alternating_LR
                || _frame.inputMode == Input_Alternating_RL) {
            // the user might have switched to this mode without the extFrame
            // being available, in that case fall back to the standard frame
            if (_extFrame.width != _frame.width || _extFrame.height != _frame.height)
                convertFrameToTexture(_frame, _extFrameTex);
            else
                convertFrameToTexture(_extFrame, _extFrameTex);
        }
        // Render the subtitle into the subtitle texture
        if (drawSubtitleToImage(viewWidth, viewHeight, _frame.subtitle)) {
            glBindTexture(GL_TEXTURE_2D, _subtitleTex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8,
                    _subtitleImg.width(), _subtitleImg.height(), 0, GL_RGBA,
                    GL_UNSIGNED_BYTE, _subtitleImg.bits());
            glGenerateMipmap(GL_TEXTURE_2D);
        }
        // Done.
        _frameIsNew = false;
    }
    if (_frame.inputMode != _lastFrameInputMode
            || _frame.surroundMode != _lastFrameSurroundMode) {
        emit stateChanged();
    }
    _lastFrameInputMode = _frame.inputMode;
    _lastFrameSurroundMode = _frame.surroundMode;
}

void Bino::render(
        const QVector3D& unitedScreenBottomLeft, const QVector3D& unitedScreenBottomRight, const QVector3D& unitedScreenTopLeft,
        const QVector3D& intersectedScreenBottomLeft, const QVector3D& intersectedScreenBottomRight, const QVector3D& intersectedScreenTopLeft,
        const QMatrix4x4& projectionMatrix,
        const QMatrix4x4& orientationMatrix,
        const QMatrix4x4& viewMatrix,
        int view, // 0 = left, 1 = right
        int texWidth, int texHeight, unsigned int texture)
{
    // Update screen
    switch (_screenType) {
    case ScreenUnited:
        _screen = Screen(unitedScreenBottomLeft, unitedScreenBottomRight, unitedScreenTopLeft);
        break;
    case ScreenIntersected:
        _screen = Screen(intersectedScreenBottomLeft, intersectedScreenBottomRight, intersectedScreenTopLeft);
        break;
    case ScreenGeometry:
        // do nothing
        break;
    }
    // Set up framebuffer object to render into
    glBindTexture(GL_TEXTURE_2D, _depthTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, texWidth, texHeight,
            0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
    glEnable(GL_DEPTH_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, _viewFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    // Set up view
    glViewport(0, 0, texWidth, texHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // Set up input mode
    unsigned int frameTex = _frameTex;
    float frameAspectRatio = _frame.aspectRatio;
    float viewOffsetX = 0.0f;
    float viewFactorX = 1.0f;
    float viewOffsetY = 0.0f;
    float viewFactorY = 1.0f;
    if (_swapEyes)
        view = (view == 0 ? 1 : 0);
    switch (_frame.inputMode) {
    case Input_Unknown: // cannot happen, update() sets a known mode
    case Input_Mono:
        // nothing to do
        break;
    case Input_Top_Bottom:
        viewFactorY = 0.5f;
        viewOffsetY = (view == 1 ? 0.5f : 0.0f);
        frameAspectRatio *= 2.0f;
        break;
    case Input_Top_Bottom_Half:
        viewFactorY = 0.5f;
        viewOffsetY = (view == 1 ? 0.5f : 0.0f);
        break;
    case Input_Bottom_Top:
        viewFactorY = 0.5f;
        viewOffsetY = (view != 1 ? 0.5f : 0.0f);
        frameAspectRatio *= 2.0f;
        break;
    case Input_Bottom_Top_Half:
        viewFactorY = 0.5f;
        viewOffsetY = (view != 1 ? 0.5f : 0.0f);
        break;
    case Input_Left_Right:
        viewFactorX = 0.5f;
        viewOffsetX = (view == 1 ? 0.5f : 0.0f);
        frameAspectRatio /= 2.0f;
        break;
    case Input_Left_Right_Half:
        viewFactorX = 0.5f;
        viewOffsetX = (view == 1 ? 0.5f : 0.0f);
        break;
    case Input_Right_Left:
        viewFactorX = 0.5f;
        viewOffsetX = (view != 1 ? 0.5f : 0.0f);
        frameAspectRatio /= 2.0f;
        break;
    case Input_Right_Left_Half:
        viewFactorX = 0.5f;
        viewOffsetX = (view != 1 ? 0.5f : 0.0f);
        break;
    case Input_Alternating_LR:
        if (view == 1)
            frameTex = _extFrameTex;
        break;
    case Input_Alternating_RL:
        if (view == 0)
            frameTex = _extFrameTex;
        break;
    }
    LOG_FIREHOSE("Rendering view %d from %s frame texture fx=%g ox=%g fy=%g oy=%g",
            view, frameTex == _frameTex ? "standard" : "extended", viewFactorX, viewOffsetX, viewFactorY, viewOffsetY);
    // Determine if we are producing the final rendering result here (which is the
    // case for VR mode) or if we are just rendering to intermediate textures (which
    // is the case for GUI mode). In GUI mode, the screen aspect ratio is unknown.
    bool finalRenderingStep = (_screen.aspectRatio > 0.0f);
    // Set up correct aspect ratio on screen
    float relWidth = 1.0f;
    float relHeight = 1.0f;
    if (finalRenderingStep) {
        if (_screen.aspectRatio < frameAspectRatio)
            relHeight = _screen.aspectRatio / frameAspectRatio;
        else
            relWidth = frameAspectRatio / _screen.aspectRatio;
    }
    // Set up shader program
    rebuildViewPrgIfNecessary(_frame.surroundMode, finalRenderingStep);
    glUseProgram(_viewPrg.programId());
    QMatrix4x4 projectionModelViewMatrix = projectionMatrix;
    if (_frame.surroundMode == Surround_Off)
        projectionModelViewMatrix = projectionModelViewMatrix * viewMatrix;
    _viewPrg.setUniformValue("projectionModelViewMatrix", projectionModelViewMatrix);
    _viewPrg.setUniformValue("orientationMatrix", orientationMatrix);
    _viewPrg.setUniformValue("frameTex", 0);
    _viewPrg.setUniformValue("subtitleTex", 1);
    _viewPrg.setUniformValue("view_offset_x", viewOffsetX);
    _viewPrg.setUniformValue("view_factor_x", viewFactorX);
    _viewPrg.setUniformValue("view_offset_y", viewOffsetY);
    _viewPrg.setUniformValue("view_factor_y", viewFactorY);
    _viewPrg.setUniformValue("relative_width", relWidth);
    _viewPrg.setUniformValue("relative_height", relHeight);
    // Render scene
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, _subtitleTex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, frameTex);
    if (_frame.surroundMode != Surround_Off) {
        // Set up filtering to work correctly at the horizontal wraparound:
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        if (_frame.surroundMode == Surround_360)
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        // Render
        glBindVertexArray(_cubeVao);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, 0);
        // Reset filtering parameters to their defaults
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glBindVertexArray(_screenVao);
        if (_screenType == ScreenUnited || _screenType == ScreenIntersected) {
            // the geometry might have changed; re-upload it
            glBindBuffer(GL_ARRAY_BUFFER, _positionBuf);
            glBufferData(GL_ARRAY_BUFFER, _screen.positions.size() * sizeof(float),
                    _screen.positions.constData(), GL_STATIC_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
            glBindBuffer(GL_ARRAY_BUFFER, _texcoordBuf);
            glBufferData(GL_ARRAY_BUFFER, _screen.texcoords.size() * sizeof(float),
                    _screen.texcoords.constData(), GL_STATIC_DRAW);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _indexBuf);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                    _screen.indices.length() * sizeof(unsigned int),
                    _screen.indices.constData(), GL_STATIC_DRAW);
        }
        glDrawElements(GL_TRIANGLES, _screen.indices.size(), GL_UNSIGNED_INT, 0);
    }
}

void Bino::keyPressEvent(QKeyEvent* event)
{
    if (event->matches(QKeySequence::Quit)
            || event->matches(QKeySequence::Cancel)
            || event->key() == Qt::Key_Escape
            || event->key() == Qt::Key_MediaStop) {
        quit();
    } else if (event->key() == Qt::Key_MediaTogglePlayPause
            || event->key() == Qt::Key_Space) {
        togglePause();
    } else if (event->key() == Qt::Key_MediaPause) {
        pause();
    } else if (event->key() == Qt::Key_MediaPlay) {
        play();
    } else if (event->key() == Qt::Key_VolumeMute || event->key() == Qt::Key_M) {
        toggleMute();
    } else if (event->key() == Qt::Key_VolumeDown) {
        changeVolume(-0.05f);
    } else if (event->key() == Qt::Key_VolumeUp) {
        changeVolume(+0.05f);
    } else if (event->key() == Qt::Key_Period) {
        seek(+1000);
    } else if (event->key() == Qt::Key_Comma) {
        seek(-1000);
    } else if (event->key() == Qt::Key_Right) {
        seek(+10000);
    } else if (event->key() == Qt::Key_Left) {
        seek(-10000);
    } else if (event->key() == Qt::Key_Right) {
        seek(+10000);
    } else if (event->key() == Qt::Key_Down) {
        seek(-60000);
    } else if (event->key() == Qt::Key_Up) {
        seek(+60000);
    } else if (event->key() == Qt::Key_PageDown) {
        seek(-600000);
    } else if (event->key() == Qt::Key_PageUp) {
        seek(+600000);
    } else if (event->key() == Qt::Key_N) {
        Playlist::instance()->next();
    } else if (event->key() == Qt::Key_P) {
        Playlist::instance()->prev();
    } else if (event->matches(QKeySequence::FullScreen) || event->key() == Qt::Key_F || event->key() == Qt::Key_F11) {
        emit toggleFullscreen();
    } else if (event->key() == Qt::Key_E || event->key() == Qt::Key_F7) {
        toggleSwapEyes();
    } else {
        LOG_DEBUG("Unhandled key event: key=%d text='%s'", event->key(), qPrintable(event->text()));
        event->ignore();
    }
}
