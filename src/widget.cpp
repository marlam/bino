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

#include <QGuiApplication>
#include <QMessageBox>
#include <QQuaternion>
#include <QtMath>

#include "widget.hpp"
#include "playlist.hpp"
#include "tools.hpp"
#include "log.hpp"

/* These might not be defined on Mac OS. Use crude replacements. */
#ifndef GL_MAX_FRAMEBUFFER_WIDTH
# define GL_MAX_FRAMEBUFFER_WIDTH GL_MAX_TEXTURE_SIZE
#endif
#ifndef GL_MAX_FRAMEBUFFER_HEIGHT
# define GL_MAX_FRAMEBUFFER_HEIGHT GL_MAX_TEXTURE_SIZE
#endif


static const QSize SizeBase(16, 9);

Widget::Widget(OutputMode outputMode, float surroundVerticalFOV, QWidget* parent) :
    QOpenGLWidget(parent),
    _sizeHint(0.5f * SizeBase),
    _outputMode(outputMode),
    _alternatingLastView(1),
    _inSurroundMovement(false),
    _surroundHorizontalAngleBase(0.0f),
    _surroundVerticalAngleBase(0.0f),
    _surroundHorizontalAngleCurrent(0.0f),
    _surroundVerticalAngleCurrent(0.0f)
{
    setSurroundVerticalFieldOfView(surroundVerticalFOV);
    _surroundVerticalFOVDefault = _surroundVerticalFOV; // to make sure clamping was applied
    setUpdateBehavior(QOpenGLWidget::PartialUpdate);
    setMouseTracking(true);
    setMinimumSize(8, 8);
    QSize screenSize = QGuiApplication::primaryScreen()->availableSize();
    QSize maxSize = 0.75f * screenSize;
    _sizeHint = SizeBase.scaled(maxSize, Qt::KeepAspectRatio);
    connect(Bino::instance(), &Bino::newVideoFrame, [=]() { update(); });
    connect(Bino::instance(), &Bino::toggleFullscreen, [=]() { emit toggleFullscreen(); });
    connect(Playlist::instance(), SIGNAL(mediaChanged(PlaylistEntry)), this, SLOT(mediaChanged(PlaylistEntry)));
    setFocus();
}

bool Widget::isOpenGLStereo() const
{
    return context()->format().stereo();
}

OutputMode Widget::outputMode() const
{
    return _outputMode;
}

void Widget::setOutputMode(enum OutputMode mode)
{
    _outputMode = mode;
}

void Widget::setSurroundVerticalFieldOfView(float vfov)
{
    _surroundVerticalFOV = qBound(5.0f, vfov, 115.0f);
}

void Widget::resetSurroundView()
{
    _surroundVerticalFOV = _surroundVerticalFOVDefault;
    _surroundHorizontalAngleBase = 0.0f;
    _surroundVerticalAngleBase = 0.0f;
    _surroundHorizontalAngleCurrent = 0.0f;
    _surroundVerticalAngleCurrent = 0.0f;
}

QSize Widget::sizeHint() const
{
    return _sizeHint;
}

void Widget::initializeGL()
{
    bool contextIsOk = (context()->isValid() && context()->format().majorVersion() >= 3);
    if (!contextIsOk) {
        LOG_FATAL("%s", qPrintable(tr("Insufficient OpenGL capabilities.")));
        QMessageBox::critical(this, tr("Error"), tr("Insufficient OpenGL capabilities."));
        std::exit(1);
    }
    if (outputMode() == Output_OpenGL_Stereo && !isOpenGLStereo()) {
        LOG_FATAL("%s", qPrintable(tr("OpenGL stereo mode is not available on this system.")));
        QMessageBox::critical(this, tr("Error"), tr("OpenGL stereo mode is not available on this system."));
        std::exit(1);
    }

    bool haveAnisotropicFiltering = checkTextureAnisotropicFilterAvailability();
    initializeOpenGLFunctions();
    bool isCoreProfile = (QOpenGLContext::currentContext()->format().profile() == QSurfaceFormat::CoreProfile);

    QString variantString = IsOpenGLES ? "OpenGL ES" : "OpenGL";
    if (!IsOpenGLES)
        variantString += isCoreProfile ? " core profile" : " compatibility profile";
    GLint maxTexSize, maxFBWidth, maxFBHeight;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);
    glGetIntegerv(GL_MAX_FRAMEBUFFER_WIDTH, &maxFBWidth);
    glGetIntegerv(GL_MAX_FRAMEBUFFER_HEIGHT, &maxFBHeight);
    LOG_INFO("OpenGL Variant:      %s", qPrintable(variantString));
    LOG_INFO("OpenGL Version:      %s", getOpenGLString(this, GL_VERSION));
    LOG_INFO("OpenGL GLSL Version: %s", getOpenGLString(this, GL_SHADING_LANGUAGE_VERSION));
    LOG_INFO("OpenGL Vendor:       %s", getOpenGLString(this, GL_VENDOR));
    LOG_INFO("OpenGL Renderer:     %s", getOpenGLString(this, GL_RENDERER));
    LOG_INFO("OpenGL AnisoTexFilt: %s", haveAnisotropicFiltering ? "yes" : "no");
    LOG_INFO("OpenGL Max Tex Size: %d", maxTexSize);
    LOG_INFO("OpenGL Max FB Size:  %dx%d", maxFBWidth, maxFBHeight);

    // View textures
    glGenTextures(2, _viewTex);
    for (int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, _viewTex[i]);
        unsigned char nullBytes[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
        if (IsOpenGLES)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB10_A2, 1, 1, 0, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, nullBytes);
        else
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16, 1, 1, 0, GL_RGBA, GL_UNSIGNED_SHORT, nullBytes);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        if (haveAnisotropicFiltering)
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, 4.0f);
        _viewTexWidth[i] = 1;
        _viewTexHeight[i] = 1;
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

    // Initialize Bino
    Bino::instance()->initProcess();
}

void Widget::rebuildDisplayPrgIfNecessary(OutputMode outputMode)
{
    if (outputMode == Output_Right)
        outputMode = Output_Left; // these are handled specially; see shader
    if (_displayPrg.isLinked() && _displayPrgOutputMode == outputMode)
        return;

    LOG_DEBUG("rebuilding display program for output mode %s", outputModeToString(outputMode));
    QString vertexShaderSource = readFile(":src/shader-display.vert.glsl");
    QString fragmentShaderSource = readFile(":src/shader-display.frag.glsl");
    fragmentShaderSource.replace("$OUTPUT_MODE", QString::number(int(outputMode)));
    if (IsOpenGLES) {
        vertexShaderSource.prepend("#version 300 es\n");
        fragmentShaderSource.prepend("#version 300 es\n"
                "precision mediump float;\n");
    } else {
        vertexShaderSource.prepend("#version 330\n");
        fragmentShaderSource.prepend("#version 330\n");
    }
    _displayPrg.removeAllShaders();
    _displayPrg.addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource);
    _displayPrg.addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource);
    _displayPrg.link();
    _displayPrgOutputMode = outputMode;
}

void Widget::paintGL()
{
    // Support for HighDPI output
    int width = _width * devicePixelRatioF();
    int height = _height * devicePixelRatioF();

    // Find out about the views we have
    int viewCount, viewWidth, viewHeight;
    float frameDisplayAspectRatio;
    bool surround;
    Bino::instance()->preRenderProcess(width, height, &viewCount, &viewWidth, &viewHeight, &frameDisplayAspectRatio, &surround);

    // Adjust the stereo mode if necessary
    bool frameIsStereo = (viewCount == 2);
    OutputMode outputMode = _outputMode;
    if (!frameIsStereo)
        outputMode = Output_Left;
    if (outputMode == Output_Left_Right || outputMode == Output_Right_Left)
        frameDisplayAspectRatio *= 2.0f;
    else if (outputMode == Output_Top_Bottom || outputMode == Output_Bottom_Top || outputMode == Output_HDMI_Frame_Pack)
        frameDisplayAspectRatio *= 0.5f;
    LOG_FIREHOSE("%s: %d views, %dx%d, %g, surround %s", Q_FUNC_INFO, viewCount, viewWidth, viewHeight, frameDisplayAspectRatio, surround ? "on" : "off");

    // Fill the view texture(s) as needed
    for (int v = 0; v <= 1; v++) {
        bool needThisView = true;
        switch (outputMode) {
        case Output_Left:
            needThisView = (v == 0);
            break;
        case Output_Right:
            needThisView = (v == 1);
            break;
        case Output_Alternating:
            needThisView = (v != _alternatingLastView);
            break;
        case Output_HDMI_Frame_Pack:
        case Output_OpenGL_Stereo:
        case Output_Left_Right:
        case Output_Left_Right_Half:
        case Output_Right_Left:
        case Output_Right_Left_Half:
        case Output_Top_Bottom:
        case Output_Top_Bottom_Half:
        case Output_Bottom_Top:
        case Output_Bottom_Top_Half:
        case Output_Even_Odd_Rows:
        case Output_Even_Odd_Columns:
        case Output_Checkerboard:
        case Output_Red_Cyan_Dubois:
        case Output_Red_Cyan_FullColor:
        case Output_Red_Cyan_HalfColor:
        case Output_Red_Cyan_Monochrome:
        case Output_Green_Magenta_Dubois:
        case Output_Green_Magenta_FullColor:
        case Output_Green_Magenta_HalfColor:
        case Output_Green_Magenta_Monochrome:
        case Output_Amber_Blue_Dubois:
        case Output_Amber_Blue_FullColor:
        case Output_Amber_Blue_HalfColor:
        case Output_Amber_Blue_Monochrome:
        case Output_Red_Green_Monochrome:
        case Output_Red_Blue_Monochrome:
            break;
        }
        if (!needThisView)
            continue;
        // prepare view texture
        glBindTexture(GL_TEXTURE_2D, _viewTex[v]);
        if (_viewTexWidth[v] != viewWidth || _viewTexHeight[v] != viewHeight) {
            if (IsOpenGLES)
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB10_A2, viewWidth, viewHeight, 0, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, nullptr);
            else
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16, viewWidth, viewHeight, 0, GL_RGBA, GL_UNSIGNED_SHORT, nullptr);
            _viewTexWidth[v] = viewWidth;
            _viewTexHeight[v] = viewHeight;
        }
        // render view into view texture
        LOG_FIREHOSE("%s: getting view %d for stereo mode %s", Q_FUNC_INFO, v, outputModeToString(outputMode));
        QMatrix4x4 projectionMatrix;
        QMatrix4x4 orientationMatrix;
        QMatrix4x4 viewMatrix;
        if (Bino::instance()->assumeSurroundMode() != Surround_Off) {
            float verticalFieldOfView = qDegreesToRadians(_surroundVerticalFOV);
            float aspectRatio = 2.0f; // always 2:1 for surround video!
            float top = qTan(verticalFieldOfView * 0.5f);
            float bottom = -top;
            float right = top * aspectRatio;
            float left = -right;
            projectionMatrix.frustum(left, right, bottom, top, 1.0f, 100.0f);
            QQuaternion orientation = QQuaternion::fromEulerAngles(
                    (_surroundVerticalAngleBase + _surroundVerticalAngleCurrent),
                    (_surroundHorizontalAngleBase + _surroundHorizontalAngleCurrent), 0.0f);
            orientationMatrix.rotate(orientation.inverted());
        }
        Bino::instance()->render(
                QVector3D(), QVector3D(), QVector3D(), QVector3D(), QVector3D(), QVector3D(),
                projectionMatrix, orientationMatrix, viewMatrix, v, viewWidth, viewHeight, _viewTex[v]);
        // generate mipmaps for the view texture
        glBindTexture(GL_TEXTURE_2D, _viewTex[v]);
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    // Put the views on screen in the current mode
    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);
    float relWidth = 1.0f;
    float relHeight = 1.0f;
    float screenAspectRatio = width / float(height);
    if (outputMode == Output_HDMI_Frame_Pack)
        screenAspectRatio = width / (height - height / 49.0f);
    if (screenAspectRatio < frameDisplayAspectRatio)
        relHeight = screenAspectRatio / frameDisplayAspectRatio;
    else
        relWidth = frameDisplayAspectRatio / screenAspectRatio;
    rebuildDisplayPrgIfNecessary((outputMode == Output_OpenGL_Stereo || outputMode == Output_Alternating)
            ? Output_Left /* also covers Output_Right */ : outputMode);
    glUseProgram(_displayPrg.programId());
    _displayPrg.setUniformValue("view0", 0);
    _displayPrg.setUniformValue("view1", 1);
    _displayPrg.setUniformValue("relativeWidth", relWidth);
    _displayPrg.setUniformValue("relativeHeight", relHeight);
    QPoint globalLowerLeft = mapToGlobal(QPoint(0, height - 1));
    _displayPrg.setUniformValue("fragOffsetX", float(globalLowerLeft.x()));
    _displayPrg.setUniformValue("fragOffsetY", float(screen()->geometry().height() - 1 - globalLowerLeft.y()));
    LOG_FIREHOSE("lower left widget corner in screen coordinates: x=%d y=%d", globalLowerLeft.x(), screen()->geometry().height() - 1 - globalLowerLeft.y());
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _viewTex[0]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, _viewTex[1]);
    glBindVertexArray(_quadVao);
    if (isOpenGLStereo()) {
        LOG_FIREHOSE("widget draw mode: opengl stereo");
        if (outputMode == Output_OpenGL_Stereo) {
            glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject(QOpenGLWidget::LeftBuffer));
            _displayPrg.setUniformValue("outputModeLeftRightView", 0);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
            glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject(QOpenGLWidget::RightBuffer));
            _displayPrg.setUniformValue("outputModeLeftRightView", 1);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
        } else {
            if (outputMode == Output_Alternating)
                outputMode = (_alternatingLastView == 0 ? Output_Right : Output_Left);
            _displayPrg.setUniformValue("outputModeLeftRightView", outputMode == Output_Left ? 0 : 1);
            glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject(QOpenGLWidget::LeftBuffer));
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
            glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject(QOpenGLWidget::RightBuffer));
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
        }
    } else {
        LOG_FIREHOSE("widget draw mode: normal");
        glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
        if (outputMode == Output_Alternating)
            outputMode = (_alternatingLastView == 0 ? Output_Right : Output_Left);
        _displayPrg.setUniformValue("outputModeLeftRightView", outputMode == Output_Left ? 0 : 1);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
    }

    // Update Output_Alternating
    if (_outputMode == Output_Alternating && frameIsStereo) {
        _alternatingLastView = (_alternatingLastView == 0 ? 1 : 0);
        update();
    }
}

void Widget::resizeGL(int w, int h)
{
    _width = w;
    _height = h;
}

void Widget::keyPressEvent(QKeyEvent* e)
{
    Bino::instance()->keyPressEvent(e);
}

void Widget::mousePressEvent(QMouseEvent* e)
{
    _inSurroundMovement = true;
    _surroundMovementStart = e->position();
    _surroundHorizontalAngleCurrent = 0.0f;
    _surroundVerticalAngleCurrent = 0.0f;
}

void Widget::mouseReleaseEvent(QMouseEvent*)
{
    _inSurroundMovement = false;
    _surroundHorizontalAngleBase += _surroundHorizontalAngleCurrent;
    _surroundVerticalAngleBase += _surroundVerticalAngleCurrent;
    _surroundHorizontalAngleCurrent = 0.0f;
    _surroundVerticalAngleCurrent = 0.0f;
}

void Widget::mouseMoveEvent(QMouseEvent* e)
{
    // Support for HighDPI output
    int width = _width * devicePixelRatioF();
    int height = _height * devicePixelRatioF();

    if (_inSurroundMovement) {
        // position delta
        QPointF posDelta = e->position() - _surroundMovementStart;
        // horizontal angle delta
        float dx = posDelta.x();
        float xf = dx / width; // in [-1,+1]
        _surroundHorizontalAngleCurrent = xf * 180.0f;
        // vertical angle
        float dy = posDelta.y();
        float yf = dy / height; // in [-1,+1]
        _surroundVerticalAngleCurrent = yf * 90.0f;
        update();
    }
}

void Widget::wheelEvent(QWheelEvent* e)
{
    setSurroundVerticalFieldOfView(_surroundVerticalFOV - e->angleDelta().y() / 120.0f);
    update();
}

void Widget::mediaChanged(PlaylistEntry)
{
    _inSurroundMovement = false;
    _surroundHorizontalAngleBase = 0.0f;
    _surroundVerticalAngleBase = 0.0f;
    _surroundHorizontalAngleCurrent = 0.0f;
    _surroundVerticalAngleCurrent = 0.0f;
}
