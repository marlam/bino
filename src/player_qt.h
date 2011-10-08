/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010-2011
 * Martin Lambers <marlam@marlam.de>
 * Frédéric Devernay <Frederic.Devernay@inrialpes.fr>
 * Joe <cuchac@email.cz>
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
#include <QCheckBox>
#include <QFontComboBox>
#include <QTimer>
#include <QSettings>
#include <QDialog>
#include <QGroupBox>
#include <QStackedWidget>

#include "controller.h"
#include "video_output_qt.h"
#include "player.h"


class player_qt_internal : public player, public controller
{
private:
    bool _playing;
    video_output_qt *_video_output;

protected:
    virtual video_output *create_video_output();
    virtual void destroy_video_output(video_output *vo);

public:
    player_qt_internal(video_output_qt *video_output);
    virtual ~player_qt_internal();

    virtual void receive_cmd(const command &cmd);

    virtual void receive_notification(const notification &note);

    const video_output_qt *get_video_output() const;
    video_output_qt *get_video_output();
    bool is_playing() const;
    bool playloop_step();
    void force_stop();
    void move_event();
};

class in_out_widget : public QWidget, public controller
{
    Q_OBJECT

private:
    QSettings *_settings;
    const player_qt_internal *_player;
    QComboBox *_video_combobox;
    QComboBox *_audio_combobox;
    QComboBox *_subtitle_combobox;
    QComboBox *_input_combobox;
    QComboBox *_output_combobox;
    QCheckBox *_swap_checkbox;
    bool _lock;

    void set_stereo_layout(video_frame::stereo_layout_t stereo_layout, bool stereo_layout_swap);
    void set_stereo_mode(parameters::stereo_mode_t stereo_mode, bool stereo_mode_swap);

private slots:
    void video_changed();
    void audio_changed();
    void subtitle_changed();
    void input_changed();
    void output_changed();
    void swap_changed();

public:
    in_out_widget(QSettings *settings, const player_qt_internal *player, QWidget *parent);
    virtual ~in_out_widget();

    void update(const player_init_data &init_data, bool have_valid_input, bool playing);

    int get_video_stream();
    int get_audio_stream();
    int get_subtitle_stream();
    void get_stereo_layout(video_frame::stereo_layout_t &stereo_layout, bool &stereo_layout_swap);
    void get_stereo_mode(parameters::stereo_mode_t &stereo_mode, bool &stereo_mode_swap);

    virtual void receive_notification(const notification &note);
};

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
    bool _playing;

private slots:
    void play_pressed();
    void pause_pressed();
    void stop_pressed();
    void loop_pressed();
    void fullscreen_pressed();
    void center_pressed();
    void bbb_pressed();
    void bb_pressed();
    void b_pressed();
    void f_pressed();
    void ff_pressed();
    void fff_pressed();
    void seek_slider_changed();

public:
    controls_widget(QSettings *settings, const player_init_data &init_data, QWidget *parent);
    virtual ~controls_widget();

    void update(const player_init_data &init_data, bool have_valid_input, bool playing, int64_t input_duration);
    virtual void receive_notification(const notification &note);
};

class color_dialog : public QDialog, public controller
{
    Q_OBJECT

private:
    parameters *_params;
    bool _lock;
    QDoubleSpinBox *_c_spinbox;
    QSlider *_c_slider;
    QDoubleSpinBox *_b_spinbox;
    QSlider *_b_slider;
    QDoubleSpinBox *_h_spinbox;
    QSlider *_h_slider;
    QDoubleSpinBox *_s_spinbox;
    QSlider *_s_slider;

private slots:
    void c_slider_changed(int val);
    void c_spinbox_changed(double val);
    void b_slider_changed(int val);
    void b_spinbox_changed(double val);
    void h_slider_changed(int val);
    void h_spinbox_changed(double val);
    void s_slider_changed(int val);
    void s_spinbox_changed(double val);

public:
    color_dialog(parameters *params, QWidget *parent);

    virtual void receive_notification(const notification &note);
};

class crosstalk_dialog : public QDialog, public controller
{
    Q_OBJECT

private:
    parameters *_params;
    bool _lock;
    QDoubleSpinBox *_r_spinbox;
    QDoubleSpinBox *_g_spinbox;
    QDoubleSpinBox *_b_spinbox;

private slots:
    void spinbox_changed();

public:
    crosstalk_dialog(parameters *params, QWidget *parent);

    virtual void receive_notification(const notification &note);
};

class subtitle_dialog: public QDialog, public controller
{
    Q_OBJECT

private:
    parameters *_params;
    bool _lock;
    QCheckBox *_encoding_checkbox;
    QComboBox *_encoding_combobox;
    QCheckBox *_font_checkbox;
    QFontComboBox *_font_combobox;
    QCheckBox *_size_checkbox;
    QSpinBox *_size_spinbox;
    QCheckBox *_scale_checkbox;
    QDoubleSpinBox *_scale_spinbox;
    QCheckBox *_color_checkbox;
    QPushButton *_color_button;
    QColor _color;

    QList<QTextCodec *> find_codecs();
    void set_color_button(uint32_t c);

private slots:
    void color_button_pressed();
    void encoding_changed();
    void font_changed();
    void size_changed();
    void scale_changed();
    void color_changed();

public:
    subtitle_dialog(parameters *params, QWidget *parent);

    virtual void receive_notification(const notification &note);
};

class stereoscopic_dialog : public QDialog, public controller
{
    Q_OBJECT

private:
    parameters *_params;
    bool _lock;
    QDoubleSpinBox *_p_spinbox;
    QSlider *_p_slider;
    QDoubleSpinBox *_sp_spinbox;
    QSlider *_sp_slider;
    QDoubleSpinBox *_g_spinbox;
    QSlider *_g_slider;

private slots:
    void p_slider_changed(int val);
    void p_spinbox_changed(double val);
    void sp_slider_changed(int val);
    void sp_spinbox_changed(double val);
    void g_slider_changed(int val);
    void g_spinbox_changed(double val);

public:
    stereoscopic_dialog(parameters *params, QWidget *parent);

    virtual void receive_notification(const notification &note);
};

class open_device_dialog : public QDialog
{
    Q_OBJECT

private:
    QComboBox *_type_combobox;
    QStackedWidget *_device_chooser_stack;
    QComboBox *_default_device_combobox;
    QComboBox *_firewire_device_combobox;
    QLineEdit *_x11_device_field;
    QGroupBox *_frame_size_groupbox;
    QSpinBox *_frame_width_spinbox;
    QSpinBox *_frame_height_spinbox;
    QGroupBox *_frame_rate_groupbox;
    QSpinBox *_frame_rate_num_spinbox;
    QSpinBox *_frame_rate_den_spinbox;

private slots:
    void frame_size_groupbox_clicked(bool checked);
    void frame_rate_groupbox_clicked(bool checked);

public:
    open_device_dialog(const QStringList &default_devices, const QStringList &firewire_devices,
            const QString &x11_device, const device_request &dev_request, QWidget *parent);
    void request(QString &device, device_request &dev_request);
};

class main_window : public QMainWindow, public controller
{
    Q_OBJECT

private:
    QSettings *_settings;
    video_container_widget *_video_container_widget;
    video_output_qt *_video_output;
    in_out_widget *_in_out_widget;
    controls_widget *_controls_widget;
    color_dialog *_color_dialog;
    crosstalk_dialog *_crosstalk_dialog;
    subtitle_dialog *_subtitle_dialog;
    stereoscopic_dialog *_stereoscopic_dialog;
    player_qt_internal *_player;
    QTimer *_timer;
    player_init_data _init_data;
    const player_init_data _init_data_template;
    bool _stop_request;

    QString current_file_hash();
    bool open_player();
    void open(QStringList urls, const device_request &dev_request = device_request());

private slots:
    void move_event();
    void playloop_step();
    void file_open();
    void file_open_url();
    void file_open_device();
    void preferences_colors();
    void preferences_crosstalk();
    void preferences_subtitle();
    void preferences_stereoscopic();
    void preferences_fullscreen();
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
