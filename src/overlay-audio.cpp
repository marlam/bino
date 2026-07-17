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

#include "overlay-audio.hpp"


OverlayAudio::OverlayAudio()
{
}

OverlayAudio::~OverlayAudio()
{
}

void OverlayAudio::updateParameters(const MetaData& metaData)
{
    _currentString.clear();
    if (metaData.videoTracks.isEmpty()) {
        _currentString += metaData.global.stringValue(QMediaMetaData::Title) + QChar(8232);
        QStringList contributingArtistList = metaData.global.value(QMediaMetaData::ContributingArtist).toStringList();
        if (contributingArtistList.isEmpty()) {
            QStringList leadPerformerList = metaData.global.value(QMediaMetaData::LeadPerformer).toStringList();
            if (leadPerformerList.isEmpty()) {
                _currentString += metaData.global.stringValue(QMediaMetaData::Author);
            } else {
                _currentString += leadPerformerList.join(", ");
            }
        } else {
            _currentString += contributingArtistList.join(", ");
        }
        _currentString += QChar(8232);
        _currentString += metaData.global.stringValue(QMediaMetaData::AlbumTitle);
    }
}

bool OverlayAudio::redraw(int w, int h)
{
    if (_currentString.isEmpty())
        w = h = 1;
    bool redraw = resize(w, h);
    if (_currentString != _lastString)
        redraw = true;

    if (!redraw)
        return false;

    clear();

    if (_currentString.isEmpty())
        return true;

    QFont font;
    float fontSize = h * 0.05f;
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
    int bottomMargin = h / 2;
    float y = h - bottomMargin - height;
    layout.setPosition(QPointF(0.0f, y));
    textWidth += fontSize / 4.0f;

    QTextLayout::FormatRange range;
    range.start = 0;
    range.length = layout.text().size();
    range.format.setForeground(Qt::white);
    layout.draw(painter(), {}, { range });

    _lastString = _currentString;
    return true;
}
