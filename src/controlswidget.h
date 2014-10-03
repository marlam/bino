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

#ifndef CONTROLSWIDGET_H
#define CONTROLSWIDGET_H

#include "config.h"

#include <QWidget>
#include "dispatch.h"

class QIcon;
class QSettings;
class QPushButton;
class QSlider;
class QLabel;

class controls_widget : public QWidget, public controller
{
    Q_OBJECT

private:
    int64_t _input_duration;
    bool _lock;
    QSettings *_settings;
    QPushButton *_play_button;
    QPushButton *_pause_button;
    QPushButton *_stop_button;
    QPushButton *_loop_button;
    QPushButton *_fullscreen_button;
    QPushButton *_center_button;
    QPushButton *_bbb_button;
    QPushButton *_bb_button;
    QPushButton *_b_button;
    QPushButton *_f_button;
    QPushButton *_ff_button;
    QPushButton *_fff_button;
    QSlider *_seek_slider;
    QLabel *_pos_label;
    QPushButton *_audio_mute_button;
    QSlider *_audio_volume_slider;

private:
    void update_audio_widgets();
    QIcon get_icon(const QString & name);

private slots:
    void play_clicked();
    void pause_clicked();
    void stop_clicked();
    void loop_clicked();
    void fullscreen_clicked();
    void center_clicked();
    void bbb_clicked();
    void bb_clicked();
    void b_clicked();
    void f_clicked();
    void ff_clicked();
    void fff_clicked();
    void seek_slider_changed();
    void audio_mute_clicked();
    void audio_volume_slider_changed();

public:
    controls_widget(QSettings *settings, QWidget *parent);

    void update();
    virtual void receive_notification(const notification &note);
};

#endif
