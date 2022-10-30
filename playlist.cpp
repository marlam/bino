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
    url(), videoTrack(NoTrack), audioTrack(NoTrack), subtitleTrack(NoTrack)
{
}

PlaylistEntry::PlaylistEntry(const QUrl& url,
        VideoFrame::StereoLayout stereoLayout,
        VideoFrame::ThreeSixtyMode threeSixtyMode,
        int videoTrack, int audioTrack, int subtitleTrack) :
    url(url), stereoLayout(stereoLayout), threeSixtyMode(threeSixtyMode),
    videoTrack(videoTrack), audioTrack(audioTrack), subtitleTrack(subtitleTrack)
{
}

bool PlaylistEntry::noMedia() const
{
    return url.isEmpty();
}


static Playlist* playlistSingleton = nullptr;

Playlist::Playlist() :
    preferredAudio(QLocale::system().language()),
    preferredSubtitle(QLocale::system().language()),
    wantSubtitle(false),
    currentIndex(-1)
{
    Q_ASSERT(!playlistSingleton);
    playlistSingleton = this;
}

Playlist* Playlist::instance()
{
    return playlistSingleton;
}

void Playlist::emitMediaChanged()
{
    if (currentIndex >= 0 && currentIndex < length()) {
        emit mediaChanged(entries[currentIndex]);
    } else {
        emit mediaChanged(PlaylistEntry());
    }
}

int Playlist::length() const
{
    return entries.length();
}

void Playlist::append(const PlaylistEntry& entry)
{
    entries.append(entry);
}

void Playlist::insert(int index, const PlaylistEntry& entry)
{
    entries.insert(index, entry);
}

void Playlist::remove(int index)
{
    if (index >= 0 && index < length()) {
        entries.remove(index);
        if (currentIndex == index) {
            if (currentIndex >= length())
                currentIndex--;
            emitMediaChanged();
        } else if (currentIndex > index) {
            currentIndex--;
        }
    }
}

void Playlist::clear()
{
    entries.clear();
    int prevIndex = currentIndex;
    currentIndex = -1;
    if (prevIndex >= 0)
        emitMediaChanged();
}

void Playlist::start()
{
    if (length() > 0 && currentIndex < 0)
        setCurrentIndex(0);
}

void Playlist::stop()
{
    setCurrentIndex(-1);
}

void Playlist::next()
{
    if (length() > 0) {
        if (currentIndex == length() - 1)
            setCurrentIndex(0);
        else
            setCurrentIndex(currentIndex++);
    }
}

void Playlist::prev()
{
    if (length() > 0) {
        if (currentIndex == 0)
            setCurrentIndex(length() - 1);
        else
            setCurrentIndex(currentIndex--);
    }
}

void Playlist::setCurrentIndex(int index)
{
    if (index == currentIndex) {
        return;
    }

    if (length() == 0 || index < 0) {
        currentIndex = -1;
        emitMediaChanged();
        return;
    }

    if (index >= length()) {
        index = length() - 1;
    }
    if (currentIndex != index) {
        currentIndex = index;
        emitMediaChanged();
    }
}
