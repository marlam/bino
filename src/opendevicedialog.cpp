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

#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QLabel>
#include <QGridLayout>
#include <QGroupBox>
#include <QStackedWidget>
#include <QPushButton>

#include "opendevicedialog.h"

#include "gui_common.h"

open_device_dialog::open_device_dialog(
        const QStringList &default_devices,
        const QStringList &firewire_devices,
        const QStringList &last_devices,
        const device_request &dev_request,
        QWidget *parent) :
    QDialog(parent)
{
    setWindowTitle(_("Open device(s)"));

    _type_combobox = new QComboBox();
    _type_combobox->setToolTip(_("<p>Choose a device type.</p>"));
    _type_combobox->addItem(_("Default"));
    _type_combobox->addItem(_("Firewire"));
    _type_combobox->addItem(_("X11"));
    for (int i = 0; i < 2; i++) {
        _default_device_combobox[i] = new QComboBox();
        _default_device_combobox[i]->setToolTip(_("<p>Choose a device.</p>"));
        _default_device_combobox[i]->addItems(default_devices);
        _firewire_device_combobox[i] = new QComboBox();
        _firewire_device_combobox[i]->setToolTip(_("<p>Choose a device.</p>"));
        _firewire_device_combobox[i]->addItems(firewire_devices);
        _x11_device_field[i] = new QLineEdit();
        _x11_device_field[i]->setToolTip(_("<p>Set the X11 device string. "
                    "Refer to the manual for details.</p>"));
        _x11_device_field[i]->setText("localhost:0.0+0,0");
        _device_chooser_stack[i] = new QStackedWidget();
        _device_chooser_stack[i]->addWidget(_default_device_combobox[i]);
        _device_chooser_stack[i]->addWidget(_firewire_device_combobox[i]);
        _device_chooser_stack[i]->addWidget(_x11_device_field[i]);
        if (last_devices.size() > i) {
            int j;
            if (dev_request.device == device_request::sys_default
                    && (j = _default_device_combobox[i]->findText(last_devices[i])) >= 0) {
                _default_device_combobox[i]->setCurrentIndex(j);
            } else if (dev_request.device == device_request::firewire
                    && (j = _firewire_device_combobox[i]->findText(last_devices[i])) >= 0) {
                _firewire_device_combobox[i]->setCurrentIndex(j);
            } else if (dev_request.device == device_request::x11) {
                _x11_device_field[i]->setText(last_devices[i]);
            }
        }
    }
    connect(_type_combobox, SIGNAL(currentIndexChanged(int)), _device_chooser_stack[0], SLOT(setCurrentIndex(int)));
    connect(_type_combobox, SIGNAL(currentIndexChanged(int)), _device_chooser_stack[1], SLOT(setCurrentIndex(int)));
    _type_combobox->setCurrentIndex(dev_request.device == device_request::firewire ? 1
            : dev_request.device == device_request::x11 ? 2 : 0);
    QLabel *first_device_label = new QLabel(_("First device:"));
    _second_device_checkbox = new QCheckBox(_("Second device:"));
    _second_device_checkbox->setChecked(last_devices.size() > 1);
    _device_chooser_stack[1]->setEnabled(last_devices.size() > 1);
    connect(_second_device_checkbox, SIGNAL(toggled(bool)), _device_chooser_stack[1], SLOT(setEnabled(bool)));
    _frame_size_groupbox = new QGroupBox(_("Request frame size"));
    _frame_size_groupbox->setToolTip(_("<p>Request a specific frame size from the device, e.g. 640x480. "
                "The device must support this frame size. Some devices require a frame size to be selected.</p>"));
    _frame_size_groupbox->setCheckable(true);
    _frame_size_groupbox->setChecked(dev_request.width != 0 && dev_request.height != 0);
    connect(_frame_size_groupbox, SIGNAL(clicked(bool)), this, SLOT(frame_size_groupbox_clicked(bool)));
    _frame_width_spinbox = new QSpinBox();
    _frame_width_spinbox->setRange(_frame_size_groupbox->isChecked() ? 1 : 0, 65535);
    _frame_width_spinbox->setValue(dev_request.width);
    _frame_height_spinbox = new QSpinBox();
    _frame_height_spinbox->setRange(_frame_size_groupbox->isChecked() ? 1 : 0, 65535);
    _frame_height_spinbox->setValue(dev_request.height);
    _frame_rate_groupbox = new QGroupBox(_("Request frame rate"));
    _frame_rate_groupbox->setToolTip(_("<p>Request a specific frame rate from the device, e.g. 25/1. "
                "The device must support this frame rate. Some devices require a frame rate to be selected.</p>"));
    _frame_rate_groupbox->setCheckable(true);
    _frame_rate_groupbox->setChecked(dev_request.frame_rate_num != 0 && dev_request.frame_rate_den != 0);
    connect(_frame_rate_groupbox, SIGNAL(clicked(bool)), this, SLOT(frame_rate_groupbox_clicked(bool)));
    _frame_rate_num_spinbox = new QSpinBox();
    _frame_rate_num_spinbox->setRange(_frame_rate_groupbox->isChecked() ? 1 : 0, 65535);
    _frame_rate_num_spinbox->setValue(dev_request.frame_rate_num);
    _frame_rate_den_spinbox = new QSpinBox();
    _frame_rate_den_spinbox->setRange(_frame_rate_groupbox->isChecked() ? 1 : 0, 65535);
    _frame_rate_den_spinbox->setValue(dev_request.frame_rate_den);
    _mjpeg_checkbox = new QCheckBox(_("Request MJPEG format"));
    _mjpeg_checkbox->setToolTip(_("<p>Request MJPEG data from the input device. "
                "The device may ignore this request.</p>"));
    _mjpeg_checkbox->setChecked(dev_request.request_mjpeg);
    QPushButton *cancel_btn = new QPushButton(_("Cancel"));
    connect(cancel_btn, SIGNAL(clicked()), this, SLOT(reject()));
    QPushButton *ok_btn = new QPushButton(_("OK"));
    connect(ok_btn, SIGNAL(clicked()), this, SLOT(accept()));

    QGridLayout *frame_size_layout = new QGridLayout;
    frame_size_layout->addWidget(_frame_width_spinbox, 0, 0);
    frame_size_layout->addWidget(new QLabel("x"), 0, 1);
    frame_size_layout->addWidget(_frame_height_spinbox, 0, 2);
    _frame_size_groupbox->setLayout(frame_size_layout);
    QGridLayout *frame_rate_layout = new QGridLayout;
    frame_rate_layout->addWidget(_frame_rate_num_spinbox, 0, 0);
    frame_rate_layout->addWidget(new QLabel("/"), 0, 1);
    frame_rate_layout->addWidget(_frame_rate_den_spinbox, 0, 2);
    _frame_rate_groupbox->setLayout(frame_rate_layout);

    QGridLayout *layout = new QGridLayout;
    layout->addWidget(_type_combobox, 0, 0, 1, 2);
    layout->addWidget(first_device_label, 1, 0);
    layout->addWidget(_device_chooser_stack[0], 1, 1);
    layout->addWidget(_second_device_checkbox, 2, 0);
    layout->addWidget(_device_chooser_stack[1], 2, 1);
    layout->addWidget(_frame_size_groupbox, 3, 0, 1, 2);
    layout->addWidget(_frame_rate_groupbox, 4, 0, 1, 2);
    layout->addWidget(_mjpeg_checkbox, 5, 0, 1, 2);
    layout->addWidget(cancel_btn, 6, 0);
    layout->addWidget(ok_btn, 6, 1);
    layout->setRowStretch(1, 1);
    setLayout(layout);
}

void open_device_dialog::frame_size_groupbox_clicked(bool checked)
{
    _frame_width_spinbox->setRange(checked ? 1 : 0, 65535);
    _frame_width_spinbox->setValue(checked ? 640 : 0);
    _frame_height_spinbox->setRange(checked ? 1 : 0, 65535);
    _frame_height_spinbox->setValue(checked ? 480 : 0);
}

void open_device_dialog::frame_rate_groupbox_clicked(bool checked)
{
    _frame_rate_num_spinbox->setRange(checked ? 1 : 0, 65535);
    _frame_rate_num_spinbox->setValue(checked ? 25 : 0);
    _frame_rate_den_spinbox->setRange(checked ? 1 : 0, 65535);
    _frame_rate_den_spinbox->setValue(checked ? 1 : 0);
}

void open_device_dialog::request(QStringList &devices, device_request &dev_request)
{
    if (_type_combobox->currentIndex() == 1)
    {
        dev_request.device = device_request::firewire;
        devices.push_back(_firewire_device_combobox[0]->currentText());
        if (_second_device_checkbox->isChecked())
            devices.push_back(_firewire_device_combobox[1]->currentText());
    }
    else if (_type_combobox->currentIndex() == 2)
    {
        dev_request.device = device_request::x11;
        devices.push_back(_x11_device_field[0]->text());
        if (_second_device_checkbox->isChecked())
            devices.push_back(_x11_device_field[1]->text());
    }
    else
    {
        dev_request.device = device_request::sys_default;
        devices.push_back(_default_device_combobox[0]->currentText());
        if (_second_device_checkbox->isChecked())
            devices.push_back(_default_device_combobox[1]->currentText());
    }
    dev_request.width = _frame_size_groupbox->isChecked() ? _frame_width_spinbox->value() : 0;
    dev_request.height = _frame_size_groupbox->isChecked() ? _frame_height_spinbox->value() : 0;
    dev_request.frame_rate_num = _frame_rate_groupbox->isChecked() ? _frame_rate_num_spinbox->value() : 0;
    dev_request.frame_rate_den = _frame_rate_groupbox->isChecked() ? _frame_rate_den_spinbox->value() : 0;
    dev_request.request_mjpeg = _mjpeg_checkbox->isChecked();
}
