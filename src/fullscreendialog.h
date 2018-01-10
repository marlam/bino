/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010, 2011, 2012, 2013, 2018
 * Martin Lambers <marlam@marlam.de>
 * Frédéric Devernay <Frederic.Devernay@inrialpes.fr>
 * Joe <cuchac@email.cz>
 * Daniel Schaal <farbing@web.de>
 * Binocle <http://binocle.com> (author: Olivier Letz <oletz@binocle.com>)
 * Frédéric Bour <frederic.bour@lakaban.net>
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

#ifndef FULLSCREENDIALOG_H
#define FULLSCREENDIALOG_H

#include "config.h"

#include <QWidget>
#include "dispatch.h"

class QRadioButton;
class QComboBox;
class QLineEdit;
class QCheckBox;

class fullscreen_dialog : public QWidget, public controller
{
    Q_OBJECT

private:
    QRadioButton* _single_btn;
    QComboBox* _single_box;
    QRadioButton* _dual_btn;
    QComboBox* _dual_box0;
    QComboBox* _dual_box1;
    QRadioButton* _multi_btn;
    QLineEdit* _multi_edt;
    QCheckBox* _flip_left_box;
    QCheckBox* _flop_left_box;
    QCheckBox* _flip_right_box;
    QCheckBox* _flop_right_box;
    QCheckBox* _3d_ready_sync_box;
    QCheckBox* _inhibit_screensaver_box;

public:
    fullscreen_dialog(QWidget* parent = 0);
    void apply();
};

#endif
