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

#ifndef OPENDEVICEDIALOG_H
#define OPENDEVICEDIALOG_H

#include "config.h"

#include <QDialog>
#include "dispatch.h"

class QComboBox;
class QSpinBox;
class QStackedWidget;
class QLineEdit;
class QCheckBox;
class QGroupBox;

class open_device_dialog : public QDialog
{
    Q_OBJECT

private:
    QComboBox *_type_combobox;
    QStackedWidget *_device_chooser_stack[2];
    QComboBox *_default_device_combobox[2];
    QComboBox *_firewire_device_combobox[2];
    QLineEdit *_x11_device_field[2];
    QCheckBox *_second_device_checkbox;
    QGroupBox *_frame_size_groupbox;
    QSpinBox *_frame_width_spinbox;
    QSpinBox *_frame_height_spinbox;
    QGroupBox *_frame_rate_groupbox;
    QSpinBox *_frame_rate_num_spinbox;
    QSpinBox *_frame_rate_den_spinbox;
    QCheckBox *_mjpeg_checkbox;

private slots:
    void frame_size_groupbox_clicked(bool checked);
    void frame_rate_groupbox_clicked(bool checked);

public:
    open_device_dialog(const QStringList &default_devices, const QStringList &firewire_devices,
            const QStringList &last_devices, const device_request &dev_request, QWidget *parent);
    void request(QStringList &devices, device_request &dev_request);
};

#endif
