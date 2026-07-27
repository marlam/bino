/*
 * This file is part of Bino, a 3D video player.
 *
 * Copyright (C) 2022, 2023, 2024, 2025, 2026
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
#include <QSocketNotifier>
#include <QLocalServer>
#include <QTcpServer>
#include <QTimer>


class CommandInterpreter : public QObject
{
Q_OBJECT

public:
    enum Type {
        Type_File,
        Type_FIFO,
        Type_LocalSocket,
        Type_TcpSocket
    };

private:
    // Initialization information
    enum Type _type;
    QString _name;
    // Type File and FIFO:
    QFile _file;
    // Type FIFO:
    QSocketNotifier _notifier;
    // Type Socket:
    QLocalServer _localServer;
    // Type TcpSocket:
    QTcpServer _tcpServer;
    // List of commands: new lines will be appended when they become available,
    // and the first line(s) will be consumed by the next processCommand() call.
    QStringList _lineList;
    bool _moreLinesInFuture;
    int _lineNumber;
    // Timer handling required by processCommand():
    bool _waitForStop;
    QTimer _timer;
    QTimer _waitTimer;

private Q_SLOTS:
    void processLine();

public:
    CommandInterpreter();

    bool init(enum Type type, const QString& name);
    void start();
};
