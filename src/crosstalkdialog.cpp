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
#include <QGridLayout>

#include "crosstalkdialog.h"
#include "gui_common.h"

crosstalk_dialog::crosstalk_dialog(QWidget *parent) : QWidget(parent), _lock(false)
{
    /* TRANSLATORS: Please keep the lines short using <br> where necessary. */
    QLabel *rtfm_label = new QLabel(_("<p>Please read the manual to find out<br>"
                "how to measure the crosstalk levels<br>"
                "of your display.</p>"));

    QLabel *r_label = new QLabel(_("Red:"));
    _r_spinbox = new QDoubleSpinBox();
    _r_spinbox->setRange(0.0, +1.0);
    _r_spinbox->setValue(dispatch::parameters().crosstalk_r());
    _r_spinbox->setDecimals(2);
    _r_spinbox->setSingleStep(0.01);
    connect(_r_spinbox, SIGNAL(valueChanged(double)), this, SLOT(spinbox_changed()));
    QLabel *g_label = new QLabel(_("Green:"));
    _g_spinbox = new QDoubleSpinBox();
    _g_spinbox->setRange(0.0, +1.0);
    _g_spinbox->setValue(dispatch::parameters().crosstalk_g());
    _g_spinbox->setDecimals(2);
    _g_spinbox->setSingleStep(0.01);
    connect(_g_spinbox, SIGNAL(valueChanged(double)), this, SLOT(spinbox_changed()));
    QLabel *b_label = new QLabel(_("Blue:"));
    _b_spinbox = new QDoubleSpinBox();
    _b_spinbox->setRange(0.0, +1.0);
    _b_spinbox->setValue(dispatch::parameters().crosstalk_b());
    _b_spinbox->setDecimals(2);
    _b_spinbox->setSingleStep(0.01);
    connect(_b_spinbox, SIGNAL(valueChanged(double)), this, SLOT(spinbox_changed()));

    QGridLayout *layout = new QGridLayout;
    layout->addWidget(rtfm_label, 0, 0, 1, 2);
    layout->addWidget(r_label, 2, 0);
    layout->addWidget(_r_spinbox, 2, 1);
    layout->addWidget(g_label, 3, 0);
    layout->addWidget(_g_spinbox, 3, 1);
    layout->addWidget(b_label, 4, 0);
    layout->addWidget(_b_spinbox, 4, 1);
    setLayout(layout);
}

void crosstalk_dialog::spinbox_changed()
{
    if (!_lock)
    {
        std::ostringstream v;
        s11n::save(v, static_cast<float>(_r_spinbox->value()));
        s11n::save(v, static_cast<float>(_g_spinbox->value()));
        s11n::save(v, static_cast<float>(_b_spinbox->value()));
        send_cmd(command::set_crosstalk, v.str());
    }
}

void crosstalk_dialog::receive_notification(const notification &note)
{
    switch (note.type)
    {
    case notification::crosstalk:
        _lock = true;
        _r_spinbox->setValue(dispatch::parameters().crosstalk_r());
        _g_spinbox->setValue(dispatch::parameters().crosstalk_g());
        _b_spinbox->setValue(dispatch::parameters().crosstalk_b());
        _lock = false;
        break;
    default:
        /* not handled */
        break;
    }
}
