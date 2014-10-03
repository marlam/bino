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

#ifndef VIDEODIALOG_H
#define VIDEODIALOG_H

#include "config.h"

#include <QWidget>
#include "dispatch.h"

class QComboBox;
class QDoubleSpinBox;
class QSlider;

class video_dialog : public QWidget, public controller
{
    Q_OBJECT

private:
    bool _lock;
    QComboBox *_crop_ar_combobox;
    QComboBox *_source_ar_combobox;
    QDoubleSpinBox *_p_spinbox;
    QSlider *_p_slider;
    QDoubleSpinBox *_sp_spinbox;
    QSlider *_sp_slider;
    QDoubleSpinBox *_g_spinbox;
    QSlider *_g_slider;

    void set_crop_ar(float val);
    void set_source_ar(float val);

private slots:
    void crop_ar_changed();
    void source_ar_changed();
    void p_slider_changed(int val);
    void p_spinbox_changed(double val);
    void sp_slider_changed(int val);
    void sp_spinbox_changed(double val);
    void g_slider_changed(int val);
    void g_spinbox_changed(double val);

public:
    video_dialog(QWidget *parent = 0);
    void update();

    virtual void receive_notification(const notification &note);
};

#endif
