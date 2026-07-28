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

#include <QIcon>

#include "overlay-ui.hpp"
#include "bino.hpp"
#include "playlist.hpp"


OverlayUI::OverlayUI() :
    _lastSurround(false),
    _lastStereo3D(false),
    _lastPosition(-1),
    _lastDuration(-1),
    _lastSeekable(false),
    _lastPlaying(false),
    _lastPointer(-1.0f, -1.0f),
    _lastShowPointer(false),
    _boxIsActive { false, false, false, false, false, false, false, false, false, false }
{
}

OverlayUI::~OverlayUI()
{
}

void OverlayUI::computeBoxes()
{
    float xOffset = 0.0f;
    float xFactor = 1.0f;
    float yOffset = 0.0f;
    if (_currentSurround) {
        xOffset = 0.25f * image().width();
        xFactor = 0.5f;
        yOffset = -0.3f * image().height();
    }
    _buttonSize = image().width() / 18.0f * xFactor;

    _penWidth = _buttonSize / 10.0f;
    bool isReallySeekable = (_currentSeekable && _currentDuration > 0 && _currentPosition >= 0);

    // 9 buttons
    float x = 0.5f * _buttonSize + xOffset;
    float y = image().height() - 2.5f * _buttonSize + yOffset;
    for (int i = 0; i < 9; i++) {
        _boxIsActive[i] = true;
        if (i == 0 || i == 8) {
            _boxIsActive[i] = Bino::instance()->playlistMode() && Playlist::instance()->length() > 1;
        } else if (i == 4) {
            if (_currentPosition <= 0 && _currentDuration <= 40) {
                // guess this is an image
                _boxIsActive[4] = false;
            }
        } else {
            _boxIsActive[i] = isReallySeekable;
        }
        _boxes[i] = QRectF(x, y, _buttonSize, _buttonSize);
        x += 2.0f * _buttonSize;
    }

    // seek bar
    float barX = 0.5f * _buttonSize + xOffset;
    float barY = image().height() - _buttonSize + yOffset;
    float barW = image().width() - 2.0f * barX;
    float barH = 0.5f * _buttonSize;
    _boxes[9] = QRectF(barX, barY, barW, barH);
    _boxIsActive[9] = isReallySeekable;

    // background
    _boxes[10] = QRectF(xOffset, image().height() - 3.0f * _buttonSize + yOffset, image().width() - 2.0f * xOffset, 3.0f * _buttonSize);
    _boxIsActive[10] = _currentSurround && _currentStereo3D;
}

QPointF OverlayUI::pointerToImage(const QPointF& pointer)
{
    return QPointF(pointer.x() * image().width(), pointer.y() * image().height());
}

int OverlayUI::boxIndex(const QPointF& pointer)
{
    QPointF p = pointerToImage(pointer);
    for (int i = 0; i < 10; i++) {
        if (_boxIsActive[i] && _boxes[i].contains(p))
            return i;
    }
    return -1;
}

float OverlayUI::pointerToSeekPos(const QPointF& pointer)
{
    // this assumes that the pointer is inside _boxes[9]
    QPointF p = pointerToImage(pointer);
    float seekPos = (p.x() - (_boxes[9].x() + 0.5f * _penWidth)) / (image().width() - 2.0f * (_boxes[9].x() + 0.5f * _penWidth));
    if (seekPos < 0.0f)
        seekPos = 0.0f;
    else if (seekPos > 1.0f)
        seekPos = 1.0f;
    return seekPos;
}

void OverlayUI::updateParameters(bool surround, bool stereo3D,
        qint64 position, qint64 duration, bool seekable,
        bool playing, const QPointF& pointer, bool showPointer)
{
    _currentSurround = surround;
    _currentStereo3D = stereo3D;
    _currentPosition = position;
    _currentDuration = duration;
    _currentSeekable = seekable;
    _currentPlaying = playing;
    _currentPointer = pointer;
    _currentShowPointer = showPointer;
}

bool OverlayUI::redraw(int w, int h)
{
    if (_currentSurround) {
        int d = qMin(w, h);
        w = h = d;
    }
    bool redraw = resize(w, h);
    if (_currentSurround != _lastSurround
            || _currentStereo3D != _lastStereo3D
            || _currentPosition != _lastPosition
            || _currentDuration != _lastDuration
            || _currentSeekable != _lastSeekable
            || _currentPlaying != _lastPlaying
            || _currentPointer != _lastPointer
            || _currentShowPointer != _lastShowPointer) {
        redraw = true;
    }

    if (!redraw)
        return false;

    clear();

    computeBoxes();
    int highlightedBox = boxIndex(_currentPointer);

    QColor normalColor = Qt::black;
    normalColor.setAlphaF(0.7f);
    QColor highlightColor = Qt::red;
    highlightColor.setAlphaF(0.7f);
    QPen normalPen(Qt::white);
    normalPen.setWidthF(_penWidth);
    QPen highlightPen(Qt::red);
    highlightPen.setWidthF(_penWidth);

    // background
    if (_boxIsActive[10]) {
        painter()->fillRect(_boxes[10], QColor(51, 51, 51));
    }

    // 9 buttons
    for (int i = 0; i < 9; i++) {
        if (_boxIsActive[i]) {
            QString iconName = "media-";
            float iconFactor = 1.0f;
            if (i == 0 || i == 8) {
                iconName += "skip-";
                iconName += i < 4 ? "backward" : "forward";
                iconFactor = 0.8f;
            } else if (i == 4) {
                iconName += "playback-";
                iconName += _currentPlaying ? "pause" : "start";
            } else {
                iconName += "seek-";
                iconName += i < 4 ? "backward" : "forward";
                iconFactor = 0.2f + qAbs(i - 4) * 0.2f;
            }
            painter()->fillRect(_boxes[i], i == highlightedBox ? highlightColor : normalColor);
            QIcon icon = QIcon::fromTheme(iconName);
            float d = 0.5f * (_buttonSize * iconFactor - _buttonSize);
            QRectF iconRect = _boxes[i].marginsAdded(QMarginsF(d, d, d, d));
            painter()->setPen(i == highlightedBox ? highlightPen : normalPen);
            icon.paint(painter(), iconRect.toRect(), Qt::AlignCenter, QIcon::Active, QIcon::On);
        }
    }

    // seek bar
    if (_boxIsActive[9]) {
        QRectF barRect = _boxes[9];
        painter()->fillRect(barRect, 9 == highlightedBox ? highlightColor : normalColor);
        painter()->setPen(normalPen);
        painter()->drawLine(QLineF(barRect.x() + _penWidth, barRect.y() + 0.25f * _buttonSize,
                    barRect.x() + barRect.width() - _penWidth, barRect.y() + 0.25f * _buttonSize));
        float posF = _currentPosition / float(_currentDuration);
        float barPosX = barRect.x() + 0.5f * _penWidth + posF * (barRect.width() - _penWidth);
        painter()->drawLine(QLineF(barPosX, barRect.y() + _penWidth,
                    barPosX, barRect.y() + barRect.height() - _penWidth));
        if (9 == highlightedBox) {
            painter()->setPen(highlightPen);
            posF = pointerToSeekPos(_currentPointer);
            float barPosX = barRect.x() + 0.5f * _penWidth + posF * (barRect.width() - _penWidth);
            painter()->drawLine(QLineF(barPosX, barRect.y() + _penWidth,
                        barPosX, barRect.y() + barRect.height() - _penWidth));
        }
    }

    // pointer
    if (_currentShowPointer) {
        painter()->setPen(highlightPen);
        painter()->drawEllipse(
                QPointF(_currentPointer.x() * w, _currentPointer.y() * h),
                _penWidth * 2.0f, _penWidth * 2.0f);
    }

    _lastSurround = _currentSurround;
    _lastStereo3D = _currentStereo3D;
    _lastPosition = _currentPosition;
    _lastDuration = _currentDuration;
    _lastSeekable = _currentSeekable;
    _lastPlaying = _currentPlaying;
    _lastPointer = _currentPointer;
    _lastShowPointer = _currentShowPointer;
    return true;
}

bool OverlayUI::pointerPress(const QPointF& pointer)
{
    return (boxIndex(pointer) >= 0);
}

void OverlayUI::pointerRelease(const QPointF& pointer)
{
    int i = boxIndex(pointer);
    if (i == 0)
        Playlist::instance()->prev();
    else if (i == 1)
        Bino::instance()->seek(-600000);
    else if (i == 2)
        Bino::instance()->seek(-60000);
    else if (i == 3)
        Bino::instance()->seek(-10000);
    else if (i == 4)
        Bino::instance()->togglePause();
    else if (i == 5)
        Bino::instance()->seek(+10000);
    else if (i == 6)
        Bino::instance()->seek(+60000);
    else if (i == 7)
        Bino::instance()->seek(+600000);
    else if (i == 8)
        Playlist::instance()->next();
    else if (i == 9)
        Bino::instance()->setPosition(pointerToSeekPos(pointer));
}

QDataStream &operator<<(QDataStream& ds, const OverlayUI& o)
{
    ds << o._currentSurround;
    ds << o._currentStereo3D;
    ds << o._currentPosition;
    ds << o._currentDuration;
    ds << o._currentSeekable;
    ds << o._currentPlaying;
    ds << o._currentPointer;
    ds << o._currentShowPointer;
    return ds;
}

QDataStream &operator>>(QDataStream& ds, OverlayUI& o)
{
    ds >> o._currentSurround;
    ds >> o._currentStereo3D;
    ds >> o._currentPosition;
    ds >> o._currentDuration;
    ds >> o._currentSeekable;
    ds >> o._currentPlaying;
    ds >> o._currentPointer;
    ds >> o._currentShowPointer;
    return ds;
}
