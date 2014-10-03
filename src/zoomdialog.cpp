/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010, 2011, 2012, 2013, 2014
 * Martin Lambers <marlam@marlam.de>
 * Frédéric Devernay <Frederic.Devernay@inrialpes.fr>
 * Joe <cuchac@email.cz>
 * Daniel Schaal <farbing@web.de>
 * D. Matz <bandregent@yahoo.de>
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

#include "config.h"

#include <QLabel>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QPushButton>
#include <QGridLayout>

#include "zoomdialog.h"

#include "gui_common.h"

zoom_dialog::zoom_dialog(QWidget *parent) : QWidget(parent), _lock(false)
{
    QLabel *info_label = new QLabel(_(
                "<p>Set zoom level for videos that<br>"
                "are wider than the screen:<br>"
                "0: Show full video width.<br>"
                "1: Use full screen height.</p>"));
    QLabel *z_label = new QLabel(_("Zoom:"));
    z_label->setToolTip(_("<p>Set the zoom level for videos that are wider than the screen.</p>"));
    _z_slider = new QSlider(Qt::Horizontal);
    _z_slider->setRange(0, 1000);
    _z_slider->setValue(dispatch::parameters().zoom() * 1000.0f);
    _z_slider->setToolTip(z_label->toolTip());
    connect(_z_slider, SIGNAL(valueChanged(int)), this, SLOT(z_slider_changed(int)));
    _z_spinbox = new QDoubleSpinBox();
    _z_spinbox->setRange(0.0, 1.0);
    _z_spinbox->setValue(dispatch::parameters().zoom());
    _z_spinbox->setDecimals(2);
    _z_spinbox->setSingleStep(0.01);
    _z_spinbox->setToolTip(z_label->toolTip());
    connect(_z_spinbox, SIGNAL(valueChanged(double)), this, SLOT(z_spinbox_changed(double)));

    QGridLayout *layout = new QGridLayout;
    layout->addWidget(info_label, 0, 0, 1, 3);
    layout->addWidget(z_label, 1, 0);
    layout->addWidget(_z_slider, 1, 1);
    layout->addWidget(_z_spinbox, 1, 2);
    setLayout(layout);
}

void zoom_dialog::z_slider_changed(int val)
{
    if (!_lock)
        send_cmd(command::set_zoom, val / 1000.0f);
}

void zoom_dialog::z_spinbox_changed(double val)
{
    if (!_lock)
        send_cmd(command::set_zoom, static_cast<float>(val));
}

void zoom_dialog::receive_notification(const notification &note)
{
    switch (note.type)
    {
    case notification::zoom:
        _lock = true;
        _z_slider->setValue(dispatch::parameters().zoom() * 1000.0f);
        _z_spinbox->setValue(dispatch::parameters().zoom());
        _lock = false;
        break;
    default:
        /* not handled */
        break;
    }
}
