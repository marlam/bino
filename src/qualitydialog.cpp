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
#include <QSlider>
#include <QSpinBox>
#include <QGridLayout>

#include "qualitydialog.h"

#include "gui_common.h"

quality_dialog::quality_dialog(QWidget *parent) : QWidget(parent), _lock(false)
{
    QLabel *q_label = new QLabel(_("Rendering Quality:"));
    _q_slider = new QSlider(Qt::Horizontal);
    _q_slider->setRange(0, 4);
    _q_slider->setValue(dispatch::parameters().quality());
    connect(_q_slider, SIGNAL(valueChanged(int)), this, SLOT(q_slider_changed(int)));
    _q_spinbox = new QSpinBox();
    _q_spinbox->setRange(0, 4);
    _q_spinbox->setValue(dispatch::parameters().quality());
    _q_spinbox->setSingleStep(1);
    connect(_q_spinbox, SIGNAL(valueChanged(int)), this, SLOT(q_spinbox_changed(int)));

    QGridLayout *layout = new QGridLayout;
    layout->addWidget(q_label, 0, 0);
    layout->addWidget(_q_slider, 0, 1);
    layout->addWidget(_q_spinbox, 0, 2);
    setLayout(layout);
}

void quality_dialog::q_slider_changed(int val)
{
    if (!_lock)
        send_cmd(command::set_quality, val);
}

void quality_dialog::q_spinbox_changed(int val)
{
    if (!_lock)
        send_cmd(command::set_quality, val);
}

void quality_dialog::receive_notification(const notification &note)
{
    switch (note.type)
    {
    case notification::quality:
        _lock = true;
        _q_slider->setValue(dispatch::parameters().quality());
        _q_spinbox->setValue(dispatch::parameters().quality());
        _lock = false;
        break;
    default:
        /* not handled */
        break;
    }
}
