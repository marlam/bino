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

#pragma once

#include <QLocale>
#include <QUrl>

#include "modes.hpp"


class PlaylistEntry
{
public:
    static const int NoTrack = -2;
    static const int DefaultTrack = -1;

    QUrl url;
    InputMode inputMode;
    SurroundMode surroundMode;
    int videoTrack;
    int audioTrack;
    int subtitleTrack;

    PlaylistEntry();
    PlaylistEntry(const QUrl& url,
            InputMode inputMode = Input_Unknown,
            SurroundMode surroundMode = Surround_Unknown,
            int videoTrack = DefaultTrack,
            int audioTrack = DefaultTrack,
            int subtitleTrack = DefaultTrack);

    bool noMedia() const;
    QString optionsToString() const;
    bool optionsFromString(const QString& s);
};


class Playlist : public QObject
{
Q_OBJECT

private:
    QLocale::Language _preferredAudio;
    QLocale::Language _preferredSubtitle;
    bool _wantSubtitle;
    LoopMode _loopMode;
    WaitMode _waitMode;

    QList<PlaylistEntry> _entries;
    int _currentIndex;

    void emitMediaChanged();

public:
    Playlist();
    static Playlist* instance();

    QLocale::Language preferredAudio() const;
    void setPreferredAudio(const QLocale::Language& lang);
    QLocale::Language preferredSubtitle() const;
    void setPreferredSubtitle(const QLocale::Language& lang);
    bool wantSubtitle() const;
    void setWantSubtitle(bool want);
    QList<PlaylistEntry>& entries();
    const QList<PlaylistEntry>& entries() const;

    int length() const;
    void append(const PlaylistEntry& entry);
    void insert(int index, const PlaylistEntry& entry);
    void remove(int index);
    void clear();

    LoopMode loopMode() const;
    WaitMode waitMode() const;

    bool save(const QString& fileName, QString& errStr) const;
    bool load(const QString& fileName, QString& errStr);

public slots:
    void start();
    void stop();
    void next();
    void prev();
    void setCurrentIndex(int index);
    void setLoopMode(LoopMode loopMode);
    void setWaitMode(WaitMode waitMode);
    void setWaitModeAuto();

    void mediaEnded();

signals:
    void mediaChanged(PlaylistEntry entry);
};
