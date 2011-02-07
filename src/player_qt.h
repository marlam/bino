/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010-2011
 * Martin Lambers <marlam@marlam.de>
 * Frédéric Devernay <Frederic.Devernay@inrialpes.fr>
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

#ifndef PLAYER_GUI_H
#define PLAYER_GUI_H

#include <QMainWindow>
#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QTimer>
#include <QSettings>

#include "controller.h"
#include "video_output_qt.h"
#include "player.h"


class player_qt_internal : public player, public controller
{
private:
    bool _playing;
    video_container_widget *_container_widget;

protected:
    video_output *create_video_output();

public:
    player_qt_internal(video_container_widget *widget = NULL);
    virtual ~player_qt_internal();

    virtual void receive_cmd(const command &cmd);

    virtual void receive_notification(const notification &note);

    bool playloop_step();
    void force_stop();
    void move_event();
};

class in_out_widget : public QWidget, public controller
{
    Q_OBJECT

private:
    QSettings *_settings;
    QComboBox *_input_combobox;
    QSpinBox *_audio_spinbox;
    QComboBox *_output_combobox;
    QPushButton *_swap_eyes_button;
    QPushButton *_fullscreen_button;
    QPushButton *_center_button;
    QLabel *_parallax_label;
    QDoubleSpinBox *_parallax_spinbox;
    QLabel *_ghostbust_label;
    QSpinBox *_ghostbust_spinbox;
    bool _lock;

    void set_input(video_frame::stereo_layout_t stereo_layout, bool stereo_layout_swap);
    void set_output(parameters::stereo_mode_t stereo_mode, bool stereo_mode_swap);

private slots:
    void input_changed();
    void swap_eyes_changed();
    void fullscreen_pressed();
    void center_pressed();
    void parallax_changed();
    void ghostbust_changed();

public:
    in_out_widget(QSettings *settings, QWidget *parent);
    virtual ~in_out_widget();

    void update(const player_init_data &init_data, bool have_valid_input, bool playing);

    void input(video_frame::stereo_layout_t &stereo_layout, bool &stereo_layout_swap);
    int audio_stream();
    void output(parameters::stereo_mode_t &stereo_mode, bool &stereo_mode_swap);

    virtual void receive_notification(const notification &note);
};

class controls_widget : public QWidget, public controller
{
    Q_OBJECT

private:
    bool _lock;
    QSettings *_settings;
    QPushButton *_play_button;
    QPushButton *_pause_button;
    QPushButton *_stop_button;
    QPushButton *_bbb_button;
    QPushButton *_bb_button;
    QPushButton *_b_button;
    QPushButton *_f_button;
    QPushButton *_ff_button;
    QPushButton *_fff_button;
    QSlider *_seek_slider;
    bool _playing;

private slots:
    void play_pressed();
    void pause_pressed();
    void stop_pressed();
    void bbb_pressed();
    void bb_pressed();
    void b_pressed();
    void f_pressed();
    void ff_pressed();
    void fff_pressed();
    void seek_slider_changed();

public:
    controls_widget(QSettings *settings, QWidget *parent);
    virtual ~controls_widget();

    void update(const player_init_data &init_data, bool have_valid_input, bool playing);
    virtual void receive_notification(const notification &note);
};

class main_window : public QMainWindow, public controller
{
    Q_OBJECT

private:
    QSettings *_settings;
    video_container_widget *_video_container_widget;
    in_out_widget *_in_out_widget;
    controls_widget *_controls_widget;
    player_qt_internal *_player;
    QTimer *_timer;
    player_init_data _init_data;
    bool _stop_request;

    QString current_file_hash();
    bool open_player();
    void open(QStringList filenames, bool automatic);

private slots:
    void move_event();
    void playloop_step();
    void file_open();
    void file_open_url();
    void preferences_crosstalk();
    void help_manual();
    void help_website();
    void help_keyboard();
    void help_about();

protected:
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);
    void moveEvent(QMoveEvent *event);
    void closeEvent(QCloseEvent *event);
    bool eventFilter(QObject *obj, QEvent *event);

public:
    main_window(QSettings *settings, const player_init_data &init_data);
    virtual ~main_window();

    virtual void receive_notification(const notification &note);
};

class player_qt : public player
{
private:
    bool _qt_app_owner;
    main_window *_main_window;
    QSettings *_settings;

public:
    player_qt();
    virtual ~player_qt();

    QSettings *settings()
    {
        return _settings;
    }

    virtual void open(const player_init_data &init_data);
    virtual void run();
    virtual void close();
};

#endif
