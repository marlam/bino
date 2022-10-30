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

#include <QOpenGLExtraFunctions>
#include <QOpenGLShaderProgram>
#include <QAudioDevice>
#include <QCameraDevice>
#include <QAudioOutput>
#include <QAudioInput>
#include <QCamera>
#include <QTemporaryFile>
#include <QMediaPlayer>
#include <QMediaCaptureSession>
#include <QMediaRecorder>
#include <QKeyEvent>

#include "screen.hpp"
#include "videosink.hpp"
#include "playlist.hpp"


class Bino : public QObject, QOpenGLExtraFunctions
{
Q_OBJECT

private:
    /* Data not directly relevant for rendering */
    bool _wantExit;
    VideoSink* _videoSink;
    QAudioOutput* _audioOutput;
    // for playing a play list:
    QMediaPlayer* _player;
    // for capturing audio/video:
    QAudioInput* _audioInput;
    QCamera* _videoInput;
    QMediaCaptureSession* _captureSession;
    QTemporaryFile* _tempFile;
    QMediaRecorder* _recorder;
    // for rendering subtitles:
    QImage _subtitleImg;
    QString _subtitleImgString;

    /* Static data for rendering, initialized on the main process */
    Screen _screen;

    /* Static data for rendering, initialized in initProcess() */
    unsigned int _depthTex;
    unsigned int _frameFbo;
    unsigned int _viewFbo;
    unsigned int _quadVao;
    unsigned int _cubeVao;
    unsigned int _planeTexs[3];
    unsigned int _frameTex;
    unsigned int _subtitleTex;
    unsigned int _screenVao;
    QOpenGLShaderProgram _colorPrg;
    QOpenGLShaderProgram _viewPrg;

    /* Dynamic data for rendering */
    VideoFrame _frame;
    bool _frameIsNew;
    bool _swapEyes;

    bool drawSubtitleToImage(int w, int h, const QString& string);

public:
    Bino(const Screen& screen, bool swapEyes);
    virtual ~Bino();

    /* Initialization functions, to be called by main() before
     * starting either GUI or VR mode */
    void initializeOutput(const QAudioDevice& audioOutputDevice);
    void startPlaylistMode();
    void stopPlaylistMode();
    void startCaptureMode(
            bool withAudioInput,
            const QAudioDevice& audioInputDevice,
            const QCameraDevice& videoInputDevice);
    void stopCaptureMode();
    bool playlistMode() const;
    bool captureMode() const;

    /* Interaction functions, can be called while in GUI or VR mode */
    void seek(qint64 milliseconds);
    void togglePause();
    void pause();
    void play();
    void toggleMute();
    void changeVolume(int offset);
    void stop();
    void toggleSwapEyes();
    void setVideoTrack(int i);
    void setAudioTrack(int i);
    void setSubtitleTrack(int i);
    void setInputLayout(VideoFrame::StereoLayout layout);
    void setThreeSixtyMode(VideoFrame::ThreeSixtyMode mode);

    /* Functions necessary for GUI mode */
    bool swapEyes() const;
    bool muted() const;
    bool paused() const;
    bool playing() const;
    bool stopped() const;
    QUrl url() const;
    int videoTrack() const;
    int audioTrack() const;
    int subtitleTrack() const;
    VideoFrame::StereoLayout inputLayout() const;
    bool assumeStereoInputLayout() const;
    VideoFrame::ThreeSixtyMode threeSixtyMode() const;
    bool assumeThreeSixtyMode() const;

    /* Functions necessary for VR mode */
    void serializeStaticData(QDataStream& ds) const;
    void deserializeStaticData(QDataStream& ds);
    void serializeDynamicData(QDataStream& ds) const;
    void deserializeDynamicData(QDataStream& ds);
    bool wantExit() const;

    /* Functions shared by GUI and VR mode */
    bool initProcess();
    void preRenderProcess(
            int screenWidth = 0,
            int screenHeight = 0,
            int* viewCount = nullptr,
            int* viewWidth = nullptr,
            int* viewHeight = nullptr,
            float* frameDisplayAspectRatio = nullptr,
            bool* threeSixty = nullptr);
    void render(
            const QMatrix4x4& projectionMatrix,
            const QMatrix4x4& viewMatrix,
            int view, // 0 = left, 1 = right
            int texWidth, int texHeight, unsigned int texture);
    void keyPressEvent(QKeyEvent* event);

public slots:
    void mediaChanged(PlaylistEntry entry);

signals:
    void newVideoFrame();
    void toggleFullscreen();
    void stateChanged();
};
