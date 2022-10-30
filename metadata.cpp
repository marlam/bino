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

#include <QMediaPlayer>
#include <QGuiApplication>
#include <QThread>
#include <QMap>

#include "metadata.hpp"
#include "log.hpp"


MetaData::MetaData()
{
}

static QMap<QUrl, MetaData> cache;

bool MetaData::detectCached(const QUrl& url, QString* errMsg)
{
    // try to find the url in the cache
    *this = cache[url];
    if (!this->url.isEmpty())
        return true;

    // detection via QMediaPlayer
    QMediaPlayer player;
    bool failure = false;
    bool available = false;
    QString errorMessage;
    player.connect(&player, &QMediaPlayer::errorOccurred,
            [&](QMediaPlayer::Error, const QString& errorString) {
            errorMessage = errorString;
            LOG_WARNING("%s: cannot get meta data: %s", qPrintable(player.source().toString()), qPrintable(errorString));
            failure = true;
            });
    player.connect(&player, &QMediaPlayer::metaDataChanged, [&]() { available = true; });
    player.setSource(url);
    while (!failure && !available) {
        QGuiApplication::processEvents();
    }
    if (failure) {
        if (errMsg)
            *errMsg = errorMessage;
        return false;
    }

    this->url = url;
    global = player.metaData();
    videoTracks = player.videoTracks();
    audioTracks = player.audioTracks();
    subtitleTracks = player.subtitleTracks();

    cache.insert(url, *this);
    return true;
}
