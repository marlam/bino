/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010, 2011, 2012, 2013
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

#ifndef COLORDIALOG_H
#define COLORDIALOG_H

#include "config.h"

#include <QWidget>
#include "dispatch.h"

class QDoubleSpinBox;
class QSlider;

class color_dialog : public QWidget, public controller
{
    Q_OBJECT

private:
    bool _lock;
    QDoubleSpinBox *_c_spinbox;
    QSlider *_c_slider;
    QDoubleSpinBox *_b_spinbox;
    QSlider *_b_slider;
    QDoubleSpinBox *_h_spinbox;
    QSlider *_h_slider;
    QDoubleSpinBox *_s_spinbox;
    QSlider *_s_slider;

private slots:
    void c_slider_changed(int val);
    void c_spinbox_changed(double val);
    void b_slider_changed(int val);
    void b_spinbox_changed(double val);
    void h_slider_changed(int val);
    void h_spinbox_changed(double val);
    void s_slider_changed(int val);
    void s_spinbox_changed(double val);

public:
    color_dialog(QWidget *parent = 0);

    virtual void receive_notification(const notification &note);
};


#endif
