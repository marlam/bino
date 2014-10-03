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

#include <QGridLayout>
#include <QLabel>
#include <QComboBox>
#include <QSettings>
#include <QCheckBox>
#include <QStandardItemModel>

#include "inoutwidget.h"

#include "gui_common.h"
#include "media_input.h"
#include "video_output.h"

in_out_widget::in_out_widget(QSettings *settings, QWidget *parent) :
    QWidget(parent), _settings(settings), _lock(false)
{
    QGridLayout *layout0 = new QGridLayout;
    QLabel *video_label = new QLabel(_("Video:"));
    video_label->setToolTip(_("<p>Select the video stream.</p>"));
    layout0->addWidget(video_label, 0, 0);
    _video_combobox = new QComboBox(this);
    _video_combobox->setToolTip(video_label->toolTip());
    connect(_video_combobox, SIGNAL(currentIndexChanged(int)), this, SLOT(video_changed()));
    layout0->addWidget(_video_combobox, 0, 1);
    QLabel *audio_label = new QLabel(_("Audio:"));
    audio_label->setToolTip(_("<p>Select the audio stream.</p>"));
    layout0->addWidget(audio_label, 0, 2);
    _audio_combobox = new QComboBox(this);
    _audio_combobox->setToolTip(audio_label->toolTip());
    connect(_audio_combobox, SIGNAL(currentIndexChanged(int)), this, SLOT(audio_changed()));
    layout0->addWidget(_audio_combobox, 0, 3);
    QLabel *subtitle_label = new QLabel(_("Subtitle:"));
    subtitle_label->setToolTip(_("<p>Select the subtitle stream.</p>"));
    layout0->addWidget(subtitle_label, 0, 4);
    _subtitle_combobox = new QComboBox(this);
    _subtitle_combobox->setToolTip(subtitle_label->toolTip());
    connect(_subtitle_combobox, SIGNAL(currentIndexChanged(int)), this, SLOT(subtitle_changed()));
    layout0->addWidget(_subtitle_combobox, 0, 5);
    layout0->setColumnStretch(1, 1);
    layout0->setColumnStretch(3, 1);
    layout0->setColumnStretch(5, 1);

    QGridLayout *layout1 = new QGridLayout;
    QLabel *input_label = new QLabel(_("Input:"));
    input_label->setToolTip(_("<p>Set the 3D layout of the video stream.</p>"));
    layout1->addWidget(input_label, 0, 0);
    _input_combobox = new QComboBox(this);
    _input_combobox->setToolTip(input_label->toolTip());
    _input_combobox->addItem(QIcon(":icons-local/input-layout-mono.png"), _("2D"));
    _input_combobox->addItem(QIcon(":icons-local/input-layout-separate-left-right.png"), _("Separate streams, left first"));
    _input_combobox->addItem(QIcon(":icons-local/input-layout-separate-right-left.png"), _("Separate streams, right first"));
    _input_combobox->addItem(QIcon(":icons-local/input-layout-alternating-left-right.png"), _("Alternating, left first"));
    _input_combobox->addItem(QIcon(":icons-local/input-layout-alternating-right-left.png"), _("Alternating, right first"));
    _input_combobox->addItem(QIcon(":icons-local/input-layout-top-bottom.png"), _("Top/bottom"));
    _input_combobox->addItem(QIcon(":icons-local/input-layout-top-bottom-half.png"), _("Top/bottom, half height"));
    _input_combobox->addItem(QIcon(":icons-local/input-layout-bottom-top.png"), _("Bottom/top"));
    _input_combobox->addItem(QIcon(":icons-local/input-layout-bottom-top-half.png"), _("Bottom/top, half height"));
    _input_combobox->addItem(QIcon(":icons-local/input-layout-left-right.png"), _("Left/right"));
    _input_combobox->addItem(QIcon(":icons-local/input-layout-left-right-half.png"), _("Left/right, half width"));
    _input_combobox->addItem(QIcon(":icons-local/input-layout-right-left.png"), _("Right/left"));
    _input_combobox->addItem(QIcon(":icons-local/input-layout-right-left-half.png"), _("Right/left, half width"));
    _input_combobox->addItem(QIcon(":icons-local/input-layout-even-odd-rows.png"), _("Even/odd rows"));
    _input_combobox->addItem(QIcon(":icons-local/input-layout-odd-even-rows.png"), _("Odd/even rows"));
    connect(_input_combobox, SIGNAL(currentIndexChanged(int)), this, SLOT(input_changed()));
    layout1->addWidget(_input_combobox, 0, 1);
    layout1->setColumnStretch(1, 1);

    QGridLayout *layout2 = new QGridLayout;
    QLabel *output_label = new QLabel(_("Output:"));
    output_label->setToolTip(_("<p>Set the 3D output type for your display.</p>"));
    layout2->addWidget(output_label, 0, 0);
    _output_combobox = new QComboBox(this);
    _output_combobox->setToolTip(output_label->toolTip());
    _output_combobox->addItem(QIcon(":icons-local/output-type-mono-left.png"), _("Left view"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-mono-right.png"), _("Right view"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-stereo.png"), _("OpenGL stereo"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-alternating.png"), _("Left/right alternating"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-top-bottom.png"), _("Top/bottom"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-top-bottom-half.png"), _("Top/bottom, half height"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-left-right.png"), _("Left/right"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-left-right-half.png"), _("Left/right, half width"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-even-odd-rows.png"), _("Even/odd rows"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-even-odd-columns.png"), _("Even/odd columns"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-checkerboard.png"), _("Checkerboard pattern"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-hdmi-frame-pack.png"), _("HDMI frame packing mode"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-red-cyan.png"), _("Red/cyan glasses, monochrome"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-red-cyan.png"), _("Red/cyan glasses, half color"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-red-cyan.png"), _("Red/cyan glasses, full color"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-red-cyan.png"), _("Red/cyan glasses, Dubois"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-green-magenta.png"), _("Green/magenta glasses, monochrome"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-green-magenta.png"), _("Green/magenta glasses, half color"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-green-magenta.png"), _("Green/magenta glasses, full color"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-green-magenta.png"), _("Green/magenta glasses, Dubois"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-amber-blue.png"), _("Amber/blue glasses, monochrome"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-amber-blue.png"), _("Amber/blue glasses, half color"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-amber-blue.png"), _("Amber/blue glasses, full color"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-amber-blue.png"), _("Amber/blue glasses, Dubois"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-red-green.png"), _("Red/green glasses, monochrome"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-red-blue.png"), _("Red/blue glasses, monochrome"));
    connect(_output_combobox, SIGNAL(currentIndexChanged(int)), this, SLOT(output_changed()));
    layout2->addWidget(_output_combobox, 0, 1);
    layout2->setColumnStretch(1, 1);
    _swap_checkbox = new QCheckBox(_("Swap left/right"));
    _swap_checkbox->setToolTip(_("<p>Swap the left and right view. "
                "Use this if the 3D effect seems wrong.</p>"));
    connect(_swap_checkbox, SIGNAL(stateChanged(int)), this, SLOT(swap_changed()));
    layout2->addWidget(_swap_checkbox, 0, 2);

    QGridLayout *layout = new QGridLayout;
    layout->addLayout(layout0, 0, 0);
    layout->addLayout(layout1, 1, 0);
    layout->addLayout(layout2, 2, 0);
    setLayout(layout);

    // Align the labels
    int minw = input_label->minimumSizeHint().width();
    if (output_label->minimumSizeHint().width() > minw)
    {
        minw = output_label->minimumSizeHint().width();
    }
    if (video_label->minimumSizeHint().width() > minw)
    {
        minw = video_label->minimumSizeHint().width();
    }
    if (audio_label->minimumSizeHint().width() > minw)
    {
        minw = audio_label->minimumSizeHint().width();
    }
    if (subtitle_label->minimumSizeHint().width() > minw)
    {
        minw = subtitle_label->minimumSizeHint().width();
    }
    input_label->setMinimumSize(QSize(minw, input_label->minimumSizeHint().height()));
    output_label->setMinimumSize(QSize(minw, output_label->minimumSizeHint().height()));
    video_label->setMinimumSize(QSize(minw, video_label->minimumSizeHint().height()));
    audio_label->setMinimumSize(QSize(minw, audio_label->minimumSizeHint().height()));
    subtitle_label->setMinimumSize(QSize(minw, subtitle_label->minimumSizeHint().height()));

    _video_combobox->setEnabled(false);
    _audio_combobox->setEnabled(false);
    _subtitle_combobox->setEnabled(false);
    _input_combobox->setEnabled(false);
    _output_combobox->setEnabled(false);
    _swap_checkbox->setEnabled(false);

    update();
}

void in_out_widget::set_stereo_layout(parameters::stereo_layout_t stereo_layout, bool stereo_layout_swap)
{
    switch (stereo_layout)
    {
    case parameters::layout_mono:
        _input_combobox->setCurrentIndex(0);
        break;
    case parameters::layout_separate:
        _input_combobox->setCurrentIndex(stereo_layout_swap ? 2 : 1);
        break;
    case parameters::layout_alternating:
        _input_combobox->setCurrentIndex(stereo_layout_swap ? 4 : 3);
        break;
    case parameters::layout_top_bottom:
        _input_combobox->setCurrentIndex(stereo_layout_swap ? 7 : 5);
        break;
    case parameters::layout_top_bottom_half:
        _input_combobox->setCurrentIndex(stereo_layout_swap ? 8 : 6);
        break;
    case parameters::layout_left_right:
        _input_combobox->setCurrentIndex(stereo_layout_swap ? 11: 9);
        break;
    case parameters::layout_left_right_half:
        _input_combobox->setCurrentIndex(stereo_layout_swap ? 12 : 10);
        break;
    case parameters::layout_even_odd_rows:
        _input_combobox->setCurrentIndex(stereo_layout_swap ? 14 : 13);
        break;
    }
    _video_combobox->setEnabled(stereo_layout != parameters::layout_separate);
}

void in_out_widget::set_stereo_mode(parameters::stereo_mode_t stereo_mode, bool stereo_mode_swap)
{
    switch (stereo_mode)
    {
    default:
    case parameters::mode_mono_left:
        _output_combobox->setCurrentIndex(0);
        break;
    case parameters::mode_mono_right:
        _output_combobox->setCurrentIndex(1);
        break;
    case parameters::mode_stereo:
        _output_combobox->setCurrentIndex(2);
        break;
    case parameters::mode_alternating:
        _output_combobox->setCurrentIndex(3);
        break;
    case parameters::mode_top_bottom:
        _output_combobox->setCurrentIndex(4);
        break;
    case parameters::mode_top_bottom_half:
        _output_combobox->setCurrentIndex(5);
        break;
    case parameters::mode_left_right:
        _output_combobox->setCurrentIndex(6);
        break;
    case parameters::mode_left_right_half:
        _output_combobox->setCurrentIndex(7);
        break;
    case parameters::mode_even_odd_rows:
        _output_combobox->setCurrentIndex(8);
        break;
    case parameters::mode_even_odd_columns:
        _output_combobox->setCurrentIndex(9);
        break;
    case parameters::mode_checkerboard:
        _output_combobox->setCurrentIndex(10);
        break;
    case parameters::mode_hdmi_frame_pack:
        _output_combobox->setCurrentIndex(11);
        break;
    case parameters::mode_red_cyan_monochrome:
        _output_combobox->setCurrentIndex(12);
        break;
    case parameters::mode_red_cyan_half_color:
        _output_combobox->setCurrentIndex(13);
        break;
    case parameters::mode_red_cyan_full_color:
        _output_combobox->setCurrentIndex(14);
        break;
    case parameters::mode_red_cyan_dubois:
        _output_combobox->setCurrentIndex(15);
        break;
    case parameters::mode_green_magenta_monochrome:
        _output_combobox->setCurrentIndex(16);
        break;
    case parameters::mode_green_magenta_half_color:
        _output_combobox->setCurrentIndex(17);
        break;
    case parameters::mode_green_magenta_full_color:
        _output_combobox->setCurrentIndex(18);
        break;
    case parameters::mode_green_magenta_dubois:
        _output_combobox->setCurrentIndex(19);
        break;
    case parameters::mode_amber_blue_monochrome:
        _output_combobox->setCurrentIndex(20);
        break;
    case parameters::mode_amber_blue_half_color:
        _output_combobox->setCurrentIndex(21);
        break;
    case parameters::mode_amber_blue_full_color:
        _output_combobox->setCurrentIndex(22);
        break;
    case parameters::mode_amber_blue_dubois:
        _output_combobox->setCurrentIndex(23);
        break;
    case parameters::mode_red_green_monochrome:
        _output_combobox->setCurrentIndex(24);
        break;
    case parameters::mode_red_blue_monochrome:
        _output_combobox->setCurrentIndex(25);
        break;
    }
    _swap_checkbox->setChecked(stereo_mode_swap);
}

void in_out_widget::video_changed()
{
    if (!_lock)
    {
        parameters::stereo_layout_t stereo_layout;
        bool stereo_layout_swap;
        get_stereo_layout(stereo_layout, stereo_layout_swap);
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

void in_out_widget::subtitle_changed()
{
    if (!_lock)
    {
        send_cmd(command::set_subtitle_stream, _subtitle_combobox->currentIndex() - 1);
    }
}

void in_out_widget::input_changed()
{
    if (!_lock)
    {
        parameters::stereo_layout_t stereo_layout;
        bool stereo_layout_swap;
        get_stereo_layout(stereo_layout, stereo_layout_swap);
        if (stereo_layout == parameters::layout_separate)
        {
            _lock = true;
            _video_combobox->setCurrentIndex(0);
            _video_combobox->setEnabled(false);
            _lock = false;
        }
        else
        {
            _video_combobox->setEnabled(true);
        }
        send_cmd(command::set_stereo_layout, static_cast<int>(stereo_layout));
        send_cmd(command::set_stereo_layout_swap, stereo_layout_swap);
        parameters::stereo_mode_t stereo_mode;
        bool stereo_mode_swap;
        get_stereo_mode(stereo_mode, stereo_mode_swap);
        if (stereo_layout == parameters::layout_mono
                && !(stereo_mode == parameters::mode_mono_left || stereo_mode == parameters::mode_mono_right))
        {
            QString s = _settings->value("Session/2d-stereo-mode", "").toString();
            parameters::stereo_mode_from_string(s.toStdString(), stereo_mode, stereo_mode_swap);
            _lock = true;
            set_stereo_mode(stereo_mode, stereo_mode_swap);
            _lock = false;
        }
        else if (stereo_layout != parameters::layout_mono
                && (stereo_mode == parameters::mode_mono_left || stereo_mode == parameters::mode_mono_right))
        {
            QString s = _settings->value("Session/3d-stereo-mode", "").toString();
            parameters::stereo_mode_from_string(s.toStdString(), stereo_mode, stereo_mode_swap);
            _lock = true;
            set_stereo_mode(stereo_mode, stereo_mode_swap);
            _lock = false;
        }
    }
}

void in_out_widget::output_changed()
{
    if (!_lock)
    {
        parameters::stereo_mode_t stereo_mode;
        bool stereo_mode_swap;
        get_stereo_mode(stereo_mode, stereo_mode_swap);
        send_cmd(command::set_stereo_mode, static_cast<int>(stereo_mode));
        send_cmd(command::set_stereo_mode_swap, stereo_mode_swap);
    }
}

void in_out_widget::swap_changed()
{
    if (!_lock)
    {
        send_cmd(command::toggle_stereo_mode_swap);
    }
}

void in_out_widget::update()
{
    const media_input *mi = dispatch::media_input();
    _lock = true;
    _video_combobox->setEnabled(mi);
    _audio_combobox->setEnabled(mi);
    _subtitle_combobox->setEnabled(mi);
    _input_combobox->setEnabled(mi);
    _output_combobox->setEnabled(mi);
    _swap_checkbox->setEnabled(mi);
    _video_combobox->clear();
    _audio_combobox->clear();
    _subtitle_combobox->clear();
    if (mi)
    {
        for (int i = 0; i < mi->video_streams(); i++)
        {
            _video_combobox->addItem(mi->video_stream_name(i).c_str());
        }
        for (int i = 0; i < mi->audio_streams(); i++)
        {
            _audio_combobox->addItem(mi->audio_stream_name(i).c_str());
        }
        _subtitle_combobox->addItem(_("Off"));
        for (int i = 0; i < mi->subtitle_streams(); i++)
        {
            _subtitle_combobox->addItem(mi->subtitle_stream_name(i).c_str());
        }
        _video_combobox->setCurrentIndex(dispatch::parameters().video_stream());
        _audio_combobox->setCurrentIndex(dispatch::parameters().audio_stream());
        _subtitle_combobox->setCurrentIndex(dispatch::parameters().subtitle_stream() + 1);
        // Disable unsupported input modes
        for (int i = 0; i < _input_combobox->count(); i++)
        {
            _input_combobox->setCurrentIndex(i);
            parameters::stereo_layout_t layout;
            bool swap;
            get_stereo_layout(layout, swap);
            qobject_cast<QStandardItemModel *>(_input_combobox->model())->item(i)->setEnabled(
                    mi->stereo_layout_is_supported(layout, swap));
        }
        // Disable unsupported output modes
        if (!dispatch::video_output() || !dispatch::video_output()->supports_stereo())
        {
            set_stereo_mode(parameters::mode_stereo, false);
            qobject_cast<QStandardItemModel *>(_output_combobox->model())->item(_output_combobox->currentIndex())->setEnabled(false);
        }
        set_stereo_layout(dispatch::parameters().stereo_layout(), dispatch::parameters().stereo_layout_swap());
        set_stereo_mode(dispatch::parameters().stereo_mode(), dispatch::parameters().stereo_mode_swap());
    }
    _lock = false;
}

int in_out_widget::get_video_stream()
{
    return _video_combobox->currentIndex();
}

int in_out_widget::get_audio_stream()
{
    return _audio_combobox->currentIndex();
}

int in_out_widget::get_subtitle_stream()
{
    return _subtitle_combobox->currentIndex() - 1;
}

void in_out_widget::get_stereo_layout(parameters::stereo_layout_t &stereo_layout, bool &stereo_layout_swap)
{
    switch (_input_combobox->currentIndex())
    {
    case 0:
        stereo_layout = parameters::layout_mono;
        stereo_layout_swap = false;
        break;
    case 1:
        stereo_layout = parameters::layout_separate;
        stereo_layout_swap = false;
        break;
    case 2:
        stereo_layout = parameters::layout_separate;
        stereo_layout_swap = true;
        break;
    case 3:
        stereo_layout = parameters::layout_alternating;
        stereo_layout_swap = false;
        break;
    case 4:
        stereo_layout = parameters::layout_alternating;
        stereo_layout_swap = true;
        break;
    case 5:
        stereo_layout = parameters::layout_top_bottom;
        stereo_layout_swap = false;
        break;
    case 6:
        stereo_layout = parameters::layout_top_bottom_half;
        stereo_layout_swap = false;
        break;
    case 7:
        stereo_layout = parameters::layout_top_bottom;
        stereo_layout_swap = true;
        break;
    case 8:
        stereo_layout = parameters::layout_top_bottom_half;
        stereo_layout_swap = true;
        break;
    case 9:
        stereo_layout = parameters::layout_left_right;
        stereo_layout_swap = false;
        break;
    case 10:
        stereo_layout = parameters::layout_left_right_half;
        stereo_layout_swap = false;
        break;
    case 11:
        stereo_layout = parameters::layout_left_right;
        stereo_layout_swap = true;
        break;
    case 12:
        stereo_layout = parameters::layout_left_right_half;
        stereo_layout_swap = true;
        break;
    case 13:
        stereo_layout = parameters::layout_even_odd_rows;
        stereo_layout_swap = false;
        break;
    case 14:
        stereo_layout = parameters::layout_even_odd_rows;
        stereo_layout_swap = true;
        break;
    }
}

void in_out_widget::get_stereo_mode(parameters::stereo_mode_t &stereo_mode, bool &stereo_mode_swap)
{
    switch (_output_combobox->currentIndex())
    {
    case 0:
        stereo_mode = parameters::mode_mono_left;
        break;
    case 1:
        stereo_mode = parameters::mode_mono_right;
        break;
    case 2:
        stereo_mode = parameters::mode_stereo;
        break;
    case 3:
        stereo_mode = parameters::mode_alternating;
        break;
    case 4:
        stereo_mode = parameters::mode_top_bottom;
        break;
    case 5:
        stereo_mode = parameters::mode_top_bottom_half;
        break;
    case 6:
        stereo_mode = parameters::mode_left_right;
        break;
    case 7:
        stereo_mode = parameters::mode_left_right_half;
        break;
    case 8:
        stereo_mode = parameters::mode_even_odd_rows;
        break;
    case 9:
        stereo_mode = parameters::mode_even_odd_columns;
        break;
    case 10:
        stereo_mode = parameters::mode_checkerboard;
        break;
    case 11:
        stereo_mode = parameters::mode_hdmi_frame_pack;
        break;
    case 12:
        stereo_mode = parameters::mode_red_cyan_monochrome;
        break;
    case 13:
        stereo_mode = parameters::mode_red_cyan_half_color;
        break;
    case 14:
        stereo_mode = parameters::mode_red_cyan_full_color;
        break;
    case 15:
        stereo_mode = parameters::mode_red_cyan_dubois;
        break;
    case 16:
        stereo_mode = parameters::mode_green_magenta_monochrome;
        break;
    case 17:
        stereo_mode = parameters::mode_green_magenta_half_color;
        break;
    case 18:
        stereo_mode = parameters::mode_green_magenta_full_color;
        break;
    case 19:
        stereo_mode = parameters::mode_green_magenta_dubois;
        break;
    case 20:
        stereo_mode = parameters::mode_amber_blue_monochrome;
        break;
    case 21:
        stereo_mode = parameters::mode_amber_blue_half_color;
        break;
    case 22:
        stereo_mode = parameters::mode_amber_blue_full_color;
        break;
    case 23:
        stereo_mode = parameters::mode_amber_blue_dubois;
        break;
    case 24:
        stereo_mode = parameters::mode_red_green_monochrome;
        break;
    case 25:
        stereo_mode = parameters::mode_red_blue_monochrome;
        break;
    }
    stereo_mode_swap = _swap_checkbox->isChecked();
}

void in_out_widget::receive_notification(const notification &note)
{
    _lock = true;
    switch (note.type)
    {
    case notification::open:
    case notification::play:
        update();
        break;
    case notification::video_stream:
        _video_combobox->setCurrentIndex(dispatch::parameters().video_stream());
        break;
    case notification::audio_stream:
        _audio_combobox->setCurrentIndex(dispatch::parameters().audio_stream());
        break;
    case notification::subtitle_stream:
        _subtitle_combobox->setCurrentIndex(dispatch::parameters().subtitle_stream() + 1);
        break;
    case notification::stereo_mode_swap:
        _swap_checkbox->setChecked(dispatch::parameters().stereo_mode_swap());
        break;
    default:
        break;
    }
    _lock = false;
}
