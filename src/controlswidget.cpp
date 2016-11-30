/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010, 2011, 2012, 2013, 2014, 2016
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

#include <QGridLayout>
#include <QIcon>
#include <QLabel>
#include <QSettings>
#include <QSlider>
#include <QPushButton>

#include "controlswidget.h"
#include "gui_common.h"
#include "media_input.h"

controls_widget::controls_widget(QSettings *settings, QWidget *parent)
    : QWidget(parent), _input_duration(0), _lock(false), _settings(settings)
{
    QGridLayout *row0_layout = new QGridLayout;
    _seek_slider = new QSlider(Qt::Horizontal);
    _seek_slider->setToolTip(_("<p>This slider shows the progress during video playback, "
                "and can be used to seek in the video.</p>"));
    _seek_slider->setRange(0, 2000);
    _seek_slider->setTracking(false);
    connect(_seek_slider, SIGNAL(valueChanged(int)), this, SLOT(seek_slider_changed()));
    row0_layout->addWidget(_seek_slider, 0, 0);
    _pos_label = new QLabel("0:00");
    _pos_label->setToolTip(_("<p>Elapsed / total time.</p>"));
    _pos_label->setAlignment(Qt::AlignRight);
    _pos_label->setTextFormat(Qt::PlainText);
    _pos_label->setFrameShape(QFrame::StyledPanel);
    _pos_label->setMinimumSize(QSize(0, 0));
    row0_layout->addWidget(_pos_label, 0, 1);
    _audio_mute_button = new QPushButton(get_icon("audio-volume-medium"), "");
    _audio_mute_button->setToolTip(_("<p>Toggle audio mute.</p>"));
    _audio_mute_button->setCheckable(true);
    connect(_audio_mute_button, SIGNAL(toggled(bool)), this, SLOT(audio_mute_clicked()));
    row0_layout->addWidget(_audio_mute_button, 0, 2);
    _audio_volume_slider = new QSlider(Qt::Horizontal);
    _audio_volume_slider->setToolTip(_("<p>Adjust audio volume.</p>"));
    _audio_volume_slider->setRange(0, 1000);
    _audio_volume_slider->setTickPosition(QSlider::TicksBelow);
    _audio_volume_slider->setTickInterval(100);
    _audio_volume_slider->setSingleStep(25);
    _audio_volume_slider->setPageStep(200);
    connect(_audio_volume_slider, SIGNAL(valueChanged(int)), this, SLOT(audio_volume_slider_changed()));
    row0_layout->addWidget(_audio_volume_slider, 0, 3);
    row0_layout->setColumnStretch(0, 1);

    QGridLayout *row1_layout = new QGridLayout;
    _play_button = new QPushButton(get_icon("media-playback-start"), "");
    _play_button->setToolTip(_("<p>Play.</p>"));
    connect(_play_button, SIGNAL(clicked()), this, SLOT(play_clicked()));
    row1_layout->addWidget(_play_button, 1, 0);
    _pause_button = new QPushButton(get_icon("media-playback-pause"), "");
    _pause_button->setToolTip(_("<p>Pause.</p>"));
    connect(_pause_button, SIGNAL(clicked()), this, SLOT(pause_clicked()));
    row1_layout->addWidget(_pause_button, 1, 1);
    _stop_button = new QPushButton(get_icon("media-playback-stop"), "");
    _stop_button->setToolTip(_("<p>Stop.</p>"));
    connect(_stop_button, SIGNAL(clicked()), this, SLOT(stop_clicked()));
    row1_layout->addWidget(_stop_button, 1, 2);
    row1_layout->addWidget(new QWidget, 1, 3);
    _loop_button = new QPushButton(get_icon("media-playlist-repeat"), "");
    _loop_button->setToolTip(_("<p>Toggle loop mode.</p>"));
    _loop_button->setCheckable(true);
    _loop_button->setChecked(dispatch::parameters().loop_mode() != parameters::no_loop);
    connect(_loop_button, SIGNAL(toggled(bool)), this, SLOT(loop_clicked()));
    row1_layout->addWidget(_loop_button, 1, 4);
    row1_layout->addWidget(new QWidget, 1, 5);
    _fullscreen_button = new QPushButton(get_icon("view-fullscreen"), "");
    _fullscreen_button->setToolTip(_("<p>Switch to fullscreen mode. "
                "You can leave fullscreen mode by pressing the f key.</p>"));
    _fullscreen_button->setCheckable(true);
    connect(_fullscreen_button, SIGNAL(clicked()), this, SLOT(fullscreen_clicked()));
    row1_layout->addWidget(_fullscreen_button, 1, 6);
    _center_button = new QPushButton(get_icon("view-restore"), "");
    _center_button->setToolTip(_("<p>Center the video area on your screen.</p>"));
    connect(_center_button, SIGNAL(clicked()), this, SLOT(center_clicked()));
    row1_layout->addWidget(_center_button, 1, 7);
    row1_layout->addWidget(new QWidget, 1, 8);
    _bbb_button = new QPushButton(get_icon("media-seek-backward"), "");
    _bbb_button->setFixedSize(_bbb_button->minimumSizeHint());
    _bbb_button->setIconSize(_bbb_button->iconSize() * 12 / 10);
    _bbb_button->setToolTip(_("<p>Seek backward 10 minutes.</p>"));
    connect(_bbb_button, SIGNAL(clicked()), this, SLOT(bbb_clicked()));
    row1_layout->addWidget(_bbb_button, 1, 9);
    _bb_button = new QPushButton(get_icon("media-seek-backward"), "");
    _bb_button->setFixedSize(_bb_button->minimumSizeHint());
    _bb_button->setToolTip(_("<p>Seek backward 1 minute.</p>"));
    connect(_bb_button, SIGNAL(clicked()), this, SLOT(bb_clicked()));
    row1_layout->addWidget(_bb_button, 1, 10);
    _b_button = new QPushButton(get_icon("media-seek-backward"), "");
    _b_button->setFixedSize(_b_button->minimumSizeHint());
    _b_button->setIconSize(_b_button->iconSize() * 8 / 10);
    _b_button->setToolTip(_("<p>Seek backward 10 seconds.</p>"));
    connect(_b_button, SIGNAL(clicked()), this, SLOT(b_clicked()));
    row1_layout->addWidget(_b_button, 1, 11);
    _f_button = new QPushButton(get_icon("media-seek-forward"), "");
    _f_button->setFixedSize(_f_button->minimumSizeHint());
    _f_button->setIconSize(_f_button->iconSize() * 8 / 10);
    _f_button->setToolTip(_("<p>Seek forward 10 seconds.</p>"));
    connect(_f_button, SIGNAL(clicked()), this, SLOT(f_clicked()));
    row1_layout->addWidget(_f_button, 1, 12);
    _ff_button = new QPushButton(get_icon("media-seek-forward"), "");
    _ff_button->setFixedSize(_ff_button->minimumSizeHint());
    _ff_button->setToolTip(_("<p>Seek forward 1 minute.</p>"));
    connect(_ff_button, SIGNAL(clicked()), this, SLOT(ff_clicked()));
    row1_layout->addWidget(_ff_button, 1, 13);
    _fff_button = new QPushButton(get_icon("media-seek-forward"), "");
    _fff_button->setFixedSize(_fff_button->minimumSizeHint());
    _fff_button->setIconSize(_fff_button->iconSize() * 12 / 10);
    _fff_button->setToolTip(_("<p>Seek forward 10 minutes.</p>"));
    connect(_fff_button, SIGNAL(clicked()), this, SLOT(fff_clicked()));
    row1_layout->addWidget(_fff_button, 1, 14);
    row1_layout->setRowStretch(0, 0);
    row1_layout->setColumnStretch(3, 1);
    row1_layout->setColumnStretch(5, 1);
    row1_layout->setColumnStretch(8, 1);

    QGridLayout *layout = new QGridLayout;
    layout->addLayout(row0_layout, 0, 0);
    layout->addLayout(row1_layout, 1, 0);
    setLayout(layout);

    _play_button->setEnabled(false);
    _pause_button->setEnabled(false);
    _stop_button->setEnabled(false);
    _loop_button->setEnabled(false);
    _fullscreen_button->setEnabled(false);
    _center_button->setEnabled(false);
    _bbb_button->setEnabled(false);
    _bb_button->setEnabled(false);
    _b_button->setEnabled(false);
    _f_button->setEnabled(false);
    _ff_button->setEnabled(false);
    _fff_button->setEnabled(false);
    _seek_slider->setEnabled(false);
    _pos_label->setEnabled(false);

    update();
    update_audio_widgets();
}

void controls_widget::update_audio_widgets()
{
    _lock = true;
    _audio_mute_button->setChecked(dispatch::parameters().audio_mute());
    _audio_mute_button->setIcon(get_icon(
                dispatch::parameters().audio_mute() ? "audio-volume-muted"
                : dispatch::parameters().audio_volume() < 0.33f ? "audio-volume-low"
                : dispatch::parameters().audio_volume() < 0.66f ? "audio-volume-medium"
                : "audio-volume-high"));
    _audio_volume_slider->setValue(qRound(dispatch::parameters().audio_volume() * 1000.0f));
    _lock = false;
}

void controls_widget::play_clicked()
{
    if (dispatch::playing())
    {
        send_cmd(command::toggle_pause);
    }
    else
    {
        send_cmd(command::toggle_play);
    }
}

void controls_widget::pause_clicked()
{
    send_cmd(command::toggle_pause);
}

void controls_widget::stop_clicked()
{
    send_cmd(command::toggle_play);
}

void controls_widget::loop_clicked()
{
    if (!_lock)
    {
        parameters::loop_mode_t loop_mode = _loop_button->isChecked()
            ? parameters::loop_current : parameters::no_loop;
        send_cmd(command::set_loop_mode, static_cast<int>(loop_mode));
    }
}

void controls_widget::fullscreen_clicked()
{
    if (!_lock)
    {
        send_cmd(command::toggle_fullscreen);
    }
}

void controls_widget::center_clicked()
{
    send_cmd(command::center);
}

void controls_widget::bbb_clicked()
{
    send_cmd(command::seek, -600.0f);
}

void controls_widget::bb_clicked()
{
    send_cmd(command::seek, -60.0f);
}

void controls_widget::b_clicked()
{
    send_cmd(command::seek, -10.0f);
}

void controls_widget::f_clicked()
{
    send_cmd(command::seek, +10.0f);
}

void controls_widget::ff_clicked()
{
    send_cmd(command::seek, +60.0f);
}

void controls_widget::fff_clicked()
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

void controls_widget::audio_mute_clicked()
{
    if (!_lock)
    {
        send_cmd(command::toggle_audio_mute);
    }
}

void controls_widget::audio_volume_slider_changed()
{
    if (!_lock)
    {
        send_cmd(command::set_audio_volume, _audio_volume_slider->value() / 1000.0f);
    }
}

void controls_widget::update()
{
    if (dispatch::media_input())
    {
        _play_button->setDefault(true);
        _play_button->setFocus();
        _loop_button->setEnabled(true);
        _fullscreen_button->setEnabled(true);
        _input_duration = dispatch::media_input()->duration();
        if (_input_duration < 0)
            _input_duration = 0;
        std::string hr_duration = str::human_readable_time(_input_duration);
        _pos_label->setText((hr_duration + '/' + hr_duration).c_str());
        _pos_label->setMinimumSize(_pos_label->minimumSizeHint());
        _pos_label->setText(hr_duration.c_str());
    }
    else
    {
        _play_button->setEnabled(false);
        _pause_button->setEnabled(false);
        _stop_button->setEnabled(false);
        _loop_button->setEnabled(false);
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
        _pos_label->setEnabled(false);
        _pos_label->setText("0:00");
        _pos_label->setMinimumSize(QSize(0, 0));
    }
}

void controls_widget::receive_notification(const notification &note)
{
    bool b;

    switch (note.type)
    {
    case notification::open:
        update();
        break;
    case notification::play:
        b = dispatch::playing();
        _play_button->setEnabled(!b);
        _pause_button->setEnabled(b);
        _stop_button->setEnabled(b);
        if (b && _fullscreen_button->isChecked() && !dispatch::parameters().fullscreen())
        {
            _lock = true;
            send_cmd(command::toggle_fullscreen);
            _lock = false;
        }
        _center_button->setEnabled(b);
        _bbb_button->setEnabled(b);
        _bb_button->setEnabled(b);
        _b_button->setEnabled(b);
        _f_button->setEnabled(b);
        _ff_button->setEnabled(b);
        _fff_button->setEnabled(b);
        _seek_slider->setEnabled(b);
        _pos_label->setEnabled(b);
        if (!b)
        {
            _seek_slider->setValue(0);
            _pos_label->setText(str::human_readable_time(_input_duration).c_str());
        }
        break;
    case notification::pause:
        b = dispatch::pausing();
        _play_button->setEnabled(b);
        _pause_button->setEnabled(!b);
        break;
    case notification::fullscreen:
        _lock = true;
        _fullscreen_button->setChecked(dispatch::parameters().fullscreen());
        _lock = false;
        break;
    case notification::pos:
        if (!_seek_slider->isSliderDown())
        {
            _lock = true;
            float p = dispatch::position();
            _seek_slider->setValue(qRound(p * 2000.0f));
            _pos_label->setText((str::human_readable_time(
                            static_cast<int64_t>(p * 1000.0f) * _input_duration / 1000)
                        + '/' + str::human_readable_time(_input_duration)).c_str());
            _lock = false;
        }
        break;
    case notification::audio_volume:
        update_audio_widgets();
        break;
    case notification::audio_mute:
        update_audio_widgets();
        break;
    default:
        break;
    }
}

QIcon controls_widget::get_icon(const QString& name)
{
    return QIcon::fromTheme(name, QIcon(QString(":icons/") + name));
}
