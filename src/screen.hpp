/*
 * This file is part of Bino, a 3D video player.
 *
 * Copyright (C) 2017 Computer Graphics Group, University of Siegen
 * Written by Martin Lambers <martin.lambers@uni-siegen.de>
 *
 * Copyright (C) 2022
 * Martin Lambers <marlam@marlam.de>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* This VR screen functionality is adapted from qvr-videoplayer */

#pragma once

#include <QtCore>
#include <QVector>
#include <QVector3D>

class Screen
{
Q_DECLARE_TR_FUNCTIONS(Screen)

public:
    QVector<float> positions; // each position consists of 3 floats
    QVector<float> texcoords; // each texcoord consists of 2 floats
    QVector<unsigned int> indices;
    float aspectRatio;
    bool isPlanar;

    // Default constructor for a viewport-filling quad for GUI mode.
    // The aspectRatio will unknown (set to 0) since it depends on the viewport.
    Screen();

    // Construct a planar screen from three corners.
    // The aspect ratio is computed automatically.
    Screen(const QVector3D& bottomLeftCorner,
            const QVector3D& bottomRightCorner,
            const QVector3D& topLeftCorner);

    // Construct a screen by reading the specified OBJ file.
    // If the given shape name is not empty, only this shape will be considered.
    // Since the aspect ratio cannot be computed, it has to be specified.
    // The OBJ data must contain positions and texture coordinates;
    // everything else is ignored. If indices.size() == 0
    // after constructing the screen in this way, then loading
    // the OBJ file failed.
    Screen(const QString& objFileName, const QString& shapeName, float aspectRatio);
};

QDataStream &operator<<(QDataStream& ds, const Screen& screen);
QDataStream &operator>>(QDataStream& ds, Screen& screen);
