/*
 * This file is part of Bino, a 3D video player.
 *
 * Copyright (C) 2026
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

#include <QColor>

#include "overlay.hpp"


Overlay::Overlay() : _painter(nullptr)
{
}

Overlay::~Overlay()
{
    delete _painter;
}

bool Overlay::resize(int w, int h)
{
    if (_img.width() != w || _img.height() != h) {
        if (_painter)
            delete _painter;
        _img = QImage(w, h, QImage::Format_ARGB32_Premultiplied);
        _painter = new QPainter(&_img);
        _painter->setRenderHint(QPainter::Antialiasing, true);
        return true;
    } else {
        return false;
    }
}

void Overlay::clear()
{
    QColor clearColor = Qt::black;
    clearColor.setAlpha(0);
    _img.fill(clearColor);
}
