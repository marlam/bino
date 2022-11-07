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

#ifdef WITH_QVR

#include <QOpenGLExtraFunctions>
#include <QOpenGLShaderProgram>

#include <qvr/app.hpp>


class BinoQVRApp : public QVRApp, protected QOpenGLExtraFunctions
{
public:
    BinoQVRApp();

    void serializeStaticData(QDataStream& ds) const override;
    void deserializeStaticData(QDataStream& ds) override;

    void serializeDynamicData(QDataStream& ds) const override;
    void deserializeDynamicData(QDataStream& ds) override;

    bool wantExit() override;

    bool initProcess(QVRProcess* p) override;

    void preRenderProcess(QVRProcess* p) override;

    void render(QVRWindow* w, const QVRRenderContext& c, const unsigned int* textures) override;

    void keyPressEvent(const QVRRenderContext& context, QKeyEvent* event) override;
};

#endif
