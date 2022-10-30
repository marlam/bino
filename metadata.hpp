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

#include <QUrl>
#include <QList>
#include <QMediaMetaData>


class MetaData
{
public:
    QUrl url;
    QMediaMetaData global;
    QList<QMediaMetaData> videoTracks;
    QList<QMediaMetaData> audioTracks;
    QList<QMediaMetaData> subtitleTracks;

    MetaData();
    bool detectCached(const QUrl& url, QString* errMsg = nullptr);
};
