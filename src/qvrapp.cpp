/*
 * This file is part of Bino, a 3D video player.
 *
 * Copyright (C) 2016, 2017, 2018, 2019, 2020, 2021, 2022
 * Computer Graphics Group, University of Siegen
 * Written by Martin Lambers <martin.lambers@uni-siegen.de>
 * Copyright (C) 2022, 2023, 2024, 2025, 2026
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

#ifdef WITH_QVR

#include <limits>

#include <qvr/manager.hpp>
#include <qvr/device.hpp>
#include <qvr/observer.hpp>

#include "qvrapp.hpp"
#include "bino.hpp"
#include "tools.hpp"


BinoQVRApp::BinoQVRApp(bool renderDevices) :
    _renderDevices(renderDevices),
    _lastAnalogTriggerValue(0.0f),
    _haveButtonPressEvent(false),
    _haveButtonReleaseEvent(false)
{
}

unsigned int BinoQVRApp::setupTex(const QImage& img)
{
    unsigned int tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img.width(), img.height(), 0,
            GL_RGBA, GL_UNSIGNED_BYTE, img.constBits());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    if (_haveAnisotropicFiltering)
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, 4.0f);
    return tex;
}

unsigned int BinoQVRApp::setupVao(int vertexCount,
        const float* positions, const float* normals, const float* texcoords,
        int indexCount, const unsigned short* indices)
{
    GLuint vao;
    GLuint positionBuf, normalBuf, texcoordBuf, indexBuf;

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &positionBuf);
    glBindBuffer(GL_ARRAY_BUFFER, positionBuf);
    glBufferData(GL_ARRAY_BUFFER, vertexCount * 3 * sizeof(float), positions, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    glGenBuffers(1, &normalBuf);
    glBindBuffer(GL_ARRAY_BUFFER, normalBuf);
    glBufferData(GL_ARRAY_BUFFER, vertexCount * 3 * sizeof(float), normals, GL_STATIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(1);
    glGenBuffers(1, &texcoordBuf);
    glBindBuffer(GL_ARRAY_BUFFER, texcoordBuf);
    glBufferData(GL_ARRAY_BUFFER, vertexCount * 2 * sizeof(float), texcoords, GL_STATIC_DRAW);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(2);
    glGenBuffers(1, &indexBuf);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuf);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexCount * sizeof(unsigned short), indices, GL_STATIC_DRAW);
    return vao;
}

void BinoQVRApp::serializeStaticData(QDataStream& ds) const
{
    Bino::instance()->serializeStaticData(ds);
}

void BinoQVRApp::deserializeStaticData(QDataStream& ds)
{
    Bino::instance()->deserializeStaticData(ds);
}

void BinoQVRApp::serializeDynamicData(QDataStream& ds) const
{
    Bino::instance()->serializeDynamicData(ds);
}

void BinoQVRApp::deserializeDynamicData(QDataStream& ds)
{
    Bino::instance()->deserializeDynamicData(ds);
}

bool BinoQVRApp::wantExit()
{
    return Bino::instance()->wantExit();
}

bool BinoQVRApp::initProcess(QVRProcess*)
{
    initializeOpenGLFunctions();
    _haveAnisotropicFiltering = checkTextureAnisotropicFilterAvailability();
    // Shader program
    QString vrdeviceVS = readFile(":src/shader-vrdevice.vert.glsl");
    QString vrdeviceFS = readFile(":src/shader-vrdevice.frag.glsl");
    if (OpenGLType != OpenGL_Type_Desktop) {
        vrdeviceVS.prepend("#version 300 es\n");
        vrdeviceFS.prepend("#version 300 es\n"
                "precision mediump float;\n");
    } else {
        vrdeviceVS.prepend("#version 330\n");
        vrdeviceFS.prepend("#version 330\n");
    }
    _prg.addShaderFromSourceCode(QOpenGLShader::Vertex, vrdeviceVS);
    _prg.addShaderFromSourceCode(QOpenGLShader::Fragment, vrdeviceFS);
    _prg.link();
    // Get device model data
    for (int i = 0; i < QVRManager::deviceModelVertexDataCount(); i++) {
        _devModelVaos.append(setupVao(
                    QVRManager::deviceModelVertexCount(i),
                    QVRManager::deviceModelVertexPositions(i),
                    QVRManager::deviceModelVertexNormals(i),
                    QVRManager::deviceModelVertexTexCoords(i),
                    QVRManager::deviceModelVertexIndexCount(i),
                    QVRManager::deviceModelVertexIndices(i)));
        _devModelVaoIndices.append(QVRManager::deviceModelVertexIndexCount(i));
    }
    for (int i = 0; i < QVRManager::deviceModelTextureCount(); i++) {
        _devModelTextures.append(setupTex(QVRManager::deviceModelTexture(i)));
    }

    return Bino::instance()->initProcess();
}

static bool rayTriangleIntersect(
        const QVector3D& rayOrigin, const QVector3D& rayDirection,
        const QVector3D& A, const QVector3D& B, const QVector3D& C,
        const QVector2D& Atc, const QVector2D& Btc, const QVector2D& Ctc,
        float& hitAlpha, QVector2D& hitTexCoords)
{
    /* Möller-Trumbore ray/triangle intersection algorithm, adapted
     * from https://marlam.de/path-tracing/ part 4 */
    const float amin = 0.01f;
    const float amax = 100.0f;

    // get relevant vectors
    const QVector3D& d = rayDirection;
    QVector3D e1 = B - A;
    QVector3D e2 = C - A;
    QVector3D c2 = QVector3D::crossProduct(d, e2);

    // compute first determinant, for early exit test
    float Dpre = QVector3D::dotProduct(c2, e1);
    if (qAbs(Dpre) < std::numeric_limits<float>::epsilon()) {
        // ray and triangle are (nearly) parallel; no hit
        return false;
    }
    float invD = 1.0f / Dpre;

    // compute remaining relevant vectors
    QVector3D t = rayOrigin - A;
    QVector3D c1 = QVector3D::crossProduct(t, e1);

    // compute barycentric coordinates
    float D2 = QVector3D::dotProduct(c2, t);
    float u = D2 * invD;
    if (u < 0.0f || u > 1.0f) {
        // barycentric coordinate outside the triangle
        return false;
    }
    float D3 = QVector3D::dotProduct(c1, d);
    float v = D3 * invD;
    if (v < 0.0f || u + v > 1.0f) {
        // barycentric coordinate outside the triangle
        return false;
    }

    // at this point we know we have a hit, but is it valid?
    float D1 = QVector3D::dotProduct(c1, e2);
    float alpha = D1 * invD;
    if (alpha < amin || alpha > amax)
        return false;

    // a valid hit: use barycentric coordinates to interpolate vertex attributes
    hitAlpha = alpha;
    float w = 1.0f - u - v;
    hitTexCoords = w * Atc + u * Btc + v * Ctc;
    return true;
}

static QPointF toView(const QVector3D& position, const QVector3D& direction)
{
    Screen cubeSideScreen = Screen(
            QVector3D(-10.0f, -10.0f, -10.0f),
            QVector3D(+10.0f, -10.0f, -10.0f),
            QVector3D(-10.0f, +10.0f, -10.0f));
    const Screen& screen =
        Bino::instance()->assumeSurroundMode() == Surround_Off
        ? Bino::instance()->screen() : cubeSideScreen;
    bool haveHit = false;
    float nearestHitAlpha;
    QVector2D nearestHitTexCoords;
    for (long long int i = 0; i < screen.indices.size() / 3; i++) {
        float hitAlpha;
        QVector2D hitTexCoords;
        if (rayTriangleIntersect(position, direction,
                    QVector3D(screen.positions[3 * screen.indices[3 * i + 0] + 0],
                              screen.positions[3 * screen.indices[3 * i + 0] + 1],
                              screen.positions[3 * screen.indices[3 * i + 0] + 2]),
                    QVector3D(screen.positions[3 * screen.indices[3 * i + 1] + 0],
                              screen.positions[3 * screen.indices[3 * i + 1] + 1],
                              screen.positions[3 * screen.indices[3 * i + 1] + 2]),
                    QVector3D(screen.positions[3 * screen.indices[3 * i + 2] + 0],
                              screen.positions[3 * screen.indices[3 * i + 2] + 1],
                              screen.positions[3 * screen.indices[3 * i + 2] + 2]),
                    QVector2D(screen.texcoords[2 * screen.indices[3 * i + 0] + 0],
                              screen.texcoords[2 * screen.indices[3 * i + 0] + 1]),
                    QVector2D(screen.texcoords[2 * screen.indices[3 * i + 1] + 0],
                              screen.texcoords[2 * screen.indices[3 * i + 1] + 1]),
                    QVector2D(screen.texcoords[2 * screen.indices[3 * i + 2] + 0],
                              screen.texcoords[2 * screen.indices[3 * i + 2] + 1]),
                    hitAlpha, hitTexCoords)) {
            if (!haveHit || hitAlpha < nearestHitAlpha) {
                haveHit = true;
                nearestHitAlpha = hitAlpha;
                nearestHitTexCoords = hitTexCoords;
            }
        }
    }
    if (haveHit) {
        return QPointF(nearestHitTexCoords.x(), 1.0f - nearestHitTexCoords.y());
    } else {
        return QPointF(-1.0f, -1.0f);
    }
}

static QPointF toView(const QVRRenderContext& context, const QPointF& p)
{
    // We only know where the mouse points if the window has a single view
    if (context.viewCount() != 1) {
        return QPointF(-1.0f, -1.0f);
    }

    float ndcX = (2.0f * p.x()) / context.windowGeometry().width() - 1.0f;
    float ndcY = 1.0f - (2.0f * p.y()) / context.windowGeometry().height();
    QVector4D rayOriginNDC(ndcX, ndcY, -1.0f, 1.0f);
    QVector4D rayEndNDC(ndcX, ndcY, 1.0f, 1.0f);
    QMatrix4x4 invViewProj;
    if (Bino::instance()->assumeSurroundMode() == Surround_Off) {
        invViewProj = (context.frustum(0).toMatrix4x4() * context.viewMatrix(0)).inverted();
    } else {
        invViewProj = context.frustum(0).toMatrix4x4().inverted();
    }
    QVector3D rayOrigin = (invViewProj * rayOriginNDC).toVector3DAffine();
    QVector3D rayEnd = (invViewProj * rayEndNDC).toVector3DAffine();
    QVector3D rayDirection = (rayEnd - rayOrigin).normalized();
    return toView(rayOrigin, rayDirection);
}

static void deviceToRay(const QVRDevice& device, const QVRObserver& observer,
        QVector3D& rayOrigin, QVector3D& rayDirection)
{
    QVector3D org = device.position();
    QVector3D dir = device.orientation() * QVector3D(0.0f, 0.0f, -1.0f);
    if (Bino::instance()->assumeSurroundMode() == Surround_Off) {
        org = observer.navigationMatrix().map(org);
        dir = observer.navigationMatrix().mapVector(dir);
    } else {
        dir = observer.trackingMatrix().inverted().mapVector(dir);
    }
    rayOrigin = org;
    rayDirection = dir;
}

void BinoQVRApp::update(const QList<QVRObserver*>& observers)
{
    for (int i = 0; i < QVRManager::deviceCount(); i++) {
        const QVRDevice &device = QVRManager::device(i);
        if (device.hasButton(QVR_Button_Trigger)
                || device.hasAnalog(QVR_Analog_Trigger)
                || device.hasAnalog(QVR_Analog_Left_Trigger)
                || device.hasAnalog(QVR_Analog_Right_Trigger)) {
            QVector3D rayOrigin, rayDirection;
            if (_haveButtonPressEvent) {
                deviceToRay(device, *observers[0], rayOrigin, rayDirection);
                Bino::instance()->overlayUIPointerPress(toView(rayOrigin, rayDirection), true);
                _haveButtonPressEvent = false;
            } else if (_haveButtonReleaseEvent) {
                deviceToRay(device, *observers[0], rayOrigin, rayDirection);
                Bino::instance()->overlayUIPointerRelease(toView(rayOrigin, rayDirection));
                _haveButtonReleaseEvent = false;
            } else if (device.isButtonPressed(QVR_Button_Trigger)
                    || device.analogValue(QVR_Analog_Trigger) >= 0.5f
                    || device.analogValue(QVR_Analog_Left_Trigger) >= 0.5f
                    || device.analogValue(QVR_Analog_Right_Trigger) >= 0.5f) {
                deviceToRay(device, *observers[0], rayOrigin, rayDirection);
                Bino::instance()->overlayUIPointerMove(toView(rayOrigin, rayDirection), true);
            }
            break; // only use the first device to avoid conflicts
        }
    }
    Bino::instance()->updateMainProcess();
}

void BinoQVRApp::preRenderProcess(QVRProcess*)
{
    // We cannot know our screen geometry or even the screen aspect ratio,
    // but it's safe to assume that we want at least Full HD resolution
    // for the audio, subtitle, and UI overlays.
    Bino::instance()->preRenderProcess(1920, 1080);
}

void BinoQVRApp::render(QVRWindow*, const QVRRenderContext& context, const unsigned int* textures)
{
    for (int view = 0; view < context.viewCount(); view++) {
        // Render Bino view
        QMatrix4x4 projectionMatrix = context.frustum(view).toMatrix4x4();
        QMatrix4x4 orientationMatrix;
        orientationMatrix.rotate(context.navigationOrientation().inverted());
        orientationMatrix.rotate(context.trackingOrientation(view).inverted());
        QMatrix4x4 viewMatrix = context.viewMatrix(view);
        QMatrix4x4 viewMatrixPure = context.viewMatrixPure(view);
        int v = (context.eye(view) == QVR_Eye_Right ? 1 : 0);
        int texWidth = context.textureSize(view).width();
        int texHeight = context.textureSize(view).height();
        Bino::instance()->render(
                context.unitedScreenWallBottomLeft(), context.unitedScreenWallBottomRight(), context.unitedScreenWallTopLeft(),
                context.intersectedScreenWallBottomLeft(), context.intersectedScreenWallBottomRight(), context.intersectedScreenWallTopLeft(),
                projectionMatrix, orientationMatrix, viewMatrix, v, texWidth, texHeight, textures[view]);
        // Render VR device models (optional)
        if (_renderDevices) {
            glUseProgram(_prg.programId());
            for (int i = 0; i < QVRManager::deviceCount(); i++) {
                const QVRDevice& device = QVRManager::device(i);
                for (int j = 0; j < device.modelNodeCount(); j++) {
                    QMatrix4x4 nodeMatrix = device.matrix();
                    nodeMatrix.translate(device.modelNodePosition(j));
                    nodeMatrix.rotate(device.modelNodeOrientation(j));
                    QMatrix4x4 modelViewMatrix = viewMatrixPure * nodeMatrix;
                    int vertexDataIndex = device.modelNodeVertexDataIndex(j);
                    int textureIndex = device.modelNodeTextureIndex(j);
                    _prg.setUniformValue("modelViewMatrix", modelViewMatrix);
                    _prg.setUniformValue("projectionModelViewMatrix", projectionMatrix * modelViewMatrix);
                    _prg.setUniformValue("normalMatrix", modelViewMatrix.normalMatrix());
                    _prg.setUniformValue("hasDiffTex", _devModelTextures[textureIndex] == 0 ? 0 : 1);
                    _prg.setUniformValue("diffTex", 0);
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, _devModelTextures[textureIndex]);
                    glBindVertexArray(_devModelVaos[vertexDataIndex]);
                    glDrawElements(GL_TRIANGLES, _devModelVaoIndices[vertexDataIndex], GL_UNSIGNED_SHORT, 0);
                }
            }
        }
    }
    // Invalidate depth attachment (to help OpenGL ES performance)
    const GLenum fboInvalidations[] = { GL_DEPTH_ATTACHMENT };
    glInvalidateFramebuffer(GL_FRAMEBUFFER, 1, fboInvalidations);
}

void BinoQVRApp::keyPressEvent(const QVRRenderContext&, QKeyEvent* event)
{
    Bino::instance()->keyPressEvent(event);
}

void BinoQVRApp::mouseMoveEvent(const QVRRenderContext& context, QMouseEvent* event)
{
    Bino::instance()->overlayUIPointerMove(toView(context, event->position()));
}

void BinoQVRApp::mousePressEvent(const QVRRenderContext& context, QMouseEvent* event)
{
    Bino::instance()->overlayUIPointerPress(toView(context, event->position()));
}

void BinoQVRApp::mouseReleaseEvent(const QVRRenderContext& context, QMouseEvent* event)
{
    Bino::instance()->overlayUIPointerRelease(toView(context, event->position()));
}

void BinoQVRApp::deviceButtonPressEvent(QVRDeviceEvent* event)
{
    if (event->button() == QVR_Button_Trigger) {
        _haveButtonPressEvent = true;
    } else if (event->button() == QVR_Button_Menu) {
        Bino::instance()->quit();
    }
}

void BinoQVRApp::deviceButtonReleaseEvent(QVRDeviceEvent* event)
{
    if (event->button() == QVR_Button_Trigger) {
        _haveButtonReleaseEvent = true;
    }
}

void BinoQVRApp::deviceAnalogChangeEvent(QVRDeviceEvent* event)
{
    if (event->analog() == QVR_Analog_Trigger
            || event->analog() == QVR_Analog_Left_Trigger
            || event->analog() == QVR_Analog_Right_Trigger) {
        int i = event->analogIndex();
        float v = event->device().analogValue(i);
        if (v >= 0.5f && _lastAnalogTriggerValue < 0.5f) {
            _haveButtonPressEvent = true;
        } else if (v < 0.5f && _lastAnalogTriggerValue >= 0.5f) {
            _haveButtonReleaseEvent = true;
        }
        _lastAnalogTriggerValue = v;
    }
}

#endif
