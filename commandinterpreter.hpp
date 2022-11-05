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

#include <QFile>
#include <QList>
#include <QTimer>


class CommandInterpreter : public QObject
{
Q_OBJECT

private:
    QTimer _timer;
    QTimer _waitTimer;
    QFile _file;
    QList<char> _lineBuf;
    int _lineIndex;
    bool _waitForStop;

public:
    CommandInterpreter();

    bool init(const QString& fileName);
    void start();

public slots:
    void processNextCommand();
};
