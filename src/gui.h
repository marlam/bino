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

#ifndef GUI_H
#define GUI_H

#include "config.h"

#include <QMainWindow>
#include <QWidget>
#include <QDialog>

#include "dispatch.h"

class QLabel;
class QComboBox;
class QPushButton;
class QDoubleSpinBox;
class QSpinBox;
class QCheckBox;
class QFontComboBox;
class QSettings;
class QGroupBox;
class QSlider;
class QRadioButton;
class QLineEdit;
class QStackedWidget;
class QTimer;

class video_container_widget;

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

class fullscreen_dialog : public QDialog, public controller
{
    Q_OBJECT

private:
    QRadioButton* _single_btn;
    QComboBox* _single_box;
    QRadioButton* _dual_btn;
    QComboBox* _dual_box0;
    QComboBox* _dual_box1;
    QRadioButton* _multi_btn;
    QLineEdit* _multi_edt;
    QCheckBox* _flip_left_box;
    QCheckBox* _flop_left_box;
    QCheckBox* _flip_right_box;
    QCheckBox* _flop_right_box;
    QCheckBox* _3d_ready_sync_box;
    QCheckBox* _inhibit_screensaver_box;

public:
    fullscreen_dialog(QWidget* parent);
    void closeEvent(QCloseEvent* e);
};

class color_dialog : public QDialog, public controller
{
    Q_OBJECT

private:
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
    color_dialog(QWidget *parent);

    virtual void receive_notification(const notification &note);
};

class crosstalk_dialog : public QDialog, public controller
{
    Q_OBJECT

private:
    bool _lock;
    QDoubleSpinBox *_r_spinbox;
    QDoubleSpinBox *_g_spinbox;
    QDoubleSpinBox *_b_spinbox;

private slots:
    void spinbox_changed();

public:
    crosstalk_dialog(QWidget *parent);

    virtual void receive_notification(const notification &note);
};

class quality_dialog : public QDialog, public controller
{
    Q_OBJECT

private:
    bool _lock;
    QSpinBox *_q_spinbox;
    QSlider *_q_slider;

private slots:
    void q_slider_changed(int val);
    void q_spinbox_changed(int val);

public:
    quality_dialog(QWidget *parent);

    virtual void receive_notification(const notification &note);
};

class zoom_dialog : public QDialog, public controller
{
    Q_OBJECT

private:
    bool _lock;
    QDoubleSpinBox *_z_spinbox;
    QSlider *_z_slider;

private slots:
    void z_slider_changed(int val);
    void z_spinbox_changed(double val);

public:
    zoom_dialog(QWidget *parent);

    virtual void receive_notification(const notification &note);
};

class audio_dialog : public QDialog, public controller
{
    Q_OBJECT

private:
    bool _lock;
    QComboBox *_device_combobox;
    QSpinBox *_delay_spinbox;

private slots:
    void device_changed();
    void delay_changed();

public:
    audio_dialog(QWidget *parent);

    virtual void receive_notification(const notification &note);
};

class subtitle_dialog: public QDialog, public controller
{
    Q_OBJECT

private:
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
    QCheckBox *_shadow_checkbox;
    QComboBox *_shadow_combobox;
    QColor _color;

    QList<QTextCodec *> find_codecs();
    void set_color_button(uint32_t c);

private slots:
    void color_button_clicked();
    void encoding_changed();
    void font_changed();
    void size_changed();
    void scale_changed();
    void color_changed();
    void shadow_changed();

public:
    subtitle_dialog(QWidget *parent);

    virtual void receive_notification(const notification &note);
};

class video_dialog : public QDialog, public controller
{
    Q_OBJECT

private:
    bool _lock;
    QComboBox *_crop_ar_combobox;
    QComboBox *_source_ar_combobox;
    QDoubleSpinBox *_p_spinbox;
    QSlider *_p_slider;
    QDoubleSpinBox *_sp_spinbox;
    QSlider *_sp_slider;
    QDoubleSpinBox *_g_spinbox;
    QSlider *_g_slider;

    void set_crop_ar(float val);
    void set_source_ar(float val);

private slots:
    void crop_ar_changed();
    void source_ar_changed();
    void p_slider_changed(int val);
    void p_spinbox_changed(double val);
    void sp_slider_changed(int val);
    void sp_spinbox_changed(double val);
    void g_slider_changed(int val);
    void g_spinbox_changed(double val);

public:
    video_dialog(QWidget *parent);
    void update();

    virtual void receive_notification(const notification &note);
};

class sdi_output_dialog : public QDialog, public controller
{
    Q_OBJECT

private:
    bool _lock;
    QComboBox *_sdi_output_format_combobox;
    QComboBox *_sdi_output_left_stereo_mode_combobox;
    QComboBox *_sdi_output_right_stereo_mode_combobox;

    void set_sdi_output_format(int val);
    void set_sdi_output_left_stereo_mode(parameters::stereo_mode_t stereo_mode);
    void set_sdi_output_right_stereo_mode(parameters::stereo_mode_t stereo_mode);

private slots:
    void sdi_output_format_changed(int val);
    void sdi_output_left_stereo_mode_changed(int val);
    void sdi_output_right_stereo_mode_changed(int val);

public:
    sdi_output_dialog(QWidget *parent);
    void update();

    virtual void receive_notification(const notification &note);
};

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

class main_window : public QMainWindow, public controller
{
    Q_OBJECT

private:
    QSettings *_settings;
    video_container_widget *_video_container_widget;
    in_out_widget *_in_out_widget;
    controls_widget *_controls_widget;
    fullscreen_dialog *_fullscreen_dialog;
    color_dialog *_color_dialog;
    crosstalk_dialog *_crosstalk_dialog;
    quality_dialog *_quality_dialog;
    zoom_dialog *_zoom_dialog;
    audio_dialog *_audio_dialog;
    subtitle_dialog *_subtitle_dialog;
    video_dialog *_video_dialog;
#if HAVE_LIBXNVCTRL
    sdi_output_dialog *_sdi_output_dialog;
#endif // HAVE_LIBXNVCTRL
    QTimer *_timer;
    std::vector<std::string> _now_playing;

    int _max_recent_files;
    QList<QAction *> _recent_file_actions;
    QAction *_recent_files_separator;
    QAction *_clear_recent_separator;
    QAction *_clear_recent_files_act;

    void update_recent_file_actions();
    QString stripped_name(const QStringList & filenames);

private slots:
    void playloop_step();
    void file_open();
    void file_open_urls();
    void file_open_device();
    void preferences_fullscreen();
    void preferences_colors();
    void preferences_crosstalk();
    void preferences_quality();
    void preferences_zoom();
    void preferences_audio();
    void preferences_subtitle();
    void preferences_video();
    void preferences_sdi_output();
    void help_manual();
    void help_website();
    void help_keyboard();
    void help_about();

    void open_recent_file();
    void clear_recent_files();

protected:
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);
    void moveEvent(QMoveEvent* event);
    void closeEvent(QCloseEvent *event);
    bool eventFilter(QObject *obj, QEvent *event);

public:
    main_window(QSettings *settings);
    virtual ~main_window();

    void open(QStringList urls, const device_request &dev_request = device_request(),
            const parameters& initial_params = parameters());
    video_container_widget* container_widget()
    {
        return _video_container_widget;
    }

    virtual void receive_notification(const notification &note);
};

class gui
{
private:
    main_window *_main_window;
    QSettings *_settings;

public:
    gui();
    ~gui();

    void open(const open_input_data& input_data);

    class video_container_widget* container_widget()
    {
        return _main_window->container_widget();
    }
};

#endif
