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

#include "config.h"

#include <QCoreApplication>
#include <QApplication>
#include <QMainWindow>
#include <QGridLayout>
#include <QCloseEvent>
#include <QMenu>
#include <QMenuBar>
#include <QAction>
#include <QGridLayout>
#include <QLineEdit>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QComboBox>
#include <QTimer>
#include <QFile>
#include <QByteArray>
#include <QCryptographicHash>
#include <QUrl>
#include <QDesktopServices>
#include <QDoubleSpinBox>
#include <QSpinBox>

#include "player_qt.h"
#include "qt_app.h"
#include "video_output_qt.h"
#include "lib_versions.h"

#include "dbg.h"
#include "msg.h"


player_qt_internal::player_qt_internal(video_container_widget *widget) :
    player(player::master), _playing(false), _container_widget(widget), _video_output(NULL)
{
}

player_qt_internal::~player_qt_internal()
{
}

video_output *player_qt_internal::create_video_output()
{
    _video_output = new video_output_qt(_container_widget);
    return _video_output;
}

void player_qt_internal::receive_cmd(const command &cmd)
{
    if (cmd.type == command::toggle_play && !_playing)
    {
        notify(notification::play, false, true);
    }
    else if (_playing)
    {
        player::receive_cmd(cmd);
    }
}

void player_qt_internal::receive_notification(const notification &note)
{
    if (note.type == notification::play)
    {
        std::istringstream iss(note.current);
        s11n::load(iss, _playing);
    }
}

const video_output_qt *player_qt_internal::get_video_output() const
{
    return _video_output;
}

bool player_qt_internal::playloop_step()
{
    return run_step();
}

void player_qt_internal::force_stop()
{
    notify(notification::play, false, false);
}

void player_qt_internal::move_event()
{
    if (_video_output)
    {
        _video_output->move_event();
    }
}


in_out_widget::in_out_widget(QSettings *settings, const player_qt_internal *player, QWidget *parent) :
    QWidget(parent), _settings(settings), _player(player), _lock(false)
{
    QGridLayout *layout0 = new QGridLayout;
    QLabel *video_label = new QLabel("Video:");
    video_label->setToolTip(
            "<p>Select the video stream.</p>");
    layout0->addWidget(video_label, 0, 0);
    _video_combobox = new QComboBox(this);
    _video_combobox->setToolTip(video_label->toolTip());
    connect(_video_combobox, SIGNAL(currentIndexChanged(int)), this, SLOT(video_changed()));
    layout0->addWidget(_video_combobox, 0, 1);
    QLabel *audio_label = new QLabel("Audio:");
    audio_label->setToolTip(
            "<p>Select the audio stream.</p>");
    layout0->addWidget(audio_label, 0, 2);
    _audio_combobox = new QComboBox(this);
    _audio_combobox->setToolTip(audio_label->toolTip());
    connect(_audio_combobox, SIGNAL(currentIndexChanged(int)), this, SLOT(audio_changed()));
    layout0->addWidget(_audio_combobox, 0, 3);
    layout0->setColumnStretch(1, 1);
    layout0->setColumnStretch(3, 1);

    QGridLayout *layout1 = new QGridLayout;
    QLabel *input_label = new QLabel("Input:");
    input_label->setToolTip(
            "<p>Set the 3D layout of the video stream.</p>");
    layout1->addWidget(input_label, 0, 0);
    _input_combobox = new QComboBox(this);
    _input_combobox->setToolTip(input_label->toolTip());
    _input_combobox->addItem("2D");
    _input_combobox->addItem("Separate streams, left first");
    _input_combobox->addItem("Separate streams, right first");
    _input_combobox->addItem("Top/bottom");
    _input_combobox->addItem("Top/bottom, half height");
    _input_combobox->addItem("Bottom/top");
    _input_combobox->addItem("Bottom/top, half height");
    _input_combobox->addItem("Left/right");
    _input_combobox->addItem("Left/right, half width");
    _input_combobox->addItem("Right/left");
    _input_combobox->addItem("Right/left, half width");
    _input_combobox->addItem("Even/odd rows");
    _input_combobox->addItem("Odd/even rows");
    connect(_input_combobox, SIGNAL(currentIndexChanged(int)), this, SLOT(input_changed()));
    layout1->addWidget(_input_combobox, 0, 1);
    layout1->setColumnStretch(1, 1);

    QGridLayout *layout2 = new QGridLayout;
    QLabel *output_label = new QLabel("Output:");
    output_label->setToolTip(
            "<p>Set the 3D output type for your display.</p>");
    layout2->addWidget(output_label, 0, 0);
    _output_combobox = new QComboBox(this);
    _output_combobox->setToolTip(output_label->toolTip());
    _output_combobox->addItem("Left view");
    _output_combobox->addItem("Right view");
    _output_combobox->addItem("Top/bottom");
    _output_combobox->addItem("Top/bottom, half height");
    _output_combobox->addItem("Left/right");
    _output_combobox->addItem("Left/right, half width");
    _output_combobox->addItem("Even/odd rows");
    _output_combobox->addItem("Even/odd columns");
    _output_combobox->addItem("Checkerboard pattern");
    _output_combobox->addItem("Red/cyan glasses, Dubois method");
    _output_combobox->addItem("Red/cyan glasses, monochrome method");
    _output_combobox->addItem("Red/cyan glasses, full-color method");
    _output_combobox->addItem("Red/cyan glasses, half-color method");
    _output_combobox->addItem("OpenGL stereo");
    connect(_output_combobox, SIGNAL(currentIndexChanged(int)), this, SLOT(output_changed()));
    layout2->addWidget(_output_combobox, 0, 1);
    layout2->setColumnStretch(1, 1);
    _swap_checkbox = new QCheckBox("Swap left/right");
    _swap_checkbox->setToolTip(
            "<p>Swap the left and right view. "
            "Use this if the 3D effect seems wrong.</p>");
    connect(_swap_checkbox, SIGNAL(stateChanged(int)), this, SLOT(swap_changed()));
    layout2->addWidget(_swap_checkbox, 0, 2);

    QGridLayout *layout = new QGridLayout;
    layout->addLayout(layout0, 0, 0);
    layout->addLayout(layout1, 1, 0);
    layout->addLayout(layout2, 2, 0);
    setLayout(layout);

    // Align the input and output labels
    output_label->setMinimumSize(output_label->minimumSizeHint());
    input_label->setMinimumSize(output_label->minimumSizeHint());
    audio_label->setMinimumSize(output_label->minimumSizeHint());
    video_label->setMinimumSize(output_label->minimumSizeHint());

    _video_combobox->setEnabled(false);
    _audio_combobox->setEnabled(false);
    _input_combobox->setEnabled(false);
    _output_combobox->setEnabled(false);
    _swap_checkbox->setEnabled(false);
}

in_out_widget::~in_out_widget()
{
}

void in_out_widget::set_stereo_layout(video_frame::stereo_layout_t stereo_layout, bool stereo_layout_swap)
{
    switch (stereo_layout)
    {
    case video_frame::mono:
        _input_combobox->setCurrentIndex(0);
        break;
    case video_frame::separate:
        _input_combobox->setCurrentIndex(stereo_layout_swap ? 2 : 1);
        break;
    case video_frame::top_bottom:
        _input_combobox->setCurrentIndex(stereo_layout_swap ? 5 : 3);
        break;
    case video_frame::top_bottom_half:
        _input_combobox->setCurrentIndex(stereo_layout_swap ? 6 : 4);
        break;
    case video_frame::left_right:
        _input_combobox->setCurrentIndex(stereo_layout_swap ? 9: 7);
        break;
    case video_frame::left_right_half:
        _input_combobox->setCurrentIndex(stereo_layout_swap ? 10 : 8);
        break;
    case video_frame::even_odd_rows:
        _input_combobox->setCurrentIndex(stereo_layout_swap ? 12 : 11);
        break;
    }
}

void in_out_widget::set_stereo_mode(parameters::stereo_mode_t stereo_mode, bool stereo_mode_swap)
{
    switch (stereo_mode)
    {
    default:
    case parameters::mono_left:
        _output_combobox->setCurrentIndex(0);
        break;
    case parameters::mono_right:
        _output_combobox->setCurrentIndex(1);
        break;
    case parameters::top_bottom:
        _output_combobox->setCurrentIndex(2);
        break;
    case parameters::top_bottom_half:
        _output_combobox->setCurrentIndex(3);
        break;
    case parameters::left_right:
        _output_combobox->setCurrentIndex(4);
        break;
    case parameters::left_right_half:
        _output_combobox->setCurrentIndex(5);
        break;
    case parameters::even_odd_rows:
        _output_combobox->setCurrentIndex(6);
        break;
    case parameters::even_odd_columns:
        _output_combobox->setCurrentIndex(7);
        break;
    case parameters::checkerboard:
        _output_combobox->setCurrentIndex(8);
        break;
    case parameters::anaglyph_red_cyan_dubois:
        _output_combobox->setCurrentIndex(9);
        break;
    case parameters::anaglyph_red_cyan_monochrome:
        _output_combobox->setCurrentIndex(10);
        break;
    case parameters::anaglyph_red_cyan_full_color:
        _output_combobox->setCurrentIndex(11);
        break;
    case parameters::anaglyph_red_cyan_half_color:
        _output_combobox->setCurrentIndex(12);
        break;
    case parameters::stereo:
        _output_combobox->setCurrentIndex(13);
        break;
    }
    _swap_checkbox->setChecked(stereo_mode_swap);
}

void in_out_widget::video_changed()
{
    if (!_lock)
    {
        video_frame::stereo_layout_t stereo_layout;
        bool stereo_layout_swap;
        get_stereo_layout(stereo_layout, stereo_layout_swap);
        if (stereo_layout == video_frame::separate)
        {
            QMessageBox::critical(this, "Error",
                    "Video streams cannot be changed in this input mode.");
            _lock = true;
            _video_combobox->setCurrentIndex(0);
            _lock = false;
            return;
        }
        send_cmd(command::set_video_stream, _video_combobox->currentIndex());
    }
}

void in_out_widget::audio_changed()
{
    if (!_lock)
    {
        send_cmd(command::set_audio_stream, _audio_combobox->currentIndex());
    }
}

void in_out_widget::input_changed()
{
    video_frame::stereo_layout_t stereo_layout;
    bool stereo_layout_swap;
    get_stereo_layout(stereo_layout, stereo_layout_swap);
    if (!_player->get_media_input().stereo_layout_is_supported(stereo_layout, stereo_layout_swap))
    {
        QMessageBox::critical(this, "Error",
                "The input data does not support this 3D layout.");
        _input_combobox->setCurrentIndex(0);
        return;
    }
    if (stereo_layout == video_frame::separate)
    {
        _lock = true;
        _video_combobox->setCurrentIndex(0);
        _lock = false;
    }
    std::ostringstream oss;
    s11n::save(oss, static_cast<int>(stereo_layout));
    s11n::save(oss, stereo_layout_swap);
    send_cmd(command::set_stereo_layout, oss.str());
    parameters::stereo_mode_t stereo_mode;
    bool stereo_mode_swap;
    get_stereo_mode(stereo_mode, stereo_mode_swap);
    if (stereo_layout == video_frame::mono
            && !(stereo_mode == parameters::mono_left || stereo_mode == parameters::mono_right))
    {
        QString s = _settings->value("Session/2d-stereo-mode", "").toString();
        parameters::stereo_mode_from_string(s.toStdString(), stereo_mode, stereo_mode_swap);
        set_stereo_mode(stereo_mode, stereo_mode_swap);
    }
    else if (stereo_layout != video_frame::mono
            && (stereo_mode == parameters::mono_left || stereo_mode == parameters::mono_right))
    {
        QString s = _settings->value("Session/3d-stereo-mode", "").toString();
        parameters::stereo_mode_from_string(s.toStdString(), stereo_mode, stereo_mode_swap);
        set_stereo_mode(stereo_mode, stereo_mode_swap);
    }
}

void in_out_widget::output_changed()
{
    parameters::stereo_mode_t stereo_mode;
    bool stereo_mode_swap;
    get_stereo_mode(stereo_mode, stereo_mode_swap);
    if (stereo_mode == parameters::stereo && !_player->get_video_output()->supports_stereo())
    {
        QMessageBox::critical(this, "Error",
                "The display does not support OpenGL stereo mode.");
        _output_combobox->setCurrentIndex(9);
        return;
    }
    std::ostringstream oss;
    s11n::save(oss, static_cast<int>(stereo_mode));
    s11n::save(oss, stereo_mode_swap);
    send_cmd(command::set_stereo_mode, oss.str());
}

void in_out_widget::swap_changed()
{
    if (!_lock)
    {
        send_cmd(command::toggle_stereo_mode_swap);
    }
}

void in_out_widget::update(const player_init_data &init_data, bool have_valid_input, bool playing)
{
    _lock = true;
    if (have_valid_input)
    {
        _video_combobox->clear();
        for (int i = 0; i < _player->get_media_input().video_streams(); i++)
        {
            _video_combobox->addItem(_player->get_media_input().video_stream_name(i).c_str());
        }
        _audio_combobox->clear();
        for (int i = 0; i < _player->get_media_input().audio_streams(); i++)
        {
            _audio_combobox->addItem(_player->get_media_input().audio_stream_name(i).c_str());
        }
    }
    _video_combobox->setCurrentIndex(init_data.video_stream);
    _audio_combobox->setCurrentIndex(init_data.audio_stream);
    set_stereo_layout(init_data.stereo_layout, init_data.stereo_layout_swap);
    set_stereo_mode(init_data.stereo_mode, init_data.stereo_mode_swap);
    _lock = false;
    _video_combobox->setEnabled(have_valid_input);
    _audio_combobox->setEnabled(have_valid_input);
    _input_combobox->setEnabled(have_valid_input);
    _output_combobox->setEnabled(have_valid_input);
    _swap_checkbox->setEnabled(have_valid_input);
    if (have_valid_input)
    {
        receive_notification(notification(notification::play, !playing, playing));
    }
}

int in_out_widget::get_video_stream()
{
    return _video_combobox->currentIndex();
}

int in_out_widget::get_audio_stream()
{
    return _audio_combobox->currentIndex();
}

void in_out_widget::get_stereo_layout(video_frame::stereo_layout_t &stereo_layout, bool &stereo_layout_swap)
{
    switch (_input_combobox->currentIndex())
    {
    case 0:
        stereo_layout = video_frame::mono;
        stereo_layout_swap = false;
        break;
    case 1:
        stereo_layout = video_frame::separate;
        stereo_layout_swap = false;
        break;
    case 2:
        stereo_layout = video_frame::separate;
        stereo_layout_swap = true;
        break;
    case 3:
        stereo_layout = video_frame::top_bottom;
        stereo_layout_swap = false;
        break;
    case 4:
        stereo_layout = video_frame::top_bottom_half;
        stereo_layout_swap = false;
        break;
    case 5:
        stereo_layout = video_frame::top_bottom;
        stereo_layout_swap = true;
        break;
    case 6:
        stereo_layout = video_frame::top_bottom_half;
        stereo_layout_swap = true;
        break;
    case 7:
        stereo_layout = video_frame::left_right;
        stereo_layout_swap = false;
        break;
    case 8:
        stereo_layout = video_frame::left_right_half;
        stereo_layout_swap = false;
        break;
    case 9:
        stereo_layout = video_frame::left_right;
        stereo_layout_swap = true;
        break;
    case 10:
        stereo_layout = video_frame::left_right_half;
        stereo_layout_swap = true;
        break;
    case 11:
        stereo_layout = video_frame::even_odd_rows;
        stereo_layout_swap = false;
        break;
    case 12:
        stereo_layout = video_frame::even_odd_rows;
        stereo_layout_swap = true;
        break;
    }
}

void in_out_widget::get_stereo_mode(parameters::stereo_mode_t &stereo_mode, bool &stereo_mode_swap)
{
    switch (_output_combobox->currentIndex())
    {
    case 0:
        stereo_mode = parameters::mono_left;
        break;
    case 1:
        stereo_mode = parameters::mono_right;
        break;
    case 2:
        stereo_mode = parameters::top_bottom;
        break;
    case 3:
        stereo_mode = parameters::top_bottom_half;
        break;
    case 4:
        stereo_mode = parameters::left_right;
        break;
    case 5:
        stereo_mode = parameters::left_right_half;
        break;
    case 6:
        stereo_mode = parameters::even_odd_rows;
        break;
    case 7:
        stereo_mode = parameters::even_odd_columns;
        break;
    case 8:
        stereo_mode = parameters::checkerboard;
        break;
    case 9:
        stereo_mode = parameters::anaglyph_red_cyan_dubois;
        break;
    case 10:
        stereo_mode = parameters::anaglyph_red_cyan_monochrome;
        break;
    case 11:
        stereo_mode = parameters::anaglyph_red_cyan_full_color;
        break;
    case 12:
        stereo_mode = parameters::anaglyph_red_cyan_half_color;
        break;
    case 13:
        stereo_mode = parameters::stereo;
        break;
    }
    stereo_mode_swap = _swap_checkbox->isChecked();
}

void in_out_widget::receive_notification(const notification &note)
{
    std::istringstream current(note.current);
    int stream;
    bool flag;

    switch (note.type)
    {
    case notification::video_stream:
        s11n::load(current, stream);
        _lock = true;
        _video_combobox->setCurrentIndex(stream);
        _lock = false;
        break;
    case notification::audio_stream:
        s11n::load(current, stream);
        _lock = true;
        _audio_combobox->setCurrentIndex(stream);
        _lock = false;
        break;
    case notification::stereo_mode_swap:
        s11n::load(current, flag);
        _lock = true;
        _swap_checkbox->setChecked(flag);
        _lock = false;
        break;
    default:
        break;
    }
}


controls_widget::controls_widget(QSettings *settings, QWidget *parent)
    : QWidget(parent), _lock(false), _settings(settings), _playing(false)
{
    QGridLayout *layout = new QGridLayout;
    _seek_slider = new QSlider(Qt::Horizontal);
    _seek_slider->setToolTip(
            "<p>This slider shows the progress during video playback, "
            "and can be used to seek in the video.</p>");
    _seek_slider->setRange(0, 2000);
    _seek_slider->setTracking(false);
    connect(_seek_slider, SIGNAL(valueChanged(int)), this, SLOT(seek_slider_changed()));
    layout->addWidget(_seek_slider, 0, 0, 1, 13);
    _play_button = new QPushButton(QIcon(":icons/play.png"), "");
    _play_button->setToolTip("<p>Play.</p>");
    connect(_play_button, SIGNAL(pressed()), this, SLOT(play_pressed()));
    layout->addWidget(_play_button, 1, 0);
    _pause_button = new QPushButton(QIcon(":icons/pause.png"), "");
    _pause_button->setToolTip("<p>Pause.</p>");
    connect(_pause_button, SIGNAL(pressed()), this, SLOT(pause_pressed()));
    layout->addWidget(_pause_button, 1, 1);
    _stop_button = new QPushButton(QIcon(":icons/stop.png"), "");
    _stop_button->setToolTip("<p>Stop.</p>");
    connect(_stop_button, SIGNAL(pressed()), this, SLOT(stop_pressed()));
    layout->addWidget(_stop_button, 1, 2);
    layout->addWidget(new QWidget, 1, 3);
    _fullscreen_button = new QPushButton(QIcon(":icons/fullscreen.png"), "");
    _fullscreen_button->setToolTip(
            "<p>Switch to fullscreen mode. "
            "You can leave fullscreen mode by pressing the f key.</p>");
    connect(_fullscreen_button, SIGNAL(pressed()), this, SLOT(fullscreen_pressed()));
    layout->addWidget(_fullscreen_button, 1, 4);
    _center_button = new QPushButton(QIcon(":icons/center.png"), "");
    _center_button->setToolTip(
            "<p>Center the video area on your screen.</p>");
    connect(_center_button, SIGNAL(pressed()), this, SLOT(center_pressed()));
    layout->addWidget(_center_button, 1, 5);
    layout->addWidget(new QWidget, 1, 6);
    _bbb_button = new QPushButton(QIcon(":icons/bbb.png"), "");
    _bbb_button->setToolTip("<p>Seek backward 10 minutes.</p>");
    connect(_bbb_button, SIGNAL(pressed()), this, SLOT(bbb_pressed()));
    layout->addWidget(_bbb_button, 1, 7);
    _bb_button = new QPushButton(QIcon(":icons/bb.png"), "");
    _bb_button->setToolTip("<p>Seek backward 1 minute.</p>");
    connect(_bb_button, SIGNAL(pressed()), this, SLOT(bb_pressed()));
    layout->addWidget(_bb_button, 1, 8);
    _b_button = new QPushButton(QIcon(":icons/b.png"), "");
    _b_button->setToolTip("<p>Seek backward 10 seconds.</p>");
    connect(_b_button, SIGNAL(pressed()), this, SLOT(b_pressed()));
    layout->addWidget(_b_button, 1, 9);
    _f_button = new QPushButton(QIcon(":icons/f.png"), "");
    _f_button->setToolTip("<p>Seek forward 10 seconds.</p>");
    connect(_f_button, SIGNAL(pressed()), this, SLOT(f_pressed()));
    layout->addWidget(_f_button, 1, 10);
    _ff_button = new QPushButton(QIcon(":icons/ff.png"), "");
    _ff_button->setToolTip("<p>Seek forward 1 minute.</p>");
    connect(_ff_button, SIGNAL(pressed()), this, SLOT(ff_pressed()));
    layout->addWidget(_ff_button, 1, 11);
    _fff_button = new QPushButton(QIcon(":icons/fff.png"), "");
    _fff_button->setToolTip("<p>Seek forward 10 minutes.</p>");
    connect(_fff_button, SIGNAL(pressed()), this, SLOT(fff_pressed()));
    layout->addWidget(_fff_button, 1, 12);
    layout->setRowStretch(0, 0);
    layout->setColumnStretch(3, 1);
    layout->setColumnStretch(6, 1);
    setLayout(layout);

    _play_button->setEnabled(false);
    _pause_button->setEnabled(false);
    _stop_button->setEnabled(false);
    _fullscreen_button->setEnabled(false);
    _center_button->setEnabled(false);
    _bbb_button->setEnabled(false);
    _bb_button->setEnabled(false);
    _b_button->setEnabled(false);
    _f_button->setEnabled(false);
    _ff_button->setEnabled(false);
    _fff_button->setEnabled(false);
    _seek_slider->setEnabled(false);
}

controls_widget::~controls_widget()
{
}

void controls_widget::play_pressed()
{
    if (_playing)
    {
        send_cmd(command::toggle_pause);
    }
    else
    {
        send_cmd(command::toggle_play);
    }
}

void controls_widget::pause_pressed()
{
    send_cmd(command::toggle_pause);
}

void controls_widget::stop_pressed()
{
    send_cmd(command::toggle_play);
}

void controls_widget::fullscreen_pressed()
{
    send_cmd(command::toggle_fullscreen);
}

void controls_widget::center_pressed()
{
    send_cmd(command::center);
}

void controls_widget::bbb_pressed()
{
    send_cmd(command::seek, -600.0f);
}

void controls_widget::bb_pressed()
{
    send_cmd(command::seek, -60.0f);
}

void controls_widget::b_pressed()
{
    send_cmd(command::seek, -10.0f);
}

void controls_widget::f_pressed()
{
    send_cmd(command::seek, +10.0f);
}

void controls_widget::ff_pressed()
{
    send_cmd(command::seek, +60.0f);
}

void controls_widget::fff_pressed()
{
    send_cmd(command::seek, +600.0f);
}

void controls_widget::seek_slider_changed()
{
    if (!_lock)
    {
        send_cmd(command::set_pos, static_cast<float>(_seek_slider->value()) / 2000.0f);
    }
}

void controls_widget::update(const player_init_data &, bool have_valid_input, bool playing)
{
    if (have_valid_input)
    {
        receive_notification(notification(notification::play, !playing, playing));
    }
    else
    {
        _playing = false;
        _play_button->setEnabled(false);
        _pause_button->setEnabled(false);
        _stop_button->setEnabled(false);
        _fullscreen_button->setEnabled(false);
        _center_button->setEnabled(false);
        _bbb_button->setEnabled(false);
        _bb_button->setEnabled(false);
        _b_button->setEnabled(false);
        _f_button->setEnabled(false);
        _ff_button->setEnabled(false);
        _fff_button->setEnabled(false);
        _seek_slider->setEnabled(false);
        _seek_slider->setValue(0);
    }
}

void controls_widget::receive_notification(const notification &note)
{
    std::istringstream current(note.current);
    bool flag;
    float value;

    switch (note.type)
    {
    case notification::play:
        s11n::load(current, flag);
        _playing = flag;
        _play_button->setEnabled(!flag);
        _pause_button->setEnabled(flag);
        _stop_button->setEnabled(flag);
        _fullscreen_button->setEnabled(flag);
        _center_button->setEnabled(flag);
        _bbb_button->setEnabled(flag);
        _bb_button->setEnabled(flag);
        _b_button->setEnabled(flag);
        _f_button->setEnabled(flag);
        _ff_button->setEnabled(flag);
        _fff_button->setEnabled(flag);
        _seek_slider->setEnabled(flag);
        if (!flag)
        {
            _seek_slider->setValue(0);
        }
        break;
    case notification::pause:
        s11n::load(current, flag);
        _play_button->setEnabled(flag);
        _pause_button->setEnabled(!flag);
        break;
    case notification::pos:
        if (!_seek_slider->isSliderDown())
        {
            _lock = true;
            s11n::load(current, value);
            _seek_slider->setValue(qRound(value * 2000.0f));
            _lock = false;
        }
    default:
        break;
    }
}


color_dialog::color_dialog(const parameters &params, QWidget *parent) : QDialog(parent),
    _lock(false)
{
    setModal(false);
    setWindowTitle("Display Color Adjustments");

    QLabel *c_label = new QLabel("Contrast:");
    _c_slider = new QSlider(Qt::Horizontal);
    _c_slider->setRange(-1000, 1000);
    _c_slider->setValue(params.contrast * 1000.0f);
    connect(_c_slider, SIGNAL(valueChanged(int)), this, SLOT(c_slider_changed(int)));
    _c_spinbox = new QDoubleSpinBox();
    _c_spinbox->setRange(-1.0, +1.0);
    _c_spinbox->setValue(params.contrast);
    _c_spinbox->setDecimals(2);
    _c_spinbox->setSingleStep(0.01);
    connect(_c_spinbox, SIGNAL(valueChanged(double)), this, SLOT(c_spinbox_changed(double)));
    QLabel *b_label = new QLabel("Brightness:");
    _b_slider = new QSlider(Qt::Horizontal);
    _b_slider->setRange(-1000, 1000);
    _b_slider->setValue(params.brightness * 1000.0f);
    connect(_b_slider, SIGNAL(valueChanged(int)), this, SLOT(b_slider_changed(int)));
    _b_spinbox = new QDoubleSpinBox();
    _b_spinbox->setRange(-1.0, +1.0);
    _b_spinbox->setValue(params.brightness);
    _b_spinbox->setDecimals(2);
    _b_spinbox->setSingleStep(0.01);
    connect(_b_spinbox, SIGNAL(valueChanged(double)), this, SLOT(b_spinbox_changed(double)));
    QLabel *h_label = new QLabel("Hue:");
    _h_slider = new QSlider(Qt::Horizontal);
    _h_slider->setRange(-1000, 1000);
    _h_slider->setValue(params.hue * 1000.0f);
    connect(_h_slider, SIGNAL(valueChanged(int)), this, SLOT(h_slider_changed(int)));
    _h_spinbox = new QDoubleSpinBox();
    _h_spinbox->setRange(-1.0, +1.0);
    _h_spinbox->setValue(params.hue);
    _h_spinbox->setDecimals(2);
    _h_spinbox->setSingleStep(0.01);
    connect(_h_spinbox, SIGNAL(valueChanged(double)), this, SLOT(h_spinbox_changed(double)));
    QLabel *s_label = new QLabel("Saturation:");
    _s_slider = new QSlider(Qt::Horizontal);
    _s_slider->setRange(-1000, 1000);
    _s_slider->setValue(params.saturation * 1000.0f);
    connect(_s_slider, SIGNAL(valueChanged(int)), this, SLOT(s_slider_changed(int)));
    _s_spinbox = new QDoubleSpinBox();
    _s_spinbox->setRange(-1.0, +1.0);
    _s_spinbox->setValue(params.saturation);
    _s_spinbox->setDecimals(2);
    _s_spinbox->setSingleStep(0.01);
    connect(_s_spinbox, SIGNAL(valueChanged(double)), this, SLOT(s_spinbox_changed(double)));

    QGridLayout *layout = new QGridLayout;
    layout->addWidget(c_label, 0, 0);
    layout->addWidget(_c_slider, 0, 1);
    layout->addWidget(_c_spinbox, 0, 2);
    layout->addWidget(b_label, 1, 0);
    layout->addWidget(_b_slider, 1, 1);
    layout->addWidget(_b_spinbox, 1, 2);
    layout->addWidget(h_label, 2, 0);
    layout->addWidget(_h_slider, 2, 1);
    layout->addWidget(_h_spinbox, 2, 2);
    layout->addWidget(s_label, 3, 0);
    layout->addWidget(_s_slider, 3, 1);
    layout->addWidget(_s_spinbox, 3, 2);
    setLayout(layout);
}

void color_dialog::c_slider_changed(int val)
{
    if (!_lock)
    {
        send_cmd(command::set_contrast, val / 1000.0f);
    }
}

void color_dialog::c_spinbox_changed(double val)
{
    if (!_lock)
    {
        send_cmd(command::set_contrast, static_cast<float>(val));
    }
}

void color_dialog::b_slider_changed(int val)
{
    if (!_lock)
    {
        send_cmd(command::set_brightness, val / 1000.0f);
    }
}

void color_dialog::b_spinbox_changed(double val)
{
    if (!_lock)
    {
        send_cmd(command::set_brightness, static_cast<float>(val));
    }
}

void color_dialog::h_slider_changed(int val)
{
    if (!_lock)
    {
        send_cmd(command::set_hue, val / 1000.0f);
    }
}

void color_dialog::h_spinbox_changed(double val)
{
    if (!_lock)
    {
        send_cmd(command::set_hue, static_cast<float>(val));
    }
}

void color_dialog::s_slider_changed(int val)
{
    if (!_lock)
    {
        send_cmd(command::set_saturation, val / 1000.0f);
    }
}

void color_dialog::s_spinbox_changed(double val)
{
    if (!_lock)
    {
        send_cmd(command::set_saturation, static_cast<float>(val));
    }
}

void color_dialog::receive_notification(const notification &note)
{
    std::istringstream current(note.current);
    float value;

    switch (note.type)
    {
    case notification::contrast:
        s11n::load(current, value);
        _lock = true;
        _c_slider->setValue(value * 1000.0f);
        _c_spinbox->setValue(value);
        _lock = false;
        break;
    case notification::brightness:
        s11n::load(current, value);
        _lock = true;
        _b_slider->setValue(value * 1000.0f);
        _b_spinbox->setValue(value);
        _lock = false;
        break;
    case notification::hue:
        s11n::load(current, value);
        _lock = true;
        _h_slider->setValue(value * 1000.0f);
        _h_spinbox->setValue(value);
        _lock = false;
        break;
    case notification::saturation:
        s11n::load(current, value);
        _lock = true;
        _s_slider->setValue(value * 1000.0f);
        _s_spinbox->setValue(value);
        _lock = false;
        break;
    default:
        /* not handled */
        break;
    }
}


crosstalk_dialog::crosstalk_dialog(parameters *params, QWidget *parent) : QDialog(parent),
    _lock(false), _params(params)
{
    setModal(false);
    setWindowTitle("Display Crosstalk Calibration");

    QLabel *rtfm_label = new QLabel(
            "<p>Please read the manual to find out<br>"
            "how to measure the crosstalk levels<br>"
            "of your display.</p>");

    QLabel *r_label = new QLabel("Red:");
    _r_spinbox = new QDoubleSpinBox();
    _r_spinbox->setRange(0.0, +1.0);
    _r_spinbox->setValue(params->crosstalk_r);
    _r_spinbox->setDecimals(2);
    _r_spinbox->setSingleStep(0.01);
    connect(_r_spinbox, SIGNAL(valueChanged(double)), this, SLOT(spinbox_changed()));
    QLabel *g_label = new QLabel("Green:");
    _g_spinbox = new QDoubleSpinBox();
    _g_spinbox->setRange(0.0, +1.0);
    _g_spinbox->setValue(params->crosstalk_g);
    _g_spinbox->setDecimals(2);
    _g_spinbox->setSingleStep(0.01);
    connect(_g_spinbox, SIGNAL(valueChanged(double)), this, SLOT(spinbox_changed()));
    QLabel *b_label = new QLabel("Blue:");
    _b_spinbox = new QDoubleSpinBox();
    _b_spinbox->setRange(0.0, +1.0);
    _b_spinbox->setValue(params->crosstalk_b);
    _b_spinbox->setDecimals(2);
    _b_spinbox->setSingleStep(0.01);
    connect(_b_spinbox, SIGNAL(valueChanged(double)), this, SLOT(spinbox_changed()));

    QGridLayout *layout = new QGridLayout;
    layout->addWidget(rtfm_label, 0, 0, 2, 3);
    layout->addWidget(r_label, 3, 0);
    layout->addWidget(_r_spinbox, 3, 1);
    layout->addWidget(g_label, 4, 0);
    layout->addWidget(_g_spinbox, 4, 1);
    layout->addWidget(b_label, 5, 0);
    layout->addWidget(_b_spinbox, 5, 1);
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
        /* Also set crosstalk levels in init data, because this must also work in
         * the absence of a player that interpretes the above command (i.e. when
         * no video is currently playing */
        _params->crosstalk_r = _r_spinbox->value();
        _params->crosstalk_g = _g_spinbox->value();
        _params->crosstalk_b = _b_spinbox->value();
    }
}

void crosstalk_dialog::receive_notification(const notification &note)
{
    std::istringstream current(note.current);
    float r, g, b;

    switch (note.type)
    {
    case notification::crosstalk:
        s11n::load(current, r);
        s11n::load(current, g);
        s11n::load(current, b);
        _lock = true;
        _r_spinbox->setValue(r);
        _g_spinbox->setValue(g);
        _b_spinbox->setValue(b);
        _lock = false;
        break;
    default:
        /* not handled */
        break;
    }
}


stereoscopic_dialog::stereoscopic_dialog(const parameters &params, QWidget *parent) : QDialog(parent),
    _lock(false)
{
    setModal(false);
    setWindowTitle("Stereoscopic Video Settings");

    QLabel *p_label = new QLabel("Parallax:");
    p_label->setToolTip(
            "<p>Adjust parallax, from -1 to +1. This changes the separation of left and right view, "
            "and thus moves the point where both lines of sight meet.</p>");
    _p_slider = new QSlider(Qt::Horizontal);
    _p_slider->setToolTip(p_label->toolTip());
    _p_slider->setRange(-1000, 1000);
    _p_slider->setValue(params.parallax * 1000.0f);
    connect(_p_slider, SIGNAL(valueChanged(int)), this, SLOT(p_slider_changed(int)));
    _p_spinbox = new QDoubleSpinBox();
    _p_spinbox->setToolTip(p_label->toolTip());
    _p_spinbox->setRange(-1.0, +1.0);
    _p_spinbox->setValue(params.parallax);
    _p_spinbox->setDecimals(2);
    _p_spinbox->setSingleStep(0.01);
    connect(_p_spinbox, SIGNAL(valueChanged(double)), this, SLOT(p_spinbox_changed(double)));
    QLabel *g_label = new QLabel("Ghostbusting:");
    g_label->setToolTip(
            "<p>Set the amount of crosstalk ghostbusting, from 0 to 1. "
            "You need to set the crosstalk levels of your display first. "
            "Note that crosstalk ghostbusting does not work with anaglyph glasses.</p>");
    _g_slider = new QSlider(Qt::Horizontal);
    _g_slider->setToolTip(g_label->toolTip());
    _g_slider->setRange(0, 1000);
    _g_slider->setValue(params.ghostbust * 1000.0f);
    connect(_g_slider, SIGNAL(valueChanged(int)), this, SLOT(g_slider_changed(int)));
    _g_spinbox = new QDoubleSpinBox();
    _g_spinbox->setToolTip(g_label->toolTip());
    _g_spinbox->setRange(0.0, +1.0);
    _g_spinbox->setValue(params.ghostbust);
    _g_spinbox->setDecimals(2);
    _g_spinbox->setSingleStep(0.01);
    connect(_g_spinbox, SIGNAL(valueChanged(double)), this, SLOT(g_spinbox_changed(double)));

    QGridLayout *layout = new QGridLayout;
    layout->addWidget(p_label, 0, 0);
    layout->addWidget(_p_slider, 0, 1);
    layout->addWidget(_p_spinbox, 0, 2);
    layout->addWidget(g_label, 1, 0);
    layout->addWidget(_g_slider, 1, 1);
    layout->addWidget(_g_spinbox, 1, 2);
    setLayout(layout);
}

void stereoscopic_dialog::p_slider_changed(int val)
{
    if (!_lock)
    {
        send_cmd(command::set_parallax, val / 1000.0f);
    }
}

void stereoscopic_dialog::p_spinbox_changed(double val)
{
    if (!_lock)
    {
        send_cmd(command::set_parallax, static_cast<float>(val));
    }
}

void stereoscopic_dialog::g_slider_changed(int val)
{
    if (!_lock)
    {
        send_cmd(command::set_ghostbust, val / 1000.0f);
    }
}

void stereoscopic_dialog::g_spinbox_changed(double val)
{
    if (!_lock)
    {
        send_cmd(command::set_ghostbust, static_cast<float>(val));
    }
}

void stereoscopic_dialog::receive_notification(const notification &note)
{
    std::istringstream current(note.current);
    float value;

    switch (note.type)
    {
    case notification::parallax:
        s11n::load(current, value);
        _lock = true;
        _p_slider->setValue(value * 1000.0f);
        _p_spinbox->setValue(value);
        _lock = false;
        break;
    case notification::ghostbust:
        s11n::load(current, value);
        _lock = true;
        _g_slider->setValue(value * 1000.0f);
        _g_spinbox->setValue(value);
        _lock = false;
        break;
    default:
        /* not handled */
        break;
    }
}


main_window::main_window(QSettings *settings, const player_init_data &init_data) :
    _settings(settings),
    _color_dialog(NULL),
    _crosstalk_dialog(NULL),
    _stereoscopic_dialog(NULL),
    _player(NULL),
    _init_data(init_data),
    _init_data_template(init_data),
    _stop_request(false)
{
    // Application properties
    setWindowTitle(PACKAGE_NAME);
    setWindowIcon(QIcon(":icons/appicon.png"));

    // Load preferences
    _settings->beginGroup("Session");
    if (!(_init_data.params.contrast >= -1.0f && _init_data.params.contrast <= +1.0f))
    {
        _init_data.params.contrast = _settings->value("contrast", QString("0")).toFloat();
    }
    if (!(_init_data.params.brightness >= -1.0f && _init_data.params.brightness <= +1.0f))
    {
        _init_data.params.brightness = _settings->value("brightness", QString("0")).toFloat();
    }
    if (!(_init_data.params.hue >= -1.0f && _init_data.params.hue <= +1.0f))
    {
        _init_data.params.hue = _settings->value("hue", QString("0")).toFloat();
    }
    if (!(_init_data.params.saturation >= -1.0f && _init_data.params.saturation <= +1.0f))
    {
        _init_data.params.saturation = _settings->value("saturation", QString("0")).toFloat();
    }
    if (!(_init_data.params.crosstalk_r >= 0.0f && _init_data.params.crosstalk_r <= 1.0f))
    {
        _init_data.params.crosstalk_r = _settings->value("crosstalk_r", QString("0")).toFloat();
    }
    if (!(_init_data.params.crosstalk_g >= 0.0f && _init_data.params.crosstalk_g <= 1.0f))
    {
        _init_data.params.crosstalk_g = _settings->value("crosstalk_g", QString("0")).toFloat();
    }
    if (!(_init_data.params.crosstalk_b >= 0.0f && _init_data.params.crosstalk_b <= 1.0f))
    {
        _init_data.params.crosstalk_b = _settings->value("crosstalk_b", QString("0")).toFloat();
    }
    _settings->endGroup();
    _init_data.params.set_defaults();

    // Central widget, player, and timer
    QWidget *central_widget = new QWidget(this);
    QGridLayout *layout = new QGridLayout();
    _video_container_widget = new video_container_widget(central_widget);
    connect(_video_container_widget, SIGNAL(move_event()), this, SLOT(move_event()));
    layout->addWidget(_video_container_widget, 0, 0);
    _player = new player_qt_internal(_video_container_widget);
    _timer = new QTimer(this);
    connect(_timer, SIGNAL(timeout()), this, SLOT(playloop_step()));
    _in_out_widget = new in_out_widget(_settings, _player, central_widget);
    layout->addWidget(_in_out_widget, 1, 0);
    _controls_widget = new controls_widget(_settings, central_widget);
    layout->addWidget(_controls_widget, 2, 0);
    layout->setRowStretch(0, 1);
    layout->setColumnStretch(0, 1);
    central_widget->setLayout(layout);
    setCentralWidget(central_widget);

    // Menus
    QMenu *file_menu = menuBar()->addMenu(tr("&File"));
    QAction *file_open_act = new QAction(tr("&Open..."), this);
    file_open_act->setShortcut(QKeySequence::Open);
    connect(file_open_act, SIGNAL(triggered()), this, SLOT(file_open()));
    file_menu->addAction(file_open_act);
    QAction *file_open_url_act = new QAction(tr("Open &URL..."), this);
    connect(file_open_url_act, SIGNAL(triggered()), this, SLOT(file_open_url()));
    file_menu->addAction(file_open_url_act);
    file_menu->addSeparator();
    QAction *file_quit_act = new QAction(tr("&Quit..."), this);
#if QT_VERSION >= 0x040600
    file_quit_act->setShortcut(QKeySequence::Quit);
#else
    file_quit_act->setShortcut(tr("Ctrl+Q"));
#endif
    connect(file_quit_act, SIGNAL(triggered()), this, SLOT(close()));
    file_menu->addAction(file_quit_act);
    QMenu *preferences_menu = menuBar()->addMenu(tr("&Preferences"));
    QAction *preferences_colors_act = new QAction(tr("Display &Color Adjustments..."), this);
    connect(preferences_colors_act, SIGNAL(triggered()), this, SLOT(preferences_colors()));
    preferences_menu->addAction(preferences_colors_act);
    QAction *preferences_crosstalk_act = new QAction(tr("Display Cross&talk Calibration..."), this);
    connect(preferences_crosstalk_act, SIGNAL(triggered()), this, SLOT(preferences_crosstalk()));
    preferences_menu->addAction(preferences_crosstalk_act);
    preferences_menu->addSeparator();
    QAction *preferences_stereoscopic_act = new QAction(tr("Stereoscopic Video Settings..."), this);
    connect(preferences_stereoscopic_act, SIGNAL(triggered()), this, SLOT(preferences_stereoscopic()));
    preferences_menu->addAction(preferences_stereoscopic_act);
    QMenu *help_menu = menuBar()->addMenu(tr("&Help"));
    QAction *help_manual_act = new QAction(tr("&Manual..."), this);
    help_manual_act->setShortcut(QKeySequence::HelpContents);
    connect(help_manual_act, SIGNAL(triggered()), this, SLOT(help_manual()));
    help_menu->addAction(help_manual_act);
    QAction *help_website_act = new QAction(tr("&Website..."), this);
    connect(help_website_act, SIGNAL(triggered()), this, SLOT(help_website()));
    help_menu->addAction(help_website_act);
    QAction *help_keyboard_act = new QAction(tr("&Keyboard Shortcuts"), this);
    connect(help_keyboard_act, SIGNAL(triggered()), this, SLOT(help_keyboard()));
    help_menu->addAction(help_keyboard_act);
    QAction *help_about_act = new QAction(tr("&About"), this);
    connect(help_about_act, SIGNAL(triggered()), this, SLOT(help_about()));
    help_menu->addAction(help_about_act);

    // Handle FileOpen events
    QApplication::instance()->installEventFilter(this);
    // Handle Drop events
    setAcceptDrops(true);

    // Update widget contents
    _in_out_widget->update(_init_data, false, false);
    _controls_widget->update(_init_data, false, false);

    // Show window. Must happen before opening initial files!
    show();
    raise();

    // Open files if any
    if (init_data.urls.size() > 0)
    {
        QStringList urls;
        for (size_t i = 0; i < init_data.urls.size(); i++)
        {
            urls.push_back(QFile::decodeName(init_data.urls[i].c_str()));
        }
        open(urls);
    }
}

main_window::~main_window()
{
    if (_player)
    {
        try { _player->close(); } catch (...) { }
        delete _player;
    }
}

QString main_window::current_file_hash()
{
    // Return SHA1 hash of the name of the current file as a hex string
    QString name = QFileInfo(QFile::decodeName(_init_data.urls[0].c_str())).fileName();
    QString hash = QCryptographicHash::hash(name.toUtf8(), QCryptographicHash::Sha1).toHex();
    return hash;
}

bool main_window::open_player()
{
    try
    {
        _player->open(_init_data);
    }
    catch (std::exception &e)
    {
        QMessageBox::critical(this, "Error", e.what());
        return false;
    }
    adjustSize();
    return true;
}

void main_window::receive_notification(const notification &note)
{
    std::istringstream current(note.current);
    bool flag;

    switch (note.type)
    {
    case notification::play:
        s11n::load(current, flag);
        if (flag)
        {
            // Close and re-open the player. This resets the video state in case
            // we played it before, and it sets the input/output modes to the
            // current choice.
            _player->close();
            _init_data.stereo_layout_override = true;
            _in_out_widget->get_stereo_layout(_init_data.stereo_layout, _init_data.stereo_layout_swap);
            _init_data.video_stream = _in_out_widget->get_video_stream();
            _init_data.audio_stream = _in_out_widget->get_audio_stream();
            _init_data.stereo_mode_override = true;
            _in_out_widget->get_stereo_mode(_init_data.stereo_mode, _init_data.stereo_mode_swap);
            if (!open_player())
            {
                _stop_request = true;
            }
            // Remember the input settings of this video, using an SHA1 hash of its filename.
            _settings->beginGroup("Video/" + current_file_hash());
            _settings->setValue("stereo-layout", QString(video_frame::stereo_layout_to_string(_init_data.stereo_layout, _init_data.stereo_layout_swap).c_str()));
            _settings->endGroup();
            // Remember the 2D or 3D video output mode.
            _settings->setValue((_init_data.stereo_layout == video_frame::mono ? "Session/2d-stereo-mode" : "Session/3d-stereo-mode"),
                    QString(parameters::stereo_mode_to_string(_init_data.stereo_mode, _init_data.stereo_mode_swap).c_str()));
            // Update widgets: we're now playing
            _in_out_widget->update(_init_data, true, true);
            _controls_widget->update(_init_data, true, true);
            // Give the keyboard focus to the video widget
            _video_container_widget->setFocus(Qt::OtherFocusReason);
            // Start the play loop
            _timer->start(0);
        }
        else
        {
            _timer->stop();
        }
        break;

    case notification::video_stream:
        s11n::load(current, _init_data.video_stream);
        _settings->beginGroup("Video/" + current_file_hash());
        _settings->setValue("video-stream", QVariant(_init_data.video_stream).toString());
        _settings->endGroup();
        break;

    case notification::audio_stream:
        s11n::load(current, _init_data.audio_stream);
        _settings->beginGroup("Video/" + current_file_hash());
        _settings->setValue("audio-stream", QVariant(_init_data.audio_stream).toString());
        _settings->endGroup();
        break;

    case notification::contrast:
        s11n::load(current, _init_data.params.contrast);
        break;

    case notification::brightness:
        s11n::load(current, _init_data.params.brightness);
        break;

    case notification::hue:
        s11n::load(current, _init_data.params.hue);
        break;

    case notification::saturation:
        s11n::load(current, _init_data.params.saturation);
        break;

    case notification::stereo_mode_swap:
        s11n::load(current, _init_data.params.stereo_mode_swap);
        // TODO: save this is Session/?d-stereo-mode?
        break;

    case notification::parallax:
        s11n::load(current, _init_data.params.parallax);
        _settings->beginGroup("Video/" + current_file_hash());
        _settings->setValue("parallax", QVariant(_init_data.params.parallax).toString());
        _settings->endGroup();
        break;

    case notification::crosstalk:
        s11n::load(current, _init_data.params.crosstalk_r);
        s11n::load(current, _init_data.params.crosstalk_g);
        s11n::load(current, _init_data.params.crosstalk_b);
        break;

    case notification::ghostbust:
        s11n::load(current, _init_data.params.ghostbust);
        _settings->beginGroup("Video/" + current_file_hash());
        _settings->setValue("ghostbust", QVariant(_init_data.params.ghostbust).toString());
        _settings->endGroup();
        break;

    case notification::pause:
    case notification::stereo_layout:
    case notification::stereo_mode:
    case notification::fullscreen:
    case notification::center:
    case notification::pos:
        /* currently not handled */
        break;
    }
}

void main_window::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
    {
        event->acceptProposedAction();
    }
}

void main_window::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasUrls())
    {
        QList<QUrl> url_list = event->mimeData()->urls();
        QStringList urls;
        for (int i = 0; i < url_list.size(); i++)
        {
            if (url_list[i].toString().startsWith("file://", Qt::CaseInsensitive))
            {
                urls.append(url_list[i].toLocalFile());
            }
            else
            {
                urls.append(url_list[i].toString());
            }
        }
        open(urls);
        event->acceptProposedAction();
    }
}

void main_window::moveEvent(QMoveEvent *)
{
    move_event();
}

void main_window::closeEvent(QCloseEvent *event)
{
    // Remember the Session preferences
    _settings->beginGroup("Session");
    _settings->setValue("contrast", QVariant(_init_data.params.contrast).toString());
    _settings->setValue("brightness", QVariant(_init_data.params.brightness).toString());
    _settings->setValue("hue", QVariant(_init_data.params.hue).toString());
    _settings->setValue("saturation", QVariant(_init_data.params.saturation).toString());
    _settings->setValue("crosstalk_r", QVariant(_init_data.params.crosstalk_r).toString());
    _settings->setValue("crosstalk_g", QVariant(_init_data.params.crosstalk_g).toString());
    _settings->setValue("crosstalk_b", QVariant(_init_data.params.crosstalk_b).toString());
    _settings->endGroup();
    event->accept();
}

bool main_window::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::FileOpen)
    {
        open(QStringList(static_cast<QFileOpenEvent *>(event)->file()));
        return true;
    }
    else
    {
        // pass the event on to the parent class
        return QMainWindow::eventFilter(obj, event);
    }
}

void main_window::move_event()
{
    if (_player)
    {
        _player->move_event();
    }
}

void main_window::playloop_step()
{
    if (_stop_request)
    {
        _timer->stop();
        _player->force_stop();
        _stop_request = false;
    }
    else
    {
        bool r = false;
        try
        {
            r = _player->playloop_step();
        }
        catch (std::exception &e)
        {
            QMessageBox::critical(this, "Error", e.what());
        }
        if (!r)
        {
            _timer->stop();
        }
    }
}

void main_window::open(QStringList filenames)
{
    _player->force_stop();
    _player->close();
    parameters params_bak = _init_data.params;
    _init_data = _init_data_template;
    _init_data.params = params_bak;
    _init_data.urls.clear();
    for (int i = 0; i < filenames.size(); i++)
    {
        _init_data.urls.push_back(filenames[i].toLocal8Bit().constData());
    }
    if (open_player())
    {
        _settings->beginGroup("Video/" + current_file_hash());
        // Get stereo layout for this video
        QString layout_fallback = QString(video_frame::stereo_layout_to_string(
                    _player->get_media_input().video_frame_template().stereo_layout,
                    _player->get_media_input().video_frame_template().stereo_layout_swap).c_str());
        QString layout_name = _settings->value("stereo-layout", layout_fallback).toString();
        video_frame::stereo_layout_from_string(layout_name.toStdString(), _init_data.stereo_layout, _init_data.stereo_layout_swap);
        _init_data.stereo_layout_override = true;
        // Get output parameters for this video
        _init_data.video_stream = QVariant(_settings->value("video-stream", QVariant(_init_data.video_stream)).toString()).toInt();
        _init_data.audio_stream = QVariant(_settings->value("audio-stream", QVariant(_init_data.audio_stream)).toString()).toInt();
        _init_data.params.parallax = QVariant(_settings->value("parallax", QVariant(_init_data.params.parallax)).toString()).toFloat();
        _init_data.params.ghostbust = QVariant(_settings->value("ghostbust", QVariant(_init_data.params.ghostbust)).toString()).toFloat();
        // Get stereo mode for this video
        _settings->endGroup();
        QString mode_fallback = QString(parameters::stereo_mode_to_string(
                    _player->get_parameters().stereo_mode,
                    _player->get_parameters().stereo_mode_swap).c_str());
        QString mode_name = _settings->value(
                (_init_data.stereo_layout == video_frame::mono ? "Session/2d-stereo-mode" : "Session/3d-stereo-mode"),
                mode_fallback).toString();
        parameters::stereo_mode_from_string(mode_name.toStdString(), _init_data.stereo_mode, _init_data.stereo_mode_swap);
        _init_data.stereo_mode_override = true;
        // Fill in the rest with defaults
        _init_data.params.set_defaults();
        // Update the widget with the new settings
        _in_out_widget->update(_init_data, true, false);
        _controls_widget->update(_init_data, true, false);
    }
}

void main_window::file_open()
{
    QFileDialog *file_dialog = new QFileDialog(this);
    file_dialog->setDirectory(_settings->value("Session/file-open-dir", QDir::currentPath()).toString());
    file_dialog->setWindowTitle("Open files");
    file_dialog->setAcceptMode(QFileDialog::AcceptOpen);
    file_dialog->setFileMode(QFileDialog::ExistingFiles);
    if (!file_dialog->exec())
    {
        return;
    }
    QStringList file_names = file_dialog->selectedFiles();
    if (file_names.empty())
    {
        return;
    }
    _settings->setValue("Session/file-open-dir", file_dialog->directory().path());
    open(file_names);
}

void main_window::file_open_url()
{
    QDialog *url_dialog = new QDialog(this);
    url_dialog->setWindowTitle("Open URL");
    QLabel *url_label = new QLabel("URL:");
    QLineEdit *url_edit = new QLineEdit("");
    url_edit->setMinimumWidth(256);
    QPushButton *ok_btn = new QPushButton("OK");
    QPushButton *cancel_btn = new QPushButton("Cancel");
    connect(ok_btn, SIGNAL(pressed()), url_dialog, SLOT(accept()));
    connect(cancel_btn, SIGNAL(pressed()), url_dialog, SLOT(reject()));
    QGridLayout *layout = new QGridLayout();
    layout->addWidget(url_label, 0, 0);
    layout->addWidget(url_edit, 0, 1, 1, 3);
    layout->addWidget(ok_btn, 2, 2);
    layout->addWidget(cancel_btn, 2, 3);
    layout->setColumnStretch(1, 1);
    url_dialog->setLayout(layout);
    url_dialog->exec();
    if (url_dialog->result() == QDialog::Accepted
            && !url_edit->text().isEmpty())
    {
        QString url = url_edit->text();
        open(QStringList(url));
    }
}

void main_window::preferences_colors()
{
    if (!_color_dialog)
    {
        _color_dialog = new color_dialog(_init_data.params, this);
    }
    _color_dialog->show();
    _color_dialog->raise();
    _color_dialog->activateWindow();
}

void main_window::preferences_crosstalk()
{
    if (!_crosstalk_dialog)
    {
        _crosstalk_dialog = new crosstalk_dialog(&_init_data.params, this);
    }
    _crosstalk_dialog->show();
    _crosstalk_dialog->raise();
    _crosstalk_dialog->activateWindow();
}

void main_window::preferences_stereoscopic()
{
    if (!_stereoscopic_dialog)
    {
        _stereoscopic_dialog = new stereoscopic_dialog(_init_data.params, this);
    }
    _stereoscopic_dialog->show();
    _stereoscopic_dialog->raise();
    _stereoscopic_dialog->activateWindow();
}

void main_window::help_manual()
{
    QUrl manual_url;
#if defined(Q_WS_WIN32)
    manual_url = QUrl::fromLocalFile(QCoreApplication::applicationDirPath() + "/../doc/bino.html");
#elif defined(Q_WS_MAC)
    manual_url = QUrl::fromLocalFile(QCoreApplication::applicationDirPath() + "/../Resources/Bino Help/bino.html");
#else
    manual_url = QUrl::fromLocalFile(DOCDIR "/bino.html");
#endif
    if (!QDesktopServices::openUrl(manual_url))
    {
        QMessageBox::critical(this, "Error", "Cannot open manual");
    }
}

void main_window::help_website()
{
    if (!QDesktopServices::openUrl(QUrl(PACKAGE_URL)))
    {
        QMessageBox::critical(this, "Error", "Cannot open website");
    }
}

void main_window::help_keyboard()
{
    QMessageBox::information(this, tr("Keyboard Shortcuts"), tr(
                "<p>Keyboard control:<br>"
                "(Click into the video area to give it the keyboard focus if necessary.)"
                "<table>"
                "<tr><td>q or ESC</td><td>Stop</td></tr>"
                "<tr><td>p or SPACE</td><td>Pause / unpause</td></tr>"
                "<tr><td>f</td><td>Toggle fullscreen</td></tr>"
                "<tr><td>c</td><td>Center window</td></tr>"
                "<tr><td>s</td><td>Swap left/right view</td></tr>"
                "<tr><td>v</td><td>Cycle through available video streams</td></tr>"
                "<tr><td>a</td><td>Cycle through available audio streams</td></tr>"
                "<tr><td>1, 2</td><td>Adjust contrast</td></tr>"
                "<tr><td>3, 4</td><td>Adjust brightness</td></tr>"
                "<tr><td>5, 6</td><td>Adjust hue</td></tr>"
                "<tr><td>7, 8</td><td>Adjust saturation</td></tr>"
                "<tr><td>&lt;, &gt;</td><td>Adjust parallax</td></tr>"
                "<tr><td>(, )</td><td>Adjust ghostbusting</td></tr>"
                "<tr><td>left, right</td><td>Seek 10 seconds backward / forward</td></tr>"
                "<tr><td>up, down</td><td>Seek 1 minute backward / forward</td></tr>"
                "<tr><td>page up, page down</td><td>Seek 10 minutes backward / forward</td></tr>"
                "</table>"
                "</p>"));
}

void main_window::help_about()
{
    QString blurb = tr(
            "<p>%1 version %2</p>"
            "<p>Copyright (C) 2011 the Bino developers.<br>"
            "This is free software. You may redistribute copies of it<br>"
            "under the terms of the <a href=\"http://www.gnu.org/licenses/gpl.html\">"
            "GNU General Public License</a>.<br>"
            "There is NO WARRANTY, to the extent permitted by law.<br>"
            "See <a href=\"%3\">%3</a> for more information on this software.</p>")
        .arg(PACKAGE_NAME).arg(VERSION).arg(PACKAGE_URL);
    blurb += tr("<p>Platform:<ul><li>%1</li></ul></p>").arg(PLATFORM);
    blurb += QString("<p>Libraries used:");
    std::vector<std::string> libs = lib_versions(true);
    for (size_t i = 0; i < libs.size(); i++)
    {
        blurb += libs[i].c_str();
    }
    blurb += QString("</p>");
    QMessageBox::about(this, tr("About " PACKAGE_NAME), blurb);
}

player_qt::player_qt() : player(player::slave)
{
    _qt_app_owner = init_qt();
    QCoreApplication::setOrganizationName(PACKAGE_NAME);
    QCoreApplication::setApplicationName(PACKAGE_NAME);
    _settings = new QSettings;
}

player_qt::~player_qt()
{
    if (_qt_app_owner)
    {
        exit_qt();
    }
    delete _settings;
}

void player_qt::open(const player_init_data &init_data)
{
    msg::set_level(init_data.log_level);
    _main_window = new main_window(_settings, init_data);
}

void player_qt::run()
{
    exec_qt();
    delete _main_window;
    _main_window = NULL;
}

void player_qt::close()
{
}
