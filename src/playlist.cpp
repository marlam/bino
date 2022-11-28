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

#include "playlist.hpp"


PlaylistEntry::PlaylistEntry() :
    url(), inputMode(Input_Unknown), threeSixtyMode(ThreeSixty_Unknown),
    videoTrack(NoTrack), audioTrack(NoTrack), subtitleTrack(NoTrack)
{
}

PlaylistEntry::PlaylistEntry(const QUrl& url,
        InputMode inputMode,
        ThreeSixtyMode threeSixtyMode,
        int videoTrack, int audioTrack, int subtitleTrack) :
    url(url), inputMode(inputMode), threeSixtyMode(threeSixtyMode),
    videoTrack(videoTrack), audioTrack(audioTrack), subtitleTrack(subtitleTrack)
{
}

bool PlaylistEntry::noMedia() const
{
    return url.isEmpty();
}


static Playlist* playlistSingleton = nullptr;

Playlist::Playlist() :
    _preferredAudio(QLocale::system().language()),
    _preferredSubtitle(QLocale::system().language()),
    _wantSubtitle(false),
    _currentIndex(-1),
    _loopMode(Loop_Off)
{
    Q_ASSERT(!playlistSingleton);
    playlistSingleton = this;
}

Playlist* Playlist::instance()
{
    return playlistSingleton;
}

QLocale::Language Playlist::preferredAudio() const
{
    return _preferredAudio;
}

void Playlist::setPreferredAudio(const QLocale::Language& lang)
{
    _preferredAudio = lang;
}

QLocale::Language Playlist::preferredSubtitle() const
{
    return _preferredSubtitle;
}

void Playlist::setPreferredSubtitle(const QLocale::Language& lang)
{
    _preferredSubtitle = lang;
}

bool Playlist::wantSubtitle() const
{
    return _wantSubtitle;
}

void Playlist::setWantSubtitle(bool want)
{
    _wantSubtitle = want;
}

const QList<PlaylistEntry>& Playlist::entries() const
{
    return _entries;
}

void Playlist::emitMediaChanged()
{
    if (_currentIndex >= 0 && _currentIndex < length()) {
        emit mediaChanged(_entries[_currentIndex]);
    } else {
        emit mediaChanged(PlaylistEntry());
    }
}

int Playlist::length() const
{
    return _entries.length();
}

void Playlist::append(const PlaylistEntry& entry)
{
    _entries.append(entry);
}

void Playlist::insert(int index, const PlaylistEntry& entry)
{
    _entries.insert(index, entry);
}

void Playlist::remove(int index)
{
    if (index >= 0 && index < length()) {
        _entries.remove(index);
        if (_currentIndex == index) {
            if (_currentIndex >= length())
                _currentIndex--;
            emitMediaChanged();
        } else if (_currentIndex > index) {
            _currentIndex--;
        }
    }
}

void Playlist::clear()
{
    _entries.clear();
    int prevIndex = _currentIndex;
    _currentIndex = -1;
    if (prevIndex >= 0)
        emitMediaChanged();
}

void Playlist::start()
{
    if (length() > 0 && _currentIndex < 0)
        setCurrentIndex(0);
}

void Playlist::stop()
{
    setCurrentIndex(-1);
}

void Playlist::next()
{
    if (length() > 0) {
        if (_currentIndex == length() - 1)
            setCurrentIndex(0);
        else
            setCurrentIndex(_currentIndex++);
    }
}

void Playlist::prev()
{
    if (length() > 0) {
        if (_currentIndex == 0)
            setCurrentIndex(length() - 1);
        else
            setCurrentIndex(_currentIndex--);
    }
}

void Playlist::setCurrentIndex(int index)
{
    if (index == _currentIndex) {
        return;
    }

    if (length() == 0 || index < 0) {
        _currentIndex = -1;
        emitMediaChanged();
        return;
    }

    if (index >= length()) {
        index = length() - 1;
    }
    if (_currentIndex != index) {
        _currentIndex = index;
        emitMediaChanged();
    }
}

PlaylistLoopMode Playlist::loopMode() const
{
    return _loopMode;
}

void Playlist::setLoopMode(PlaylistLoopMode loopMode)
{
    _loopMode = loopMode;
}

void Playlist::mediaEnded()
{
    if (loopMode() == Loop_One) {
        // keep current index but play again
        emitMediaChanged();
    } else if (_currentIndex == length() - 1 && loopMode() == Loop_All) {
        // start with first index
        setCurrentIndex(0);
    } else if (_currentIndex < length() - 1) {
        // start with next index
        setCurrentIndex(_currentIndex + 1);
    }
}
