/*
 * This file is part of Bino, a 3D video player.
 *
 * Copyright (C) 2016, 2017, 2018, 2019, 2020, 2021, 2022
 * Computer Graphics Group, University of Siegen
 * Written by Martin Lambers <martin.lambers@uni-siegen.de>
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

#ifdef WITH_QVR

#include <qvr/manager.hpp>
#include <qvr/device.hpp>

#include "qvrapp.hpp"
#include "bino.hpp"
#include "tools.hpp"


BinoQVRApp::BinoQVRApp()
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
    if (IsOpenGLES) {
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

void BinoQVRApp::preRenderProcess(QVRProcess*)
{
    Bino::instance()->preRenderProcess();
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
    // Invalidate depth attachment (to help OpenGL ES performance)
    const GLenum fboInvalidations[] = { GL_DEPTH_ATTACHMENT };
    glInvalidateFramebuffer(GL_FRAMEBUFFER, 1, fboInvalidations);
}

void BinoQVRApp::keyPressEvent(const QVRRenderContext&, QKeyEvent* event)
{
    Bino::instance()->keyPressEvent(event);
}

#endif
