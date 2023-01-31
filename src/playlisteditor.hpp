/*
 * This file is part of Bino, a 3D video player.
 *
 * Copyright (C) 2023
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

#include <QDialog>
class QLabel;
class QComboBox;
class QPushButton;
class QTableWidget;

#include "playlist.hpp"


class PlaylistEntryEditor : public QDialog
{
Q_OBJECT

private:
    QLabel* urlLabel;
    QComboBox* inputModeBox;
    QComboBox* surroundModeBox;
    QComboBox* videoTrackBox;
    QComboBox* audioTrackBox;
    QComboBox* subtitleTrackBox;

    void updateBoxStates();

private slots:
    void updateEntry();
    void setFile();
    void setURL();

public:
    PlaylistEntry entry;

    PlaylistEntryEditor(const PlaylistEntry& entry, QWidget* parent);
};

class PlaylistEditor : public QDialog
{
Q_OBJECT

private:
    QTableWidget* table;
    QPushButton* upBtn;
    QPushButton* downBtn;
    QPushButton* addBtn;
    QPushButton* delBtn;
    QPushButton* editBtn;

    void updateTable();
    int selectedRow();

private slots:
    void updateButtonState();
    void up();
    void down();
    void add();
    void del();
    void edit();

public:
    PlaylistEditor(QWidget* parent);
};
