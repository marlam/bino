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

#ifndef INOUTWIDGET_H
#define INOUTWIDGET_H

#include "config.h"

#include <QWidget>

#include "dispatch.h"

class QSettings;
class QComboBox;
class QCheckBox;

class in_out_widget : public QWidget, public controller
{
    Q_OBJECT

private:
    QSettings *_settings;
    QComboBox *_video_combobox;
    QComboBox *_audio_combobox;
    QComboBox *_subtitle_combobox;
    QComboBox *_input_combobox;
    QComboBox *_output_combobox;
    QCheckBox *_swap_checkbox;
    bool _lock;

    void set_stereo_layout(parameters::stereo_layout_t stereo_layout, bool stereo_layout_swap);
    void set_stereo_mode(parameters::stereo_mode_t stereo_mode, bool stereo_mode_swap);

private slots:
    void video_changed();
    void audio_changed();
    void subtitle_changed();
    void input_changed();
    void output_changed();
    void swap_changed();

public:
    in_out_widget(QSettings *settings, QWidget *parent);

    void update();

    int get_video_stream();
    int get_audio_stream();
    int get_subtitle_stream();
    void get_stereo_layout(parameters::stereo_layout_t &stereo_layout, bool &stereo_layout_swap);
    void get_stereo_mode(parameters::stereo_mode_t &stereo_mode, bool &stereo_mode_swap);

    virtual void receive_notification(const notification &note);
};

#endif
