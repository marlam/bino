/*
 * This file is part of bino, a program to play stereoscopic videos.
 *
 * Copyright (C) 2010  Martin Lambers <marlam@marlam.de>
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
#include <QGridLayout>
#include <QTimer>
#include <QSettings>

#include "input.h"
#include "controller.h"
#include "video_output_opengl_qt.h"
#include "player.h"


class player_qt_internal : public player, public controller
{
private:
    video_output_opengl_qt *_vo;
    bool _playing;

    int64_t _seek_to;
    bool _prep_frame;
    bool _drop_frame;
    bool _display_frame;

public:
    player_qt_internal(video_output_opengl_qt *vo);
    ~player_qt_internal();

    virtual void open(const player_init_data &init_data);
    virtual void close();
    virtual void receive_cmd(const command &cmd);

    virtual void receive_notification(const notification &note);

    bool playloop_step();
    void force_stop();

    QWidget *video_output_widget();
};

class in_out_widget : public QWidget, public controller
{
    Q_OBJECT

private:
    QSettings *_settings;
    QComboBox *_input_combobox;
    QComboBox *_output_combobox;
    QPushButton *_swap_eyes_button;
    QPushButton *_fullscreen_button;
    QPushButton *_center_button;
    bool _lock;

    void set_input(enum input::mode m);
    void set_output(enum video_output::mode m);

private slots:
    void input_changed();
    void swap_eyes_changed();
    void fullscreen_pressed();
    void center_pressed();

public:
    in_out_widget(QSettings *settings, QWidget *parent);
    ~in_out_widget();

    void update(const player_init_data &init_data, bool have_valid_input, bool playing);

    enum input::mode input_mode();
    enum video_output::mode video_mode();

    virtual void receive_notification(const notification &note);
};

class controls_widget : public QWidget, public controller
{
    Q_OBJECT

private:
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

public:
    controls_widget(QSettings *settings, QWidget *parent);
    ~controls_widget();

    void update(const player_init_data &init_data, bool have_valid_input, bool playing);
    virtual void receive_notification(const notification &note);
};

class main_window : public QMainWindow, public controller
{
    Q_OBJECT

private:
    QSettings *_settings;
    player_qt_internal *_player;
    video_output_opengl_qt *_video_output;
    video_output_opengl_qt_widget *_video_widget;
    in_out_widget *_in_out_widget;
    controls_widget *_controls_widget;
    QGridLayout *_layout;
    QTimer *_timer;
    player_init_data _init_data;
    bool _stop_request;

    bool open_player();
    void open(QStringList filenames, bool automatic);

private slots:
    void playloop_step();
    void file_open();
    void file_open_url();
    void help_keyboard();
    void help_about();

protected:
    void moveEvent(QMoveEvent *event);
    void closeEvent(QCloseEvent *event);

public:
    main_window(QSettings *settings, const player_init_data &init_data);
    ~main_window();

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
    ~player_qt();

    QSettings *settings()
    {
        return _settings;
    }

    virtual void open(const player_init_data &init_data);
    virtual void run();
    virtual void close();
};

#endif
