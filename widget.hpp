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

#include <QOpenGLWidget>
#include <QOpenGLExtraFunctions>

#include "bino.hpp"

class Widget : public QOpenGLWidget, protected QOpenGLExtraFunctions
{
Q_OBJECT

public:
    // This is mirrored in shader-display-frag.glsl:
    enum StereoMode {
        Mode_Left = 0,
        Mode_Right = 1,
        Mode_OpenGL_Stereo = 2,
        Mode_Alternating = 3,
        Mode_Red_Cyan_Dubois = 4,
        Mode_Red_Cyan_FullColor = 5,
        Mode_Red_Cyan_HalfColor = 6,
        Mode_Red_Cyan_Monochrome = 7,
        Mode_Green_Magenta_Dubois = 8,
        Mode_Green_Magenta_FullColor = 9,
        Mode_Green_Magenta_HalfColor = 10,
        Mode_Green_Magenta_Monochrome = 11,
        Mode_Amber_Blue_Dubois = 12,
        Mode_Amber_Blue_FullColor = 13,
        Mode_Amber_Blue_HalfColor = 14,
        Mode_Amber_Blue_Monochrome = 15,
        Mode_Red_Green_Monochrome = 16,
        Mode_Red_Blue_Monochrome = 17
    };

private:
    Bino* _bino;
    QSize _sizeHint;
    int _width, _height;

    enum StereoMode _stereoMode;
    bool _openGLStereo;       // is this widget in quad-buffered stereo mode?
    int _alternatingLastView; // last view displayed in Mode_Alternating (0 or 1)

    bool _inThreeSixtyMovement;
    QPointF _threeSixtyMovementStart;
    float _threeSixtyHorizontalAngleBase;
    float _threeSixtyVerticalAngleBase;
    float _threeSixtyHorizontalAngleCurrent;
    float _threeSixtyVerticalAngleCurrent;

    unsigned int _viewTex[2];
    int _viewTexWidth[2], _viewTexHeight[2];
    unsigned int _quadVao;
    QOpenGLShaderProgram _prg;

public:
    Widget(Bino* bino, StereoMode stereoMode, QWidget* parent = nullptr);

    bool isOpenGLStereo() const;
    enum StereoMode stereoMode() const;
    void setStereoMode(enum StereoMode mode);

    virtual QSize sizeHint() const override;
    virtual void initializeGL() override;
    virtual void paintGL() override;
    virtual void resizeGL(int w, int h) override;

    virtual void keyPressEvent(QKeyEvent* e) override;
    virtual void mousePressEvent(QMouseEvent* e) override;
    virtual void mouseReleaseEvent(QMouseEvent* e) override;
    virtual void mouseMoveEvent(QMouseEvent* e) override;

signals:
    void toggleFullscreen();
};
