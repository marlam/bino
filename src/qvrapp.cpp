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

#ifdef WITH_QVR

#include "qvrapp.hpp"
#include "bino.hpp"


BinoQVRApp::BinoQVRApp()
{
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
    return Bino::instance()->initProcess();
}

void BinoQVRApp::preRenderProcess(QVRProcess*)
{
    Bino::instance()->preRenderProcess();
}

void BinoQVRApp::render(QVRWindow*, const QVRRenderContext& context, const unsigned int* textures)
{
    for (int view = 0; view < context.viewCount(); view++) {
        QMatrix4x4 projectionMatrix = context.frustum(view).toMatrix4x4();
        QMatrix4x4 viewMatrix = context.viewMatrix(view);
        int v = (context.eye(view) == QVR_Eye_Right ? 1 : 0);
        int texWidth = context.textureSize(view).width();
        int texHeight = context.textureSize(view).height();
        Bino::instance()->render(projectionMatrix, viewMatrix, v, texWidth, texHeight, textures[view]);
    }
}

void BinoQVRApp::keyPressEvent(const QVRRenderContext&, QKeyEvent* event)
{
    Bino::instance()->keyPressEvent(event);
}

#endif
