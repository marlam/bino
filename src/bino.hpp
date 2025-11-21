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

#pragma once

#include <QOpenGLExtraFunctions>
#include <QOpenGLShaderProgram>
#include <QAudioDevice>
#include <QCameraDevice>
#include <QAudioOutput>
#include <QAudioInput>
#include <QCamera>
#include <QMediaPlayer>
#include <QMediaCaptureSession>
#include <QKeyEvent>
#include <QScreenCapture>
#include <QWindowCapture>

#include "screen.hpp"
#include "videosink.hpp"
#include "playlist.hpp"


class Bino : public QObject, QOpenGLExtraFunctions
{
Q_OBJECT

public:
    enum ScreenType {
        ScreenUnited,           // global 2D united screen given by QVR
        ScreenIntersected,      // global 2D intersected screen given by QVR
        ScreenGeometry          // explicit geometry stored in _screen
    };

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
    QScreenCapture* _screenInput;
    QWindowCapture* _windowInput;
    QMediaCaptureSession* _captureSession;
    // for rendering subtitles:
    QImage _subtitleImg;
    QString _subtitleImgString;
    // for updating the GUI if necessary
    InputMode _lastFrameInputMode;
    SurroundMode _lastFrameSurroundMode;

    /* Static data for rendering, initialized on the main process */
    ScreenType _screenType;
    Screen _screen;

    /* Static data for rendering, initialized in initProcess() */
    unsigned int _depthTex;
    unsigned int _frameFbo;
    unsigned int _viewFbo;
    unsigned int _quadVao;
    unsigned int _cubeVao;
    unsigned int _planeTexs[3];
    unsigned int _frameTex;
    unsigned int _extFrameTex;
    unsigned int _subtitleTex;
    unsigned int _screenVao, _positionBuf, _texcoordBuf, _indexBuf;
    QOpenGLShaderProgram _colorPrg;
    int _colorPrgPlaneFormat;
    bool _colorPrgColorRangeSmall;
    int _colorPrgColorSpace;
    int _colorPrgColorTransfer;
    QOpenGLShaderProgram _viewPrg;
    SurroundMode _viewPrgSurroundMode;
    bool _viewPrgNonlinearOutput;

    /* Dynamic data for rendering */
    VideoFrame _frame;
    VideoFrame _extFrame; // for alternating stereo
    bool _frameIsNew;
    bool _frameWasSerialized;
    bool _swapEyes;

    void startCaptureMode(bool withAudioInput, const QAudioDevice& audioInputDevice, InputMode inputMode);
    void rebuildColorPrgIfNecessary(int planeFormat, bool colorRangeSmall, int colorSpace, int colorTransfer);
    void rebuildViewPrgIfNecessary(SurroundMode surroundMode, bool nonLinearOutput);
    bool drawSubtitleToImage(int w, int h, const QString& string);
    void convertFrameToTexture(const VideoFrame& frame, unsigned int frameTex);

public:
    Bino(ScreenType screenType, const Screen& screen, bool swapEyes);
    virtual ~Bino();

    static Bino* instance();

    /* Initialization functions, to be called by main() before
     * starting either GUI or VR mode */
    void initializeOutput(const QAudioDevice& audioOutputDevice);
    void startPlaylistMode();
    void stopPlaylistMode();
    void startCaptureModeCamera(
            bool withAudioInput,
            const QAudioDevice& audioInputDevice,
            const QCameraDevice& videoInputDevice,
            InputMode inputMode);
    void startCaptureModeScreen(
            bool withAudioInput,
            const QAudioDevice& audioInputDevice,
            QScreen* screenInputDevice,
            InputMode inputMode);
    void startCaptureModeWindow(
            bool withAudioInput,
            const QAudioDevice& audioInputDevice,
            const QCapturableWindow& windowInputDevice,
            InputMode inputMode);
    void stopCaptureMode();
    bool playlistMode() const;
    bool captureMode() const;

    /* Interaction functions, can be called while in GUI or VR mode */
    void quit();
    void seek(qint64 milliseconds);
    void setPosition(float pos);
    void togglePause();
    void pause();
    void play();
    void stop();
    void setMute(bool m);
    void toggleMute();
    void setVolume(float vol);
    void changeVolume(float offset);
    void setSwapEyes(bool s);
    void toggleSwapEyes();
    void setVideoTrack(int i);
    void setAudioTrack(int i);
    void setSubtitleTrack(int i);
    void setInputMode(InputMode mode);
    void setSurroundMode(SurroundMode mode);

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
    InputMode inputMode() const;                        // this might be unknown
    InputMode assumeInputMode() const;                  // this is never unknown
    bool assumeStereoInputMode() const;                 // is the assumed mode stereo?
    SurroundMode surroundMode() const;                  // this might be unknown
    SurroundMode assumeSurroundMode() const;            // this is never unknown

    /* Functions necessary for VR mode */
    void serializeStaticData(QDataStream& ds) const;
    void deserializeStaticData(QDataStream& ds);
    void serializeDynamicData(QDataStream& ds);
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
            bool* surround = nullptr);
    void render(
            const QVector3D& unitedScreenBottomLeft, const QVector3D& unitedScreenBottomRight, const QVector3D& unitedScreenTopLeft,
            const QVector3D& intersectedScreenBottomLeft, const QVector3D& intersectedScreenBottomRight, const QVector3D& intersectedScreenTopLeft,
            const QMatrix4x4& projectionMatrix,
            const QMatrix4x4& orientationMatrix,
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
    void wantQuit();
};
