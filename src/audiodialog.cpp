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
#include <QComboBox>
#include <QSpinBox>
#include <QGridLayout>

#include "audiodialog.h"

#include "gui_common.h"
#include "audio_output.h"

audio_dialog::audio_dialog(QWidget *parent) : QWidget(parent), _lock(false)
{
    QLabel *device_label = new QLabel(_("Audio device:"));
    device_label->setToolTip(_("<p>Select the audio device.<br>"
                "This will take effect for the next started video.</p>"));
    _device_combobox = new QComboBox();
    _device_combobox->setToolTip(device_label->toolTip());
    _device_combobox->addItem(_("Default"));
    audio_output ao;
    for (int i = 0; i < ao.devices(); i++)
    {
        _device_combobox->addItem(ao.device_name(i).c_str());
    }
    connect(_device_combobox, SIGNAL(currentIndexChanged(int)), this, SLOT(device_changed()));
    if (dispatch::parameters().audio_device() < 0
            || dispatch::parameters().audio_device() >= _device_combobox->count())
    {
        _device_combobox->setCurrentIndex(0);
    }
    else
    {
        _device_combobox->setCurrentIndex(dispatch::parameters().audio_device() + 1);
    }

    QLabel *delay_label = new QLabel(_("Audio delay (ms):"));
    delay_label->setToolTip(_("<p>Set an audio delay, in milliseconds.<br>"
                "This is useful if audio and video are not in sync.</p>"));
    _delay_spinbox = new QSpinBox();
    _delay_spinbox->setToolTip(delay_label->toolTip());
    _delay_spinbox->setRange(-10000, +10000);
    _delay_spinbox->setSingleStep(1);
    _delay_spinbox->setValue(dispatch::parameters().audio_delay() / 1000);
    connect(_delay_spinbox, SIGNAL(valueChanged(int)), this, SLOT(delay_changed()));

    QGridLayout *layout = new QGridLayout;
    layout->addWidget(device_label, 0, 0);
    layout->addWidget(_device_combobox, 0, 1);
    layout->addWidget(delay_label, 1, 0);
    layout->addWidget(_delay_spinbox, 1, 1);
    setLayout(layout);
}

void audio_dialog::device_changed()
{
    if (!_lock)
        send_cmd(command::set_audio_device, _device_combobox->currentIndex() - 1);
}

void audio_dialog::delay_changed()
{
    if (!_lock)
        send_cmd(command::set_audio_delay, static_cast<int64_t>(_delay_spinbox->value() * 1000));
}

void audio_dialog::receive_notification(const notification &note)
{
    switch (note.type)
    {
    case notification::audio_device:
        _lock = true;
        _device_combobox->setCurrentIndex(dispatch::parameters().audio_device() + 1);
        _lock = false;
        break;
    case notification::audio_delay:
        _lock = true;
        _delay_spinbox->setValue(dispatch::parameters().audio_delay() / 1000);
        _lock = false;
        break;
    default:
        /* not handled */
        break;
    }
}
