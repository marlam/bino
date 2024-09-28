/*
 * This file is part of Bino, a 3D video player.
 *
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

#include <QFile>
#include <QTextStream>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>

#include "tools.hpp"


bool IsOpenGLES;
void initializeIsOpenGLES(const QSurfaceFormat& format)
{
    IsOpenGLES = (QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGLES
            || format.renderableType() == QSurfaceFormat::OpenGLES);
}

QString readFile(const char* fileName)
{
    QFile f(fileName);
    f.open(QIODevice::ReadOnly);
    QTextStream in(&f);
    return in.readAll();
}

bool checkTextureAnisotropicFilterAvailability()
{
    return QOpenGLContext::currentContext()->hasExtension("GL_ARB_texture_filter_anisotropic")
        || QOpenGLContext::currentContext()->hasExtension("GL_EXT_texture_filter_anisotropic");
}

const char* getOpenGLString(QOpenGLExtraFunctions* gl, GLenum p)
{
    return reinterpret_cast<const char*>(gl->glGetString(p));
}

QString getExtension(const QString& fileName)
{
    QString extension;
    int lastDot = fileName.lastIndexOf('.');
    if (lastDot > 0)
        extension = (fileName.right(fileName.length() - lastDot - 1)).toLower();
    return extension;
}

QString getExtension(const QUrl& url)
{
    return getExtension(url.fileName());
}
