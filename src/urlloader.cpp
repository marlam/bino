/*
 * This file is part of Bino, a 3D video player.
 *
 * Copyright (C) 2024
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

#include <QNetworkRequest>
#include <QGuiApplication>

#include "urlloader.hpp"


UrlLoader::UrlLoader(const QUrl& url) : _url(url), _done(false)
{
}

UrlLoader::~UrlLoader()
{
}

void UrlLoader::urlLoaded(QNetworkReply* reply)
{
    _data = reply->readAll();
    reply->deleteLater();
    _done = true;
}

const QByteArray& UrlLoader::load()
{
    connect(&_netAccMgr, SIGNAL(finished(QNetworkReply*)), this, SLOT(urlLoaded(QNetworkReply*)));
    QNetworkRequest request(_url);
    _netAccMgr.get(request);
    while (!_done) {
        QGuiApplication::processEvents();
    }
    return _data;
}
