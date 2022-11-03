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

#include <QFont>
#include <QFontMetrics>
#include <QTextLayout>
#include <QPainter>

#include "bino.hpp"
#include "log.hpp"
#include "tools.hpp"
#include "metadata.hpp"

/* These might not be defined in OpenGL ES environments.
 * Define them here to fix compilation. */
#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
# define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif


Bino::Bino(const Screen& screen, bool swapEyes) :
    _wantExit(false),
    _videoSink(nullptr),
    _audioOutput(nullptr),
    _player(nullptr),
    _audioInput(nullptr),
    _videoInput(nullptr),
    _captureSession(nullptr),
    _tempFile(nullptr),
    _recorder(nullptr),
    _lastFrameStereoLayout(VideoFrame::Layout_Unknown),
    _lastFrameThreeSixtyMode(VideoFrame::ThreeSixty_Unknown),
    _screen(screen),
    _frameIsNew(false),
    _swapEyes(swapEyes)
{
}

Bino::~Bino()
{
    delete _videoSink;
    delete _audioOutput;
    delete _player;
    delete _audioInput;
    delete _videoInput;
    delete _captureSession;
    delete _tempFile;
    delete _recorder;
}

void Bino::initializeOutput(const QAudioDevice& audioOutputDevice)
{
    _videoSink = new VideoSink(&_frame, &_extFrame, &_frameIsNew);
    connect(_videoSink, &VideoSink::newVideoFrame, [=]() { emit newVideoFrame(); });
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
            LOG_WARNING("media player error: %s", qPrintable(errorString));
            });
    _player->connect(_player, &QMediaPlayer::playbackStateChanged,
            [=](QMediaPlayer::PlaybackState state) {
            LOG_DEBUG("playback state changed to %s",
                    state == QMediaPlayer::StoppedState ? "stopped"
                    : state == QMediaPlayer::PlayingState ? "playing"
                    : state == QMediaPlayer::PausedState ? "paused"
                    : "unknown");
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

void Bino::startCaptureMode(
        bool withAudioInput,
        const QAudioDevice& audioInputDevice,
        const QCameraDevice& videoInputDevice)
{
    if (playlistMode())
        stopPlaylistMode();
    if (captureMode())
        stopCaptureMode();

    if (withAudioInput) {
        _audioInput = new QAudioInput;
        _audioInput->setDevice(audioInputDevice);
    }
    _videoInput = new QCamera;
    _videoInput->setCameraDevice(videoInputDevice);
    _captureSession = new QMediaCaptureSession;
    _captureSession->setAudioOutput(_audioOutput);
    if (_audioInput)
        _captureSession->setAudioInput(_audioInput);
    _captureSession->setCamera(_videoInput);
    _captureSession->setVideoSink(_videoSink);
    _recorder = new QMediaRecorder;
    /* Unfortunately we have to encode a media file even though we don't need to.
     * Use a temporary file with lowest possible quality settings. */
    _recorder->setQuality(QMediaRecorder::VeryLowQuality);
    _tempFile = new QTemporaryFile;
    _tempFile->open();
    _recorder->setOutputLocation(QUrl::fromLocalFile(_tempFile->fileName()));
    _captureSession->setRecorder(_recorder);
    _recorder->record();

    emit stateChanged();
}

void Bino::stopCaptureMode()
{
    if (_recorder) {
        delete _recorder;
        _recorder = nullptr;
        delete _tempFile;
        _tempFile = nullptr;
        delete _captureSession;
        _captureSession = nullptr;
        delete _videoInput;
        _videoInput = nullptr;
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
    return _recorder;
}

void Bino::mediaChanged(PlaylistEntry entry)
{
    if (!playlistMode())
        return;
    if (entry.noMedia()) {
        _player->stop();
    } else {
        _player->setSource(entry.url);
        MetaData metaData;
        metaData.detectCached(entry.url);
        if (entry.videoTrack >= 0) {
            _player->setActiveVideoTrack(entry.videoTrack);
        }
        if (entry.audioTrack >= 0) {
            _player->setActiveAudioTrack(entry.audioTrack);
        } else if (Playlist::instance()->preferredAudio != QLocale::AnyLanguage) {
            int audioTrack = -1;
            for (int i = 0; i < int(metaData.audioTracks.length()); i++) {
                QLocale audioLanguage = metaData.audioTracks[i].value(QMediaMetaData::Language).toLocale();
                if (audioLanguage == Playlist::instance()->preferredAudio) {
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
        } else if (metaData.subtitleTracks.size() > 0 && Playlist::instance()->wantSubtitle) {
            int subtitleTrack = 0;
            for (int i = 0; i < int(metaData.subtitleTracks.length()); i++) {
                QLocale subtitleLanguage = metaData.subtitleTracks[i].value(QMediaMetaData::Language).toLocale();
                if (subtitleLanguage == Playlist::instance()->preferredSubtitle) {
                    subtitleTrack = i;
                    break;
                }
            }
            _player->setActiveSubtitleTrack(subtitleTrack);
        }
        _player->play();
        _videoSink->newUrl(entry.url, entry.stereoLayout, entry.threeSixtyMode);
    }
    emit stateChanged();
}

void Bino::seek(qint64 milliseconds)
{
    if (!playlistMode())
        return;
    _player->setPosition(_player->position() + milliseconds);
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
    if (_player->playbackState() == QMediaPlayer::PlayingState) {
        _player->play();
        emit stateChanged();
    }
}

void Bino::toggleMute()
{
    _audioOutput->setMuted(!_player->audioOutput()->isMuted());
    emit stateChanged();
}

void Bino::changeVolume(int offset)
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

void Bino::setInputLayout(VideoFrame::StereoLayout layout)
{
    _videoSink->stereoLayout = layout;
    _frame.stereoLayout = layout;
    _frame.reUpdate();
    _frameIsNew = true;
    LOG_DEBUG("setting stereo layout to %s", VideoFrame::layoutToString(layout));
}

void Bino::setThreeSixtyMode(VideoFrame::ThreeSixtyMode mode)
{
    _videoSink->threeSixtyMode = mode;
    _frame.threeSixtyMode = mode;
    _frame.reUpdate();
    _frameIsNew = true;
    LOG_DEBUG("setting 360° mode to %s", VideoFrame::modeToString(mode));
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

VideoFrame::StereoLayout Bino::inputLayout() const
{
    return _videoSink->stereoLayout;
}

VideoFrame::StereoLayout Bino::assumeInputLayout() const
{
    return _frame.stereoLayout;
}

bool Bino::assumeStereoInputLayout() const
{
    return (assumeInputLayout() != VideoFrame::Layout_Mono);
}

VideoFrame::ThreeSixtyMode Bino::threeSixtyMode() const
{
    return _videoSink->threeSixtyMode;
}

bool Bino::assumeThreeSixtyMode() const
{
    return _frame.threeSixtyMode == VideoFrame::ThreeSixty_On;
}

void Bino::serializeStaticData(QDataStream& ds) const
{
    ds << _screen;
}

void Bino::deserializeStaticData(QDataStream& ds)
{
    ds >> _screen;
}

void Bino::serializeDynamicData(QDataStream& ds) const
{
    ds << _frameIsNew;
    if (_frameIsNew) {
        ds << _frame;
        if (_frame.stereoLayout == VideoFrame::Layout_Alternating_LR
                || _frame.stereoLayout == VideoFrame::Layout_Alternating_RL) {
            ds << _extFrame;
        }
    }
    ds << _swapEyes;
}

void Bino::deserializeDynamicData(QDataStream& ds)
{
    ds >> _frameIsNew;
    if (_frameIsNew) {
        ds >> _frame;
        if (_frame.stereoLayout == VideoFrame::Layout_Alternating_LR
                || _frame.stereoLayout == VideoFrame::Layout_Alternating_RL) {
            ds >> _extFrame;
        }
    }
    ds >> _swapEyes;
}

bool Bino::wantExit() const
{
    return _wantExit;
}

bool Bino::initProcess()
{
    bool isGLES = QOpenGLContext::currentContext()->isOpenGLES();
    LOG_DEBUG("Using OpenGL in the %s variant", isGLES ? "ES" : "Desktop");

    // Qt-based OpenGL initialization
    initializeOpenGLFunctions();

    // FBO and PBO
    glGenFramebuffers(1, &_viewFbo);
    glGenFramebuffers(1, &_frameFbo);
    if (_screen.isPlanar) {
        _depthTex = 0;
    } else {
        glGenTextures(1, &_depthTex);
        glBindTexture(GL_TEXTURE_2D, _depthTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, 1, 1,
                0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
        glBindFramebuffer(GL_FRAMEBUFFER, _viewFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, _depthTex, 0);
    }
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
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 4.0f);
    glGenTextures(1, &_extFrameTex);
    glBindTexture(GL_TEXTURE_2D, _extFrameTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 4.0f);
    CHECK_GL();

    // Subtitle texture
    glGenTextures(1, &_subtitleTex);
    glBindTexture(GL_TEXTURE_2D, _subtitleTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 4.0f);
    CHECK_GL();

    // Screen geometry
    glGenVertexArrays(1, &_screenVao);
    glBindVertexArray(_screenVao);
    GLuint positionBuf;
    glGenBuffers(1, &positionBuf);
    glBindBuffer(GL_ARRAY_BUFFER, positionBuf);
    glBufferData(GL_ARRAY_BUFFER, _screen.positions.size() * sizeof(float),
            _screen.positions.constData(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    GLuint texcoordBuf;
    glGenBuffers(1, &texcoordBuf);
    glBindBuffer(GL_ARRAY_BUFFER, texcoordBuf);
    glBufferData(GL_ARRAY_BUFFER, _screen.texcoords.size() * sizeof(float),
            _screen.texcoords.constData(), GL_STATIC_DRAW);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(1);
    GLuint indexBuf;
    glGenBuffers(1, &indexBuf);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuf);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
            _screen.indices.length() * sizeof(unsigned int),
            _screen.indices.constData(), GL_STATIC_DRAW);
    CHECK_GL();

    // Shader program
    QString colorVS = readFile(":shader-color.vert.glsl");
    QString colorFS = readFile(":shader-color.frag.glsl");
    QString viewVS = readFile(":shader-view.vert.glsl");
    QString viewFS = readFile(":shader-view.frag.glsl");
    if (isGLES) {
        colorVS.prepend("#version 320 es\n");
        colorFS.prepend("#version 320 es\n"
                "precision mediump float;\n");
        viewVS.prepend("#version 320 es\n");
        viewFS.prepend("#version 320 es\n"
                "precision mediump float;\n");
    } else {
        colorVS.prepend("#version 330\n");
        colorFS.prepend("#version 330\n");
        viewVS.prepend("#version 330\n");
        viewFS.prepend("#version 330\n");
    }
    _colorPrg.addShaderFromSourceCode(QOpenGLShader::Vertex, colorVS);
    _colorPrg.addShaderFromSourceCode(QOpenGLShader::Fragment, colorFS);
    _colorPrg.link();
    _viewPrg.addShaderFromSourceCode(QOpenGLShader::Vertex, viewVS);
    _viewPrg.addShaderFromSourceCode(QOpenGLShader::Fragment, viewFS);
    _viewPrg.link();
    CHECK_GL();

    return true;
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

void Bino::convertFrameToTexture(const VideoFrame& frame, unsigned int frameTex)
{
    bool isGLES = QOpenGLContext::currentContext()->isOpenGLES();

    // 1. Get the frame data into plane textures
    int w = frame.width;
    int h = frame.height;
    int planeFormat; // see shader-color.frag.glsl
    int planeCount;
    std::array<int, 4> plane0Swizzle = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };
    if (frame.storage == VideoFrame::Storage_Image) {
        glBindTexture(GL_TEXTURE_2D, _planeTexs[0]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, frame.image.constBits());
        glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, plane0Swizzle.data());
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
            glBindTexture(GL_TEXTURE_2D, _planeTexs[0]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, planeData[0]);
            plane0Swizzle = { GL_BLUE, GL_GREEN, GL_RED, GL_ALPHA };
            glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, plane0Swizzle.data());
            planeFormat = 1;
            planeCount = 1;
        } else if (frame.pixelFormat == QVideoFrameFormat::Format_BGRA8888
                || frame.pixelFormat == QVideoFrameFormat::Format_BGRA8888_Premultiplied
                || frame.pixelFormat == QVideoFrameFormat::Format_BGRX8888) {
            glBindTexture(GL_TEXTURE_2D, _planeTexs[0]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, planeData[0]);
            plane0Swizzle = { GL_ALPHA, GL_RED, GL_GREEN, GL_BLUE };
            glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, plane0Swizzle.data());
            planeFormat = 1;
            planeCount = 1;
        } else if (frame.pixelFormat == QVideoFrameFormat::Format_ABGR8888
                || frame.pixelFormat == QVideoFrameFormat::Format_XBGR8888) {
            glBindTexture(GL_TEXTURE_2D, _planeTexs[0]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, planeData[0]);
            plane0Swizzle = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };
            glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, plane0Swizzle.data());
            planeFormat = 1;
            planeCount = 1;
        } else if (frame.pixelFormat == QVideoFrameFormat::Format_RGBA8888
                || frame.pixelFormat == QVideoFrameFormat::Format_RGBX8888) {
            glBindTexture(GL_TEXTURE_2D, _planeTexs[0]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, planeData[0]);
            plane0Swizzle = { GL_ALPHA, GL_BLUE, GL_GREEN, GL_RED };
            glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, plane0Swizzle.data());
            planeFormat = 1;
            planeCount = 1;
        } else if (frame.pixelFormat == QVideoFrameFormat::Format_YUV420P) {
            glBindTexture(GL_TEXTURE_2D, _planeTexs[0]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, planeData[0]);
            glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, plane0Swizzle.data());
            glBindTexture(GL_TEXTURE_2D, _planeTexs[1]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w / 2, h / 2, 0, GL_RED, GL_UNSIGNED_BYTE, planeData[1]);
            glBindTexture(GL_TEXTURE_2D, _planeTexs[2]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w / 2, h / 2, 0, GL_RED, GL_UNSIGNED_BYTE, planeData[2]);
            planeFormat = 2;
            planeCount = 3;
        } else if (frame.pixelFormat == QVideoFrameFormat::Format_YUV422P) {
            glBindTexture(GL_TEXTURE_2D, _planeTexs[0]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, planeData[0]);
            glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, plane0Swizzle.data());
            glBindTexture(GL_TEXTURE_2D, _planeTexs[1]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w / 2, h, 0, GL_RED, GL_UNSIGNED_BYTE, planeData[1]);
            glBindTexture(GL_TEXTURE_2D, _planeTexs[2]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w / 2, h, 0, GL_RED, GL_UNSIGNED_BYTE, planeData[2]);
            planeFormat = 2;
            planeCount = 3;
        } else if (frame.pixelFormat == QVideoFrameFormat::Format_YV12) {
            glBindTexture(GL_TEXTURE_2D, _planeTexs[0]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, planeData[0]);
            glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, plane0Swizzle.data());
            glBindTexture(GL_TEXTURE_2D, _planeTexs[1]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w / 2, h / 2, 0, GL_RED, GL_UNSIGNED_BYTE, planeData[1]);
            glBindTexture(GL_TEXTURE_2D, _planeTexs[2]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w / 2, h / 2, 0, GL_RED, GL_UNSIGNED_BYTE, planeData[2]);
            planeFormat = 3;
            planeCount = 3;
        } else if (frame.pixelFormat == QVideoFrameFormat::Format_NV12) {
            glBindTexture(GL_TEXTURE_2D, _planeTexs[0]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, planeData[0]);
            glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, plane0Swizzle.data());
            glBindTexture(GL_TEXTURE_2D, _planeTexs[1]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, w / 2, h / 2, 0, GL_RG, GL_UNSIGNED_BYTE, planeData[1]);
            planeFormat = 4;
            planeCount = 2;
        } else if (frame.pixelFormat == QVideoFrameFormat::Format_P010
                || frame.pixelFormat == QVideoFrameFormat::Format_P016) {
            glBindTexture(GL_TEXTURE_2D, _planeTexs[0]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R16, w, h, 0, GL_RED, GL_UNSIGNED_SHORT, planeData[0]);
            glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, plane0Swizzle.data());
            glBindTexture(GL_TEXTURE_2D, _planeTexs[1]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16, w / 2, h / 2, 0, GL_RG, GL_UNSIGNED_SHORT, planeData[1]);
            planeFormat = 4;
            planeCount = 2;
        } else if (frame.pixelFormat == QVideoFrameFormat::Format_Y8) {
            glBindTexture(GL_TEXTURE_2D, _planeTexs[0]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, planeData[0]);
            glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, plane0Swizzle.data());
            planeFormat = 5;
            planeCount = 1;
        } else if (frame.pixelFormat == QVideoFrameFormat::Format_Y16) {
            glBindTexture(GL_TEXTURE_2D, _planeTexs[0]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R16, w, h, 0, GL_RED, GL_UNSIGNED_SHORT, planeData[0]);
            glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, plane0Swizzle.data());
            planeFormat = 5;
            planeCount = 1;
        } else {
            LOG_FATAL("unhandled pixel format");
            std::exit(1);
        }
    }
    // 2. Convert plane textures into linear RGB in the frame texture
    glBindTexture(GL_TEXTURE_2D, frameTex);
    if (isGLES)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB10_A2, w, h, 0, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, nullptr);
    else
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16, w, h, 0, GL_BGRA, GL_UNSIGNED_SHORT, nullptr);
    glBindFramebuffer(GL_FRAMEBUFFER, _frameFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, frameTex, 0);
    glViewport(0, 0, w, h);
    glDisable(GL_DEPTH_TEST);
    glUseProgram(_colorPrg.programId());
    _colorPrg.setUniformValue("planeFormat", planeFormat);
    _colorPrg.setUniformValue("yuvValueRangeSmall", frame.yuvValueRangeSmall ? 1 : 0);
    _colorPrg.setUniformValue("yuvSpace", int(frame.yuvSpace));
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
        int* viewCountPtr, int* viewWidthPtr, int* viewHeightPtr, float* frameDisplayAspectRatioPtr, bool* threeSixtyPtr)
{
    Q_ASSERT(_frame.stereoLayout != VideoFrame::Layout_Unknown);

    int viewCount = 2;
    int viewWidth = _frame.width;
    int viewHeight = _frame.height;
    float frameDisplayAspectRatio = _frame.aspectRatio;
    switch (_frame.stereoLayout) {
    case VideoFrame::Layout_Unknown: // cannot happen, VideoFrame::update() sets a known layout
    case VideoFrame::Layout_Mono:
        viewCount = 1;
        break;
    case VideoFrame::Layout_Top_Bottom:
    case VideoFrame::Layout_Bottom_Top:
        frameDisplayAspectRatio *= 2.0f;
        [[fallthrough]];
    case VideoFrame::Layout_Top_Bottom_Half:
    case VideoFrame::Layout_Bottom_Top_Half:
        viewHeight /= 2;
        break;
    case VideoFrame::Layout_Left_Right:
    case VideoFrame::Layout_Right_Left:
        frameDisplayAspectRatio /= 2.0f;
        [[fallthrough]];
    case VideoFrame::Layout_Left_Right_Half:
    case VideoFrame::Layout_Right_Left_Half:
        viewWidth /= 2;
        break;
    case VideoFrame::Layout_Alternating_LR:
    case VideoFrame::Layout_Alternating_RL:
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
    if (threeSixtyPtr)
        *threeSixtyPtr = (_frame.threeSixtyMode == VideoFrame::ThreeSixty_On);

    /* We need to get new frame data into a texture that is suitable for
     * rendering the screen: _frameTex. */

    if (_frameIsNew) {
        // Convert _frame into _frameTex and, if needed, _extFrame into _extFrameTex.
        convertFrameToTexture(_frame, _frameTex);
        if (_frame.stereoLayout == VideoFrame::Layout_Alternating_LR
                || _frame.stereoLayout == VideoFrame::Layout_Alternating_RL) {
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
    if (_frame.stereoLayout != _lastFrameStereoLayout
            || _frame.threeSixtyMode != _lastFrameThreeSixtyMode) {
        emit stateChanged();
    }
    _lastFrameStereoLayout = _frame.stereoLayout;
    _lastFrameThreeSixtyMode = _frame.threeSixtyMode;
}

void Bino::render(
        const QMatrix4x4& projectionMatrix,
        const QMatrix4x4& viewMatrix,
        int view, // 0 = left, 1 = right
        int texWidth, int texHeight, unsigned int texture)
{
    // Set up framebuffer object to render into
    if (!_screen.isPlanar) {
        glBindTexture(GL_TEXTURE_2D, _depthTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, texWidth, texHeight,
                0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
        glEnable(GL_DEPTH_TEST);
    } else {
        glDisable(GL_DEPTH_TEST);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, _viewFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    // Set up view
    glViewport(0, 0, texWidth, texHeight);
    glClear(_screen.isPlanar ? GL_COLOR_BUFFER_BIT : (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
    // Set up stereo layout
    unsigned int frameTex = _frameTex;
    float frameAspectRatio = _frame.aspectRatio;
    float viewOffsetX = 0.0f;
    float viewFactorX = 1.0f;
    float viewOffsetY = 0.0f;
    float viewFactorY = 1.0f;
    if (_swapEyes)
        view = (view == 0 ? 1 : 0);
    switch (_frame.stereoLayout) {
    case VideoFrame::Layout_Unknown: // cannot happen, VideoFrame::update() sets a known layout
    case VideoFrame::Layout_Mono:
        // nothing to do
        break;
    case VideoFrame::Layout_Top_Bottom:
        viewFactorY = 0.5f;
        viewOffsetY = (view == 1 ? 0.5f : 0.0f);
        frameAspectRatio *= 2.0f;
        break;
    case VideoFrame::Layout_Top_Bottom_Half:
        viewFactorY = 0.5f;
        viewOffsetY = (view == 1 ? 0.5f : 0.0f);
        break;
    case VideoFrame::Layout_Bottom_Top:
        viewFactorY = 0.5f;
        viewOffsetY = (view != 1 ? 0.5f : 0.0f);
        frameAspectRatio *= 2.0f;
        break;
    case VideoFrame::Layout_Bottom_Top_Half:
        viewFactorY = 0.5f;
        viewOffsetY = (view != 1 ? 0.5f : 0.0f);
        break;
    case VideoFrame::Layout_Left_Right:
        viewFactorX = 0.5f;
        viewOffsetX = (view == 1 ? 0.5f : 0.0f);
        frameAspectRatio /= 2.0f;
        break;
    case VideoFrame::Layout_Left_Right_Half:
        viewFactorX = 0.5f;
        viewOffsetX = (view == 1 ? 0.5f : 0.0f);
        break;
    case VideoFrame::Layout_Right_Left:
        viewFactorX = 0.5f;
        viewOffsetX = (view != 1 ? 0.5f : 0.0f);
        frameAspectRatio /= 2.0f;
        break;
    case VideoFrame::Layout_Right_Left_Half:
        viewFactorX = 0.5f;
        viewOffsetX = (view != 1 ? 0.5f : 0.0f);
        break;
    case VideoFrame::Layout_Alternating_LR:
        if (view == 1)
            frameTex = _extFrameTex;
        break;
    case VideoFrame::Layout_Alternating_RL:
        if (view == 0)
            frameTex = _extFrameTex;
        break;
    }
    LOG_FIREHOSE("rendering view %d from %s frame texture fx=%g ox=%g fy=%g oy=%g",
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
    glUseProgram(_viewPrg.programId());
    _viewPrg.setUniformValue("projection_matrix", projectionMatrix);
    _viewPrg.setUniformValue("model_view_matrix", viewMatrix);
    _viewPrg.setUniformValue("frameTex", 0);
    _viewPrg.setUniformValue("subtitleTex", 1);
    _viewPrg.setUniformValue("view_offset_x", viewOffsetX);
    _viewPrg.setUniformValue("view_factor_x", viewFactorX);
    _viewPrg.setUniformValue("view_offset_y", viewOffsetY);
    _viewPrg.setUniformValue("view_factor_y", viewFactorY);
    _viewPrg.setUniformValue("relative_width", relWidth);
    _viewPrg.setUniformValue("relative_height", relHeight);
    _viewPrg.setUniformValue("three_sixty", _frame.threeSixtyMode == VideoFrame::ThreeSixty_On ? 1 : 0);
    _viewPrg.setUniformValue("nonlinear_output", finalRenderingStep ? 1 : 0);
    // Render scene
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, _subtitleTex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, frameTex);
    if (_frame.threeSixtyMode == VideoFrame::ThreeSixty_On) {
        // Set up filtering to work correctly at the horizontal wraparound:
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        // Render
        glBindVertexArray(_cubeVao);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, 0);
        // Reset filtering parameters to their defaults
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glBindVertexArray(_screenVao);
        glDrawElements(GL_TRIANGLES, _screen.indices.size(), GL_UNSIGNED_INT, 0);
    }
    // Invalidate depth attachment (to help OpenGL ES performance)
    if (!_screen.isPlanar) {
        const GLenum fboInvalidations[] = { GL_DEPTH_ATTACHMENT };
        glInvalidateFramebuffer(GL_FRAMEBUFFER, 1, fboInvalidations);
    }
}

void Bino::keyPressEvent(QKeyEvent* event)
{
    if (event->matches(QKeySequence::Quit)
            || event->matches(QKeySequence::Cancel)
            || event->key() == Qt::Key_Escape
            || event->key() == Qt::Key_MediaStop) {
        stop();
        _wantExit = true;
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
    } else if (event->matches(QKeySequence::FullScreen) || event->key() == Qt::Key_F) {
        emit toggleFullscreen();
    } else if (event->key() == Qt::Key_E || event->key() == Qt::Key_F7) {
        toggleSwapEyes();
    } else {
        LOG_DEBUG("unhandled key event: key=%d text='%s'", event->key(), qPrintable(event->text()));
        event->ignore();
    }
}
