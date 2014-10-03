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
#include <QDoubleSpinBox>
#include <QGridLayout>

#include "colordialog.h"

#include "gui_common.h"

color_dialog::color_dialog(QWidget *parent) : QWidget(parent), _lock(false)
{
    QLabel *c_label = new QLabel(_("Contrast:"));
    _c_slider = new QSlider(Qt::Horizontal);
    _c_slider->setRange(-1000, 1000);
    _c_slider->setValue(dispatch::parameters().contrast() * 1000.0f);
    connect(_c_slider, SIGNAL(valueChanged(int)), this, SLOT(c_slider_changed(int)));
    _c_spinbox = new QDoubleSpinBox();
    _c_spinbox->setRange(-1.0, +1.0);
    _c_spinbox->setValue(dispatch::parameters().contrast());
    _c_spinbox->setDecimals(2);
    _c_spinbox->setSingleStep(0.01);
    connect(_c_spinbox, SIGNAL(valueChanged(double)), this, SLOT(c_spinbox_changed(double)));
    QLabel *b_label = new QLabel(_("Brightness:"));
    _b_slider = new QSlider(Qt::Horizontal);
    _b_slider->setRange(-1000, 1000);
    _b_slider->setValue(dispatch::parameters().brightness() * 1000.0f);
    connect(_b_slider, SIGNAL(valueChanged(int)), this, SLOT(b_slider_changed(int)));
    _b_spinbox = new QDoubleSpinBox();
    _b_spinbox->setRange(-1.0, +1.0);
    _b_spinbox->setValue(dispatch::parameters().brightness());
    _b_spinbox->setDecimals(2);
    _b_spinbox->setSingleStep(0.01);
    connect(_b_spinbox, SIGNAL(valueChanged(double)), this, SLOT(b_spinbox_changed(double)));
    QLabel *h_label = new QLabel(_("Hue:"));
    _h_slider = new QSlider(Qt::Horizontal);
    _h_slider->setRange(-1000, 1000);
    _h_slider->setValue(dispatch::parameters().hue() * 1000.0f);
    connect(_h_slider, SIGNAL(valueChanged(int)), this, SLOT(h_slider_changed(int)));
    _h_spinbox = new QDoubleSpinBox();
    _h_spinbox->setRange(-1.0, +1.0);
    _h_spinbox->setValue(dispatch::parameters().hue());
    _h_spinbox->setDecimals(2);
    _h_spinbox->setSingleStep(0.01);
    connect(_h_spinbox, SIGNAL(valueChanged(double)), this, SLOT(h_spinbox_changed(double)));
    QLabel *s_label = new QLabel(_("Saturation:"));
    _s_slider = new QSlider(Qt::Horizontal);
    _s_slider->setRange(-1000, 1000);
    _s_slider->setValue(dispatch::parameters().saturation() * 1000.0f);
    connect(_s_slider, SIGNAL(valueChanged(int)), this, SLOT(s_slider_changed(int)));
    _s_spinbox = new QDoubleSpinBox();
    _s_spinbox->setRange(-1.0, +1.0);
    _s_spinbox->setValue(dispatch::parameters().saturation());
    _s_spinbox->setDecimals(2);
    _s_spinbox->setSingleStep(0.01);
    connect(_s_spinbox, SIGNAL(valueChanged(double)), this, SLOT(s_spinbox_changed(double)));

    QGridLayout *layout = new QGridLayout;
    layout->addWidget(c_label, 0, 0);
    layout->addWidget(_c_slider, 0, 1);
    layout->addWidget(_c_spinbox, 0, 2);
    layout->addWidget(b_label, 1, 0);
    layout->addWidget(_b_slider, 1, 1);
    layout->addWidget(_b_spinbox, 1, 2);
    layout->addWidget(h_label, 2, 0);
    layout->addWidget(_h_slider, 2, 1);
    layout->addWidget(_h_spinbox, 2, 2);
    layout->addWidget(s_label, 3, 0);
    layout->addWidget(_s_slider, 3, 1);
    layout->addWidget(_s_spinbox, 3, 2);
    setLayout(layout);
}

void color_dialog::c_slider_changed(int val)
{
    if (!_lock)
        send_cmd(command::set_contrast, val / 1000.0f);
}

void color_dialog::c_spinbox_changed(double val)
{
    if (!_lock)
        send_cmd(command::set_contrast, static_cast<float>(val));
}

void color_dialog::b_slider_changed(int val)
{
    if (!_lock)
        send_cmd(command::set_brightness, val / 1000.0f);
}

void color_dialog::b_spinbox_changed(double val)
{
    if (!_lock)
        send_cmd(command::set_brightness, static_cast<float>(val));
}

void color_dialog::h_slider_changed(int val)
{
    if (!_lock)
        send_cmd(command::set_hue, val / 1000.0f);
}

void color_dialog::h_spinbox_changed(double val)
{
    if (!_lock)
        send_cmd(command::set_hue, static_cast<float>(val));
}

void color_dialog::s_slider_changed(int val)
{
    if (!_lock)
        send_cmd(command::set_saturation, val / 1000.0f);
}

void color_dialog::s_spinbox_changed(double val)
{
    if (!_lock)
        send_cmd(command::set_saturation, static_cast<float>(val));
}

void color_dialog::receive_notification(const notification &note)
{
    switch (note.type)
    {
    case notification::contrast:
        _lock = true;
        _c_slider->setValue(dispatch::parameters().contrast() * 1000.0f);
        _c_spinbox->setValue(dispatch::parameters().contrast());
        _lock = false;
        break;
    case notification::brightness:
        _lock = true;
        _b_slider->setValue(dispatch::parameters().brightness() * 1000.0f);
        _b_spinbox->setValue(dispatch::parameters().brightness());
        _lock = false;
        break;
    case notification::hue:
        _lock = true;
        _h_slider->setValue(dispatch::parameters().hue() * 1000.0f);
        _h_spinbox->setValue(dispatch::parameters().hue());
        _lock = false;
        break;
    case notification::saturation:
        _lock = true;
        _s_slider->setValue(dispatch::parameters().saturation() * 1000.0f);
        _s_spinbox->setValue(dispatch::parameters().saturation());
        _lock = false;
        break;
    default:
        /* not handled */
        break;
    }
}
