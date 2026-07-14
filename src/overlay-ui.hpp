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

#pragma once

#include <QRectF>

#include "overlay.hpp"


class OverlayUI : public Overlay
{
private:
    qint64 _currentPosition;
    qint64 _currentDuration;
    bool _currentSeekable;
    bool _currentPaused;
    QPointF _currentPointer;
    qint64 _lastPosition;
    qint64 _lastDuration;
    bool _lastSeekable;
    bool _lastPaused;
    QPointF _lastPointer;
    bool _pointerPressed;

    float _buttonSize;
    float _penWidth;
    QRectF _boxes[10];
    bool _boxIsActive[10];

    void computeBoxes();
    QPointF pointerToImage(const QPointF& pointer);
    int boxIndex(const QPointF& pointer);
    float pointerToSeekPos(const QPointF& pointer);

public:
    OverlayUI();
    ~OverlayUI();

    void updateParameters(qint64 position, qint64 duration, bool seekable,
            bool paused, const QPointF& pointer);

    virtual bool redraw(int w, int h) override;

    bool pointerPress(const QPointF& pointer);
    void pointerRelease(const QPointF& pointer);
};
