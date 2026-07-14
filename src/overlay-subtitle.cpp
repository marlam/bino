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

#include <QFont>
#include <QFontMetrics>
#include <QTextLayout>

#include "overlay-subtitle.hpp"


OverlaySubtitle::OverlaySubtitle()
{
}

OverlaySubtitle::~OverlaySubtitle()
{
}

void OverlaySubtitle::updateParameters(const QString& string)
{
    _currentString = string;
}

bool OverlaySubtitle::redraw(int w, int h)
{
    bool redraw = resize(w, h);
    if (_currentString != _lastString)
        redraw = true;

    if (!redraw)
        return false;

    clear();

    if (_currentString.isEmpty())
        return true;

    // this tries to reproduce what qvideotexturehelper.cpp does since it is entirely
    // unclear and undocumented how subtitles are expected to be handled

    QFont font;
    float fontSize = h * 0.045f;
    font.setPointSize(fontSize);
    QTextLayout layout;
    layout.setText(_currentString);
    layout.setFont(font);
    QTextOption option;
    option.setUseDesignMetrics(true);
    option.setAlignment(Qt::AlignCenter);
    layout.setTextOption(option);
    QFontMetrics metrics(font);
    float lineWidth = w * 0.9f;
    float margin = w * 0.05f;
    float height = 0.0f;
    float textWidth = 0.0f;
    layout.beginLayout();
    for (;;) {
        QTextLine line = layout.createLine();
        if (!line.isValid())
            break;
        line.setLineWidth(lineWidth);
        height += metrics.leading();
        line.setPosition(QPointF(margin, height));
        height += line.height();
        textWidth = qMax(textWidth, line.naturalTextWidth());
    }
    layout.endLayout();
    int bottomMargin = h / 20;
    float y = h - bottomMargin - height;
    layout.setPosition(QPointF(0.0f, y));
    textWidth += fontSize / 4.0f;
    //QRectF bounds = QRectF((w - textWidth) * 0.5f, y, textWidth, height);

    QTextLayout::FormatRange range;
    range.start = 0;
    range.length = layout.text().size();
    range.format.setForeground(Qt::white);
    layout.draw(painter(), {}, { range });

    _lastString = _currentString;
    return true;
}
