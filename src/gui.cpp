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

#include <limits>
#include <cmath>
#include <unistd.h>

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
#include <QCheckBox>
#include <QFontComboBox>
#include <QStandardItemModel>
#include <QTextBrowser>
#include <QTabWidget>
#include <QList>
#include <QTextCodec>
#include <QRegExp>
#include <QMap>
#include <QColorDialog>
#include <QDesktopWidget>
#include <QRadioButton>
#include <QRegExpValidator>
#include <QSettings>
#include <QGroupBox>
#include <QStackedWidget>
#include <QMimeData>

#include "gettext.h"
// Qt requires strings from gettext to be in UTF-8 encoding.
#define _(string) (str::convert(gettext(string), str::localcharset(), "UTF-8").c_str())

#include "gui.h"
#include "lib_versions.h"
#include "audio_output.h"
#include "media_input.h"
#include "video_output_qt.h"

#include "dbg.h"
#include "msg.h"
#include "str.h"

#if HAVE_LIBXNVCTRL
#  include "NVCtrl.h"
#  include "NvSDIutils.h"
#endif // HAVE_LIBXNVCTRL

// for autoconf < 2.65:
#ifndef PACKAGE_URL
#  define PACKAGE_URL "http://bino3d.org"
#endif

// Helper function: Get the icon with the given name from the icon theme.
// If unavailable, fall back to the built-in icon. Icon names conform to this specification:
// http://standards.freedesktop.org/icon-naming-spec/icon-naming-spec-latest.html
static QIcon get_icon(const QString &name)
{
    return QIcon::fromTheme(name, QIcon(QString(":icons/") + name));
}


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


controls_widget::controls_widget(QSettings *settings, QWidget *parent)
    : QWidget(parent), _lock(false), _settings(settings)
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


fullscreen_dialog::fullscreen_dialog(QWidget* parent) : QDialog(parent)
{
    const int screen_count = QApplication::desktop()->screenCount();

    /* Create the dialog. This dialog differs from other dialogs (e.g. the color
     * dialog) in that its settings only take effect when the dialog is closed,
     * and not immediately. This is necessary to allow the user full flexibility
     * in defining the screens used in fullscreen mode. */

    setModal(false);
    setWindowTitle(_("Fullscreen/Multiscreen Settings"));

    QLabel* lbl = new QLabel(_("Configure fullscreen mode:"));
    lbl->setToolTip(_("<p>Select the screens to use in fullscreen mode.</p>"));

    _single_btn = new QRadioButton(_("Single screen:"));
    _single_btn->setToolTip(_("<p>Use a single screen for fullscreen mode.</p>"));
    _single_box = new QComboBox();
    _single_box->setToolTip(_single_btn->toolTip());
    _single_box->addItem(_("Primary screen"));
    if (screen_count > 1) {
        for (int i = 0; i < screen_count; i++)
            _single_box->addItem(str::asprintf(_("Screen %d"), i + 1).c_str());
    }

    _dual_btn = new QRadioButton(_("Dual screen:"));
    _dual_btn->setToolTip(_("<p>Use two screens for fullscreen mode.</p>"));
    _dual_box0 = new QComboBox();
    _dual_box0->setToolTip(_dual_btn->toolTip());
    _dual_box1 = new QComboBox();
    _dual_box1->setToolTip(_dual_btn->toolTip());
    if (screen_count > 1) {
        for (int i = 0; i < screen_count; i++) {
            _dual_box0->addItem(str::asprintf(_("Screen %d"), i + 1).c_str());
            _dual_box1->addItem(str::asprintf(_("Screen %d"), i + 1).c_str());
        }
    }

    _multi_btn = new QRadioButton(_("Multi screen:"));
    _multi_btn->setToolTip(_("<p>Use multiple screens for fullscreen mode.</p>"));
    _multi_edt = new QLineEdit();
    _multi_edt->setToolTip(_("<p>Comma-separated list of screens to use for fullscreen mode.</p>"));
    _multi_edt->setValidator(new QRegExpValidator(QRegExp("\\d{1,2}(,\\d{1,2}){0,15}"), 0));

    QLabel* lbl2 = new QLabel(_("When in fullscreen mode,"));
    lbl2->setToolTip(_("<p>Set special left/right view handling for fullscreen mode.</p>"));

    _flip_left_box = new QCheckBox(_("flip left view vertically."));
    _flip_left_box->setToolTip(lbl2->toolTip());
    _flop_left_box = new QCheckBox(_("flop left view horizontally."));
    _flop_left_box->setToolTip(lbl2->toolTip());
    _flip_right_box = new QCheckBox(_("flip right view vertically."));
    _flip_right_box->setToolTip(lbl2->toolTip());
    _flop_right_box = new QCheckBox(_("flop right view horizontally."));
    _flop_right_box->setToolTip(lbl2->toolTip());

    _3d_ready_sync_box = new QCheckBox(_("use DLP(R) 3-D Ready Sync"));
    _3d_ready_sync_box->setToolTip(_("<p>Use DLP&reg; 3-D Ready Sync for supported output modes.</p>"));

    _inhibit_screensaver_box = new QCheckBox(_("inhibit the screensaver"));
    _inhibit_screensaver_box->setToolTip(_("<p>Inhibit the screensaver during fullscreen playback.</p>"));

    QPushButton* ok_btn = new QPushButton(_("OK"));
    connect(ok_btn, SIGNAL(clicked()), this, SLOT(close()));

    QGridLayout *layout0 = new QGridLayout();
    layout0->addWidget(lbl, 0, 0, 1, 3);
    layout0->addWidget(_single_btn, 1, 0);
    layout0->addWidget(_single_box, 1, 1, 1, 2);
    layout0->addWidget(_dual_btn, 2, 0);
    layout0->addWidget(_dual_box0, 2, 1);
    layout0->addWidget(_dual_box1, 2, 2);
    layout0->addWidget(_multi_btn, 3, 0);
    layout0->addWidget(_multi_edt, 3, 1, 1, 2);
    layout0->addWidget(lbl2, 4, 0, 1, 3);
    layout0->addWidget(_flip_left_box, 5, 0, 1, 3);
    layout0->addWidget(_flop_left_box, 6, 0, 1, 3);
    layout0->addWidget(_flip_right_box, 7, 0, 1, 3);
    layout0->addWidget(_flop_right_box, 8, 0, 1, 3);
    layout0->addWidget(_3d_ready_sync_box, 9, 0, 1, 3);
    layout0->addWidget(_inhibit_screensaver_box, 10, 0, 1, 3);
    QGridLayout *layout1 = new QGridLayout();
    layout1->addWidget(ok_btn, 0, 0);
    QGridLayout *layout = new QGridLayout();
    layout->addLayout(layout0, 0, 0);
    layout->addLayout(layout1, 1, 0);
    setLayout(layout);

    /* Fill in the current settings */

    if (screen_count < 3) {
        _multi_btn->setEnabled(false);
        _multi_edt->setEnabled(false);
    } else {
        _multi_edt->setText("1,2,3");
    }
    if (screen_count < 2) {
        _dual_btn->setEnabled(false);
        _dual_box0->setEnabled(false);
        _dual_box1->setEnabled(false);
    } else {
        _dual_box0->setCurrentIndex(0);
        _dual_box1->setCurrentIndex(1);
    }
    std::vector<int> conf_screens;
    for (int i = 0; i < 16; i++)
        if (dispatch::parameters().fullscreen_screens() & (1 << i))
            conf_screens.push_back(i);
    if (conf_screens.size() >= 3 && screen_count >= 3) {
        QString screen_list;
        for (size_t i = 0; i < conf_screens.size(); i++) {
            screen_list += str::from(conf_screens[i] + 1).c_str();
            if (i < conf_screens.size() - 1)
                screen_list += ',';
        }
        _multi_btn->setChecked(true);
        _multi_edt->setText(screen_list);
    } else if (conf_screens.size() == 2 && screen_count >= 2) {
        _dual_box0->setCurrentIndex(conf_screens[0]);
        _dual_box1->setCurrentIndex(conf_screens[1]);
        _dual_btn->setChecked(true);
    } else {
        if (conf_screens.size() > 0 && conf_screens[0] < screen_count)
            _single_box->setCurrentIndex(conf_screens[0] + 1);
        else
            _single_box->setCurrentIndex(0);
        _single_btn->setChecked(true);
    }
    _flip_left_box->setChecked(dispatch::parameters().fullscreen_flip_left());
    _flop_left_box->setChecked(dispatch::parameters().fullscreen_flop_left());
    _flip_right_box->setChecked(dispatch::parameters().fullscreen_flip_right());
    _flop_right_box->setChecked(dispatch::parameters().fullscreen_flop_right());
    _3d_ready_sync_box->setChecked(dispatch::parameters().fullscreen_3d_ready_sync());
#ifndef Q_OS_WIN
    _inhibit_screensaver_box->setChecked(dispatch::parameters().fullscreen_inhibit_screensaver());
#else
    _inhibit_screensaver_box->setChecked(false);
    _inhibit_screensaver_box->setEnabled(false);
#endif
}

void fullscreen_dialog::closeEvent(QCloseEvent* e)
{
    /* Activate the settings chosen in the dialog, then close. */

    // fullscreen_screens
    if (_single_btn->isChecked()) {
        if (_single_box->currentIndex() == 0)
            send_cmd(command::set_fullscreen_screens, 0);
        else
            send_cmd(command::set_fullscreen_screens, 1 << (_single_box->currentIndex() - 1));
    } else if (_dual_btn->isChecked()) {
        send_cmd(command::set_fullscreen_screens,
                (1 << _dual_box0->currentIndex()) | (1 << _dual_box1->currentIndex()));
    } else {
        int fss = 0;
        QStringList screens = _multi_edt->text().split(',', QString::SkipEmptyParts);
        for (int i = 0; i < screens.size(); i++) {
            int s = str::to<int>(screens[i].toLatin1().data());
            if (s >= 1 && s <= 16)
                fss |= (1 << (s - 1));
        }
        send_cmd(command::set_fullscreen_screens, fss);
    }
    // flip/flop settings
    send_cmd(command::set_fullscreen_flip_left, _flip_left_box->isChecked());
    send_cmd(command::set_fullscreen_flop_left, _flop_left_box->isChecked());
    send_cmd(command::set_fullscreen_flip_right, _flip_right_box->isChecked());
    send_cmd(command::set_fullscreen_flop_right, _flop_right_box->isChecked());
    // 3d ready sync
    send_cmd(command::set_fullscreen_3d_ready_sync, _3d_ready_sync_box->isChecked());
    // inhibit_screensaver
    send_cmd(command::set_fullscreen_inhibit_screensaver, _inhibit_screensaver_box->isChecked());

    e->accept();
}


color_dialog::color_dialog(QWidget *parent) : QDialog(parent), _lock(false)
{
    setModal(false);
    setWindowTitle(_("Display Color Adjustments"));

    QLabel *c_label = new QLabel(_("Contrast:"));
    _c_slider = new QSlider(Qt::Horizontal);
    _c_slider->setRange(-1000, 1000);
    _c_slider->setValue(dispatch::parameters().contrast() * 1000.0f);
    connect(_c_slider, SIGNAL(valueChanged(int)), this, SLOT(c_slider_changed(int)));
    _c_spinbox = new QDoubleSpinBox();
    _c_spinbox->setRange(-1.0, +1.0);
    _c_spinbox->setValue(dispatch::parameters().contrast());
    _c_spinbox->setDecimals(2);
    _c_spinbox->setSingleStep(0.01);
    connect(_c_spinbox, SIGNAL(valueChanged(double)), this, SLOT(c_spinbox_changed(double)));
    QLabel *b_label = new QLabel(_("Brightness:"));
    _b_slider = new QSlider(Qt::Horizontal);
    _b_slider->setRange(-1000, 1000);
    _b_slider->setValue(dispatch::parameters().brightness() * 1000.0f);
    connect(_b_slider, SIGNAL(valueChanged(int)), this, SLOT(b_slider_changed(int)));
    _b_spinbox = new QDoubleSpinBox();
    _b_spinbox->setRange(-1.0, +1.0);
    _b_spinbox->setValue(dispatch::parameters().brightness());
    _b_spinbox->setDecimals(2);
    _b_spinbox->setSingleStep(0.01);
    connect(_b_spinbox, SIGNAL(valueChanged(double)), this, SLOT(b_spinbox_changed(double)));
    QLabel *h_label = new QLabel(_("Hue:"));
    _h_slider = new QSlider(Qt::Horizontal);
    _h_slider->setRange(-1000, 1000);
    _h_slider->setValue(dispatch::parameters().hue() * 1000.0f);
    connect(_h_slider, SIGNAL(valueChanged(int)), this, SLOT(h_slider_changed(int)));
    _h_spinbox = new QDoubleSpinBox();
    _h_spinbox->setRange(-1.0, +1.0);
    _h_spinbox->setValue(dispatch::parameters().hue());
    _h_spinbox->setDecimals(2);
    _h_spinbox->setSingleStep(0.01);
    connect(_h_spinbox, SIGNAL(valueChanged(double)), this, SLOT(h_spinbox_changed(double)));
    QLabel *s_label = new QLabel(_("Saturation:"));
    _s_slider = new QSlider(Qt::Horizontal);
    _s_slider->setRange(-1000, 1000);
    _s_slider->setValue(dispatch::parameters().saturation() * 1000.0f);
    connect(_s_slider, SIGNAL(valueChanged(int)), this, SLOT(s_slider_changed(int)));
    _s_spinbox = new QDoubleSpinBox();
    _s_spinbox->setRange(-1.0, +1.0);
    _s_spinbox->setValue(dispatch::parameters().saturation());
    _s_spinbox->setDecimals(2);
    _s_spinbox->setSingleStep(0.01);
    connect(_s_spinbox, SIGNAL(valueChanged(double)), this, SLOT(s_spinbox_changed(double)));

    QPushButton *ok_button = new QPushButton(_("OK"));
    connect(ok_button, SIGNAL(clicked()), this, SLOT(close()));

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
    layout->addWidget(ok_button, 4, 0, 1, 3);
    setLayout(layout);
}

void color_dialog::c_slider_changed(int val)
{
    if (!_lock)
        send_cmd(command::set_contrast, val / 1000.0f);
}

void color_dialog::c_spinbox_changed(double val)
{
    if (!_lock)
        send_cmd(command::set_contrast, static_cast<float>(val));
}

void color_dialog::b_slider_changed(int val)
{
    if (!_lock)
        send_cmd(command::set_brightness, val / 1000.0f);
}

void color_dialog::b_spinbox_changed(double val)
{
    if (!_lock)
        send_cmd(command::set_brightness, static_cast<float>(val));
}

void color_dialog::h_slider_changed(int val)
{
    if (!_lock)
        send_cmd(command::set_hue, val / 1000.0f);
}

void color_dialog::h_spinbox_changed(double val)
{
    if (!_lock)
        send_cmd(command::set_hue, static_cast<float>(val));
}

void color_dialog::s_slider_changed(int val)
{
    if (!_lock)
        send_cmd(command::set_saturation, val / 1000.0f);
}

void color_dialog::s_spinbox_changed(double val)
{
    if (!_lock)
        send_cmd(command::set_saturation, static_cast<float>(val));
}

void color_dialog::receive_notification(const notification &note)
{
    switch (note.type)
    {
    case notification::contrast:
        _lock = true;
        _c_slider->setValue(dispatch::parameters().contrast() * 1000.0f);
        _c_spinbox->setValue(dispatch::parameters().contrast());
        _lock = false;
        break;
    case notification::brightness:
        _lock = true;
        _b_slider->setValue(dispatch::parameters().brightness() * 1000.0f);
        _b_spinbox->setValue(dispatch::parameters().brightness());
        _lock = false;
        break;
    case notification::hue:
        _lock = true;
        _h_slider->setValue(dispatch::parameters().hue() * 1000.0f);
        _h_spinbox->setValue(dispatch::parameters().hue());
        _lock = false;
        break;
    case notification::saturation:
        _lock = true;
        _s_slider->setValue(dispatch::parameters().saturation() * 1000.0f);
        _s_spinbox->setValue(dispatch::parameters().saturation());
        _lock = false;
        break;
    default:
        /* not handled */
        break;
    }
}


crosstalk_dialog::crosstalk_dialog(QWidget *parent) : QDialog(parent), _lock(false)
{
    setModal(false);
    setWindowTitle(_("Display Crosstalk Calibration"));

    /* TRANSLATORS: Please keep the lines short using <br> where necessary. */
    QLabel *rtfm_label = new QLabel(_("<p>Please read the manual to find out<br>"
                "how to measure the crosstalk levels<br>"
                "of your display.</p>"));

    QLabel *r_label = new QLabel(_("Red:"));
    _r_spinbox = new QDoubleSpinBox();
    _r_spinbox->setRange(0.0, +1.0);
    _r_spinbox->setValue(dispatch::parameters().crosstalk_r());
    _r_spinbox->setDecimals(2);
    _r_spinbox->setSingleStep(0.01);
    connect(_r_spinbox, SIGNAL(valueChanged(double)), this, SLOT(spinbox_changed()));
    QLabel *g_label = new QLabel(_("Green:"));
    _g_spinbox = new QDoubleSpinBox();
    _g_spinbox->setRange(0.0, +1.0);
    _g_spinbox->setValue(dispatch::parameters().crosstalk_g());
    _g_spinbox->setDecimals(2);
    _g_spinbox->setSingleStep(0.01);
    connect(_g_spinbox, SIGNAL(valueChanged(double)), this, SLOT(spinbox_changed()));
    QLabel *b_label = new QLabel(_("Blue:"));
    _b_spinbox = new QDoubleSpinBox();
    _b_spinbox->setRange(0.0, +1.0);
    _b_spinbox->setValue(dispatch::parameters().crosstalk_b());
    _b_spinbox->setDecimals(2);
    _b_spinbox->setSingleStep(0.01);
    connect(_b_spinbox, SIGNAL(valueChanged(double)), this, SLOT(spinbox_changed()));

    QPushButton *ok_button = new QPushButton(_("OK"));
    connect(ok_button, SIGNAL(clicked()), this, SLOT(close()));

    QGridLayout *layout = new QGridLayout;
    layout->addWidget(rtfm_label, 0, 0, 1, 2);
    layout->addWidget(r_label, 2, 0);
    layout->addWidget(_r_spinbox, 2, 1);
    layout->addWidget(g_label, 3, 0);
    layout->addWidget(_g_spinbox, 3, 1);
    layout->addWidget(b_label, 4, 0);
    layout->addWidget(_b_spinbox, 4, 1);
    layout->addWidget(ok_button, 5, 0, 1, 2);
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
    }
}

void crosstalk_dialog::receive_notification(const notification &note)
{
    switch (note.type)
    {
    case notification::crosstalk:
        _lock = true;
        _r_spinbox->setValue(dispatch::parameters().crosstalk_r());
        _g_spinbox->setValue(dispatch::parameters().crosstalk_g());
        _b_spinbox->setValue(dispatch::parameters().crosstalk_b());
        _lock = false;
        break;
    default:
        /* not handled */
        break;
    }
}


quality_dialog::quality_dialog(QWidget *parent) : QDialog(parent), _lock(false)
{
    setModal(false);
    setWindowTitle(_("Rendering Quality Adjustments"));

    QLabel *q_label = new QLabel(_("Rendering Quality:"));
    _q_slider = new QSlider(Qt::Horizontal);
    _q_slider->setRange(0, 4);
    _q_slider->setValue(dispatch::parameters().quality());
    connect(_q_slider, SIGNAL(valueChanged(int)), this, SLOT(q_slider_changed(int)));
    _q_spinbox = new QSpinBox();
    _q_spinbox->setRange(0, 4);
    _q_spinbox->setValue(dispatch::parameters().quality());
    _q_spinbox->setSingleStep(1);
    connect(_q_spinbox, SIGNAL(valueChanged(int)), this, SLOT(q_spinbox_changed(int)));

    QPushButton *ok_button = new QPushButton(_("OK"));
    connect(ok_button, SIGNAL(clicked()), this, SLOT(close()));

    QGridLayout *layout = new QGridLayout;
    layout->addWidget(q_label, 0, 0);
    layout->addWidget(_q_slider, 0, 1);
    layout->addWidget(_q_spinbox, 0, 2);
    layout->addWidget(ok_button, 1, 0, 1, 3);
    setLayout(layout);
}

void quality_dialog::q_slider_changed(int val)
{
    if (!_lock)
        send_cmd(command::set_quality, val);
}

void quality_dialog::q_spinbox_changed(int val)
{
    if (!_lock)
        send_cmd(command::set_quality, val);
}

void quality_dialog::receive_notification(const notification &note)
{
    switch (note.type)
    {
    case notification::quality:
        _lock = true;
        _q_slider->setValue(dispatch::parameters().quality());
        _q_spinbox->setValue(dispatch::parameters().quality());
        _lock = false;
        break;
    default:
        /* not handled */
        break;
    }
}


zoom_dialog::zoom_dialog(QWidget *parent) : QDialog(parent), _lock(false)
{
    setModal(false);
    setWindowTitle(_("Zoom for wide videos"));

    QLabel *info_label = new QLabel(_(
                "<p>Set zoom level for videos that<br>"
                "are wider than the screen:<br>"
                "0: Show full video width.<br>"
                "1: Use full screen height.</p>"));
    QLabel *z_label = new QLabel(_("Zoom:"));
    z_label->setToolTip(_("<p>Set the zoom level for videos that are wider than the screen.</p>"));
    _z_slider = new QSlider(Qt::Horizontal);
    _z_slider->setRange(0, 1000);
    _z_slider->setValue(dispatch::parameters().zoom() * 1000.0f);
    _z_slider->setToolTip(z_label->toolTip());
    connect(_z_slider, SIGNAL(valueChanged(int)), this, SLOT(z_slider_changed(int)));
    _z_spinbox = new QDoubleSpinBox();
    _z_spinbox->setRange(0.0, 1.0);
    _z_spinbox->setValue(dispatch::parameters().zoom());
    _z_spinbox->setDecimals(2);
    _z_spinbox->setSingleStep(0.01);
    _z_spinbox->setToolTip(z_label->toolTip());
    connect(_z_spinbox, SIGNAL(valueChanged(double)), this, SLOT(z_spinbox_changed(double)));

    QPushButton *ok_button = new QPushButton(_("OK"));
    connect(ok_button, SIGNAL(clicked()), this, SLOT(close()));

    QGridLayout *layout = new QGridLayout;
    layout->addWidget(info_label, 0, 0, 1, 3);
    layout->addWidget(z_label, 1, 0);
    layout->addWidget(_z_slider, 1, 1);
    layout->addWidget(_z_spinbox, 1, 2);
    layout->addWidget(ok_button, 2, 0, 1, 3);
    setLayout(layout);
}

void zoom_dialog::z_slider_changed(int val)
{
    if (!_lock)
        send_cmd(command::set_zoom, val / 1000.0f);
}

void zoom_dialog::z_spinbox_changed(double val)
{
    if (!_lock)
        send_cmd(command::set_zoom, static_cast<float>(val));
}

void zoom_dialog::receive_notification(const notification &note)
{
    switch (note.type)
    {
    case notification::zoom:
        _lock = true;
        _z_slider->setValue(dispatch::parameters().zoom() * 1000.0f);
        _z_spinbox->setValue(dispatch::parameters().zoom());
        _lock = false;
        break;
    default:
        /* not handled */
        break;
    }
}


audio_dialog::audio_dialog(QWidget *parent) : QDialog(parent), _lock(false)
{
    setModal(false);
    setWindowTitle(_("Audio Settings"));

    QLabel *device_label = new QLabel(_("Audio device:"));
    device_label->setToolTip(_("<p>Select the audio device.<br>"
                "This will take effect for the next started video.</p>"));
    _device_combobox = new QComboBox();
    _device_combobox->setToolTip(device_label->toolTip());
    _device_combobox->addItem(_("Default"));
    audio_output ao;
    for (int i = 0; i < ao.devices(); i++)
    {
        _device_combobox->addItem(ao.device_name(i).c_str());
    }
    connect(_device_combobox, SIGNAL(currentIndexChanged(int)), this, SLOT(device_changed()));
    if (dispatch::parameters().audio_device() < 0
            || dispatch::parameters().audio_device() >= _device_combobox->count())
    {
        _device_combobox->setCurrentIndex(0);
    }
    else
    {
        _device_combobox->setCurrentIndex(dispatch::parameters().audio_device() + 1);
    }

    QLabel *delay_label = new QLabel(_("Audio delay (ms):"));
    delay_label->setToolTip(_("<p>Set an audio delay, in milliseconds.<br>"
                "This is useful if audio and video are not in sync.</p>"));
    _delay_spinbox = new QSpinBox();
    _delay_spinbox->setToolTip(delay_label->toolTip());
    _delay_spinbox->setRange(-10000, +10000);
    _delay_spinbox->setSingleStep(1);
    _delay_spinbox->setValue(dispatch::parameters().audio_delay() / 1000);
    connect(_delay_spinbox, SIGNAL(valueChanged(int)), this, SLOT(delay_changed()));

    QPushButton *ok_button = new QPushButton(_("OK"));
    connect(ok_button, SIGNAL(clicked()), this, SLOT(close()));

    QGridLayout *layout = new QGridLayout;
    layout->addWidget(device_label, 0, 0);
    layout->addWidget(_device_combobox, 0, 1);
    layout->addWidget(delay_label, 1, 0);
    layout->addWidget(_delay_spinbox, 1, 1);
    layout->addWidget(ok_button, 2, 0, 1, 2);
    setLayout(layout);
}

void audio_dialog::device_changed()
{
    if (!_lock)
        send_cmd(command::set_audio_device, _device_combobox->currentIndex() - 1);
}

void audio_dialog::delay_changed()
{
    if (!_lock)
        send_cmd(command::set_audio_delay, static_cast<int64_t>(_delay_spinbox->value() * 1000));
}

void audio_dialog::receive_notification(const notification &note)
{
    switch (note.type)
    {
    case notification::audio_device:
        _lock = true;
        _device_combobox->setCurrentIndex(dispatch::parameters().audio_device() + 1);
        _lock = false;
        break;
    case notification::audio_delay:
        _lock = true;
        _delay_spinbox->setValue(dispatch::parameters().audio_delay() / 1000);
        _lock = false;
        break;
    default:
        /* not handled */
        break;
    }
}


subtitle_dialog::subtitle_dialog(QWidget *parent) : QDialog(parent), _lock(false)
{
    setModal(false);
    setWindowTitle(_("Subtitle Settings"));

    QLabel *info_label = new QLabel(_(
                "<p>These settings apply only to soft subtitles, not to bitmaps.<br>"
                "Some changes require a restart of the video to take effect.</p>"));

    _encoding_checkbox = new QCheckBox(_("Encoding:"));
    _encoding_checkbox->setToolTip(_("<p>Set the subtitle character set encoding.</p>"));
    _encoding_checkbox->setChecked(!dispatch::parameters().subtitle_encoding_is_default());
    connect(_encoding_checkbox, SIGNAL(stateChanged(int)), this, SLOT(encoding_changed()));
    _encoding_combobox = new QComboBox();
    _encoding_combobox->setToolTip(_encoding_checkbox->toolTip());
    QList<QTextCodec *> codecs = find_codecs();
    foreach (QTextCodec *codec, codecs)
    {
        _encoding_combobox->addItem(codec->name(), codec->mibEnum());
    }
    _encoding_combobox->setCurrentIndex(_encoding_combobox->findText(
                dispatch::parameters().subtitle_encoding() == ""
                ? "UTF-8" : dispatch::parameters().subtitle_encoding().c_str()));
    connect(_encoding_combobox, SIGNAL(currentIndexChanged(int)), this, SLOT(encoding_changed()));

    _font_checkbox = new QCheckBox(_("Override font:"));
    _font_checkbox->setToolTip(_("<p>Override the subtitle font family.</p>"));
    _font_checkbox->setChecked(!dispatch::parameters().subtitle_font_is_default());
    connect(_font_checkbox, SIGNAL(stateChanged(int)), this, SLOT(font_changed()));
    _font_combobox = new QFontComboBox();
    _font_combobox->setToolTip(_font_checkbox->toolTip());
    _font_combobox->setCurrentFont(QFont(QString(dispatch::parameters().subtitle_font() == ""
                    ? "sans-serif" : dispatch::parameters().subtitle_font().c_str())));
    connect(_font_combobox, SIGNAL(currentFontChanged(const QFont &)), this, SLOT(font_changed()));

    _size_checkbox = new QCheckBox(_("Override font size:"));
    _size_checkbox->setToolTip(_("<p>Override the subtitle font size.</p>"));
    _size_checkbox->setChecked(dispatch::parameters().subtitle_size() > 0);
    connect(_size_checkbox, SIGNAL(stateChanged(int)), this, SLOT(size_changed()));
    _size_spinbox = new QSpinBox();
    _size_spinbox->setToolTip(_size_checkbox->toolTip());
    _size_spinbox->setRange(1, 999);
    _size_spinbox->setValue(dispatch::parameters().subtitle_size() <= 0
            ? 12 : dispatch::parameters().subtitle_size());
    connect(_size_spinbox, SIGNAL(valueChanged(int)), this, SLOT(size_changed()));

    _scale_checkbox = new QCheckBox(_("Override scale factor:"));
    _scale_checkbox->setToolTip(_("<p>Override the subtitle scale factor.</p>"));
    _scale_checkbox->setChecked(dispatch::parameters().subtitle_scale() >= 0.0f);
    connect(_scale_checkbox, SIGNAL(stateChanged(int)), this, SLOT(scale_changed()));
    _scale_spinbox = new QDoubleSpinBox();
    _scale_spinbox->setToolTip(_scale_checkbox->toolTip());
    _scale_spinbox->setRange(0.01, 100.0);
    _scale_spinbox->setSingleStep(0.1);
    _scale_spinbox->setValue(dispatch::parameters().subtitle_scale() < 0.0f
            ? 1.0f : dispatch::parameters().subtitle_scale());
    connect(_scale_spinbox, SIGNAL(valueChanged(double)), this, SLOT(scale_changed()));

    _color_checkbox = new QCheckBox(_("Override color:"));
    _color_checkbox->setToolTip(_("<p>Override the subtitle color.</p>"));
    _color_checkbox->setChecked(dispatch::parameters().subtitle_color() <= std::numeric_limits<uint32_t>::max());
    connect(_color_checkbox, SIGNAL(stateChanged(int)), this, SLOT(color_changed()));
    _color_button = new QPushButton();
    _color_button->setToolTip(_color_checkbox->toolTip());
    set_color_button(dispatch::parameters().subtitle_color() > std::numeric_limits<uint32_t>::max()
            ? 0xffffffu : dispatch::parameters().subtitle_color());
    connect(_color_button, SIGNAL(clicked()), this, SLOT(color_button_clicked()));

    _shadow_checkbox = new QCheckBox(_("Override shadow:"));
    _shadow_checkbox->setToolTip(_("<p>Override the subtitle shadow.</p>"));
    _shadow_checkbox->setChecked(dispatch::parameters().subtitle_shadow() != -1);
    connect(_shadow_checkbox, SIGNAL(stateChanged(int)), this, SLOT(shadow_changed()));
    _shadow_combobox = new QComboBox();
    _shadow_combobox->setToolTip(_shadow_checkbox->toolTip());
    _shadow_combobox->addItem(_("On"));
    _shadow_combobox->addItem(_("Off"));
    _shadow_combobox->setCurrentIndex(dispatch::parameters().subtitle_shadow() == 0 ? 1 : 0);
    connect(_shadow_combobox, SIGNAL(currentIndexChanged(int)), this, SLOT(shadow_changed()));

    QPushButton *ok_button = new QPushButton(_("OK"));
    connect(ok_button, SIGNAL(clicked()), this, SLOT(close()));

    QGridLayout *layout = new QGridLayout;
    layout->addWidget(info_label, 0, 0, 1, 2);
    layout->addWidget(_encoding_checkbox, 1, 0);
    layout->addWidget(_encoding_combobox, 1, 1);
    layout->addWidget(_font_checkbox, 2, 0);
    layout->addWidget(_font_combobox, 2, 1);
    layout->addWidget(_size_checkbox, 3, 0);
    layout->addWidget(_size_spinbox, 3, 1);
    layout->addWidget(_scale_checkbox, 4, 0);
    layout->addWidget(_scale_spinbox, 4, 1);
    layout->addWidget(_color_checkbox, 5, 0);
    layout->addWidget(_color_button, 5, 1);
    layout->addWidget(_shadow_checkbox, 6, 0);
    layout->addWidget(_shadow_combobox, 6, 1);
    layout->addWidget(ok_button, 7, 0, 1, 2);
    setLayout(layout);
}

QList<QTextCodec *> subtitle_dialog::find_codecs()
{
    QMap<QString, QTextCodec *> codec_map;
    QRegExp iso8859_regexp("ISO[- ]8859-([0-9]+).*");

    foreach (int mib, QTextCodec::availableMibs())
    {
        QTextCodec *codec = QTextCodec::codecForMib(mib);

        QString sort_key = codec->name().toUpper();
        int rank;

        if (sort_key.startsWith("UTF-8"))
        {
            rank = 1;
        }
        else if (sort_key.startsWith("UTF-16"))
        {
            rank = 2;
        }
        else if (iso8859_regexp.exactMatch(sort_key))
        {
            if (iso8859_regexp.cap(1).size() == 1)
            {
                rank = 3;
            }
            else
            {
                rank = 4;
            }
        }
        else
        {
            rank = 5;
        }
        sort_key.prepend(QChar('0' + rank));

        codec_map.insert(sort_key, codec);
    }
    return codec_map.values();
}

void subtitle_dialog::set_color_button(uint32_t c)
{
    int r = (c >> 16u) & 0xffu;
    int g = (c >> 8u) & 0xffu;
    int b = c & 0xffu;
    _color = QColor(r, g, b);
    QPixmap pm(64, 64);
    pm.fill(_color);
    _color_button->setIcon(QIcon(pm));
}

void subtitle_dialog::color_button_clicked()
{
    _color = QColorDialog::getColor(_color, this);
    QPixmap pm(64, 64);
    pm.fill(_color);
    _color_button->setIcon(QIcon(pm));
    color_changed();
}

void subtitle_dialog::encoding_changed()
{
    if (!_lock)
    {
        std::string encoding = _encoding_checkbox->isChecked()
            ? _encoding_combobox->currentText().toStdString() : "";
        std::ostringstream v;
        s11n::save(v, encoding);
        send_cmd(command::set_subtitle_encoding, v.str());
    }
}

void subtitle_dialog::font_changed()
{
    if (!_lock)
    {
        std::string font = _font_checkbox->isChecked()
            ? _font_combobox->currentFont().family().toLocal8Bit().constData()
            : "";
        std::ostringstream v;
        s11n::save(v, font);
        send_cmd(command::set_subtitle_font, v.str());
    }
}

void subtitle_dialog::size_changed()
{
    if (!_lock)
        send_cmd(command::set_subtitle_size, _size_checkbox->isChecked() ? _size_spinbox->value() : -1);
}

void subtitle_dialog::scale_changed()
{
    if (!_lock)
        send_cmd(command::set_subtitle_scale, _scale_checkbox->isChecked()
                ? static_cast<float>(_scale_spinbox->value()) : -1.0f);
}

void subtitle_dialog::color_changed()
{
    if (!_lock)
    {
        uint64_t color;
        if (_color_checkbox->isChecked())
        {
            unsigned int a = _color.alpha();
            unsigned int r = _color.red();
            unsigned int g = _color.green();
            unsigned int b = _color.blue();
            color = (a << 24u) | (r << 16u) | (g << 8u) | b;
        }
        else
        {
            color = std::numeric_limits<uint64_t>::max();
        }
        send_cmd(command::set_subtitle_color, color);
    }
}

void subtitle_dialog::shadow_changed()
{
    if (!_lock)
        send_cmd(command::set_subtitle_shadow, _shadow_checkbox->isChecked()
                ? 1 - _shadow_combobox->currentIndex() : -1);
}

void subtitle_dialog::receive_notification(const notification &note)
{
    std::string s;
    int i;
    float f;
    uint64_t c;

    _lock = true;
    switch (note.type)
    {
    case notification::subtitle_encoding:
        s = dispatch::parameters().subtitle_encoding();
        _encoding_checkbox->setChecked(!s.empty());
        if (!s.empty())
            _encoding_combobox->setCurrentIndex(_encoding_combobox->findText(s.c_str()));
        break;
    case notification::subtitle_font:
        s = dispatch::parameters().subtitle_font();
        _font_checkbox->setChecked(!s.empty());
        if (!s.empty())
            _font_combobox->setCurrentFont(QFont(QString(s.c_str())));
        break;
    case notification::subtitle_size:
        i = dispatch::parameters().subtitle_size();
        _size_checkbox->setChecked(i > 0);
        if (i > 0)
            _size_spinbox->setValue(i);
        break;
    case notification::subtitle_scale:
        f = dispatch::parameters().subtitle_scale();
        _scale_checkbox->setChecked(f >= 0.0f);
        if (f >= 0.0f)
            _scale_spinbox->setValue(f);
        break;
    case notification::subtitle_color:
        c = dispatch::parameters().subtitle_color();
        _color_checkbox->setChecked(c <= std::numeric_limits<uint32_t>::max());
        if (c <= std::numeric_limits<uint32_t>::max())
            set_color_button(c);
        break;
    case notification::subtitle_shadow:
        i = dispatch::parameters().subtitle_shadow();
        _shadow_checkbox->setChecked(i >= 0);
        if (i >= 0)
            _shadow_combobox->setCurrentIndex(1 - i);
        break;
    default:
        /* not handled */
        break;
    }
    _lock = false;
}


video_dialog::video_dialog(QWidget *parent) : QDialog(parent), _lock(false)
{
    setModal(false);
    setWindowTitle(_("Video Settings"));

    QLabel *crop_ar_label = new QLabel(_("Crop to aspect ratio:"));
    crop_ar_label->setToolTip(_("<p>Set the real aspect ratio of the video, so that borders can be cropped.</p>"));
    _crop_ar_combobox = new QComboBox();
    _crop_ar_combobox->setToolTip(crop_ar_label->toolTip());
    _crop_ar_combobox->addItem(_("Do not crop"));
    _crop_ar_combobox->addItem("16:10");
    _crop_ar_combobox->addItem("16:9");
    _crop_ar_combobox->addItem("1.85:1");
    _crop_ar_combobox->addItem("2.21:1");
    _crop_ar_combobox->addItem("2.35:1");
    _crop_ar_combobox->addItem("2.39:1");
    _crop_ar_combobox->addItem("5:3");
    _crop_ar_combobox->addItem("4:3");
    _crop_ar_combobox->addItem("5:4");
    _crop_ar_combobox->addItem("1:1");
    connect(_crop_ar_combobox, SIGNAL(currentIndexChanged(int)), this, SLOT(crop_ar_changed()));

    QLabel *source_ar_label = new QLabel(_("Force source aspect ratio:"));
    source_ar_label->setToolTip(_("<p>Force the aspect ratio of video source.</p>"));
    _source_ar_combobox = new QComboBox();
    _source_ar_combobox->setToolTip(source_ar_label->toolTip());
    _source_ar_combobox->addItem(_("Do not force"));
    _source_ar_combobox->addItem("16:10");
    _source_ar_combobox->addItem("16:9");
    _source_ar_combobox->addItem("1.85:1");
    _source_ar_combobox->addItem("2.21:1");
    _source_ar_combobox->addItem("2.35:1");
    _source_ar_combobox->addItem("2.39:1");
    _source_ar_combobox->addItem("5:3");
    _source_ar_combobox->addItem("4:3");
    _source_ar_combobox->addItem("5:4");
    _source_ar_combobox->addItem("1:1");
    connect(_source_ar_combobox, SIGNAL(currentIndexChanged(int)), this, SLOT(source_ar_changed()));

    QLabel *p_label = new QLabel(_("Parallax:"));
    p_label->setToolTip(_("<p>Adjust parallax, from -1 to +1. This changes the separation of left and right view, "
                "and thus the perceived distance of the scene.</p>"));
    _p_slider = new QSlider(Qt::Horizontal);
    _p_slider->setToolTip(p_label->toolTip());
    _p_slider->setRange(-1000, 1000);
    connect(_p_slider, SIGNAL(valueChanged(int)), this, SLOT(p_slider_changed(int)));
    _p_spinbox = new QDoubleSpinBox();
    _p_spinbox->setToolTip(p_label->toolTip());
    _p_spinbox->setRange(-1.0, +1.0);
    _p_spinbox->setDecimals(2);
    _p_spinbox->setSingleStep(0.01);
    connect(_p_spinbox, SIGNAL(valueChanged(double)), this, SLOT(p_spinbox_changed(double)));

    QLabel *sp_label = new QLabel(_("Subtitle parallax:"));
    sp_label->setToolTip(_("<p>Adjust subtitle parallax, from -1 to +1. This changes the perceived distance "
                "of the subtitles.</p>"));
    _sp_slider = new QSlider(Qt::Horizontal);
    _sp_slider->setToolTip(sp_label->toolTip());
    _sp_slider->setRange(-1000, 1000);
    connect(_sp_slider, SIGNAL(valueChanged(int)), this, SLOT(sp_slider_changed(int)));
    _sp_spinbox = new QDoubleSpinBox();
    _sp_spinbox->setToolTip(sp_label->toolTip());
    _sp_spinbox->setRange(-1.0, +1.0);
    _sp_spinbox->setDecimals(2);
    _sp_spinbox->setSingleStep(0.01);
    connect(_sp_spinbox, SIGNAL(valueChanged(double)), this, SLOT(sp_spinbox_changed(double)));

    QLabel *g_label = new QLabel(_("Ghostbusting:"));
    g_label->setToolTip(_("<p>Set the amount of crosstalk ghostbusting, from 0 to 1. "
                "You need to set the crosstalk levels of your display first. "
                "Note that crosstalk ghostbusting does not work with anaglyph glasses.</p>"));
    _g_slider = new QSlider(Qt::Horizontal);
    _g_slider->setToolTip(g_label->toolTip());
    _g_slider->setRange(0, 1000);
    connect(_g_slider, SIGNAL(valueChanged(int)), this, SLOT(g_slider_changed(int)));
    _g_spinbox = new QDoubleSpinBox();
    _g_spinbox->setToolTip(g_label->toolTip());
    _g_spinbox->setRange(0.0, +1.0);
    _g_spinbox->setDecimals(2);
    _g_spinbox->setSingleStep(0.01);
    connect(_g_spinbox, SIGNAL(valueChanged(double)), this, SLOT(g_spinbox_changed(double)));

    QPushButton *ok_button = new QPushButton(_("OK"));
    connect(ok_button, SIGNAL(clicked()), this, SLOT(close()));

    QGridLayout *layout = new QGridLayout;
    layout->addWidget(crop_ar_label, 0, 0);
    layout->addWidget(_crop_ar_combobox, 0, 1, 1, 2);
    layout->addWidget(source_ar_label, 1, 0);
    layout->addWidget(_source_ar_combobox, 1, 1, 1, 2);
    layout->addWidget(p_label, 2, 0);
    layout->addWidget(_p_slider, 2, 1);
    layout->addWidget(_p_spinbox, 2, 2);
    layout->addWidget(sp_label, 3, 0);
    layout->addWidget(_sp_slider, 3, 1);
    layout->addWidget(_sp_spinbox, 3, 2);
    layout->addWidget(g_label, 4, 0);
    layout->addWidget(_g_slider, 4, 1);
    layout->addWidget(_g_spinbox, 4, 2);
    layout->addWidget(ok_button, 5, 0, 1, 3);
    setLayout(layout);

    update();
}

void video_dialog::update()
{
    _lock = true;
    set_crop_ar(dispatch::parameters().crop_aspect_ratio());
    _p_slider->setValue(dispatch::parameters().parallax() * 1000.0f);
    _p_spinbox->setValue(dispatch::parameters().parallax());
    _sp_slider->setValue(dispatch::parameters().subtitle_parallax() * 1000.0f);
    _sp_spinbox->setValue(dispatch::parameters().subtitle_parallax());
    _g_slider->setValue(dispatch::parameters().ghostbust() * 1000.0f);
    _g_spinbox->setValue(dispatch::parameters().ghostbust());
    _lock = false;
}

void video_dialog::set_crop_ar(float value)
{
    _crop_ar_combobox->setCurrentIndex(
              ( std::abs(value - 16.0f / 10.0f) < 0.01f ? 1
              : std::abs(value - 16.0f / 9.0f)  < 0.01f ? 2
              : std::abs(value - 1.85f)         < 0.01f ? 3
              : std::abs(value - 2.21f)         < 0.01f ? 4
              : std::abs(value - 2.35f)         < 0.01f ? 5
              : std::abs(value - 2.39f)         < 0.01f ? 6
              : std::abs(value - 5.0f / 3.0f)   < 0.01f ? 7
              : std::abs(value - 4.0f / 3.0f)   < 0.01f ? 8
              : std::abs(value - 5.0f / 4.0f)   < 0.01f ? 9
              : std::abs(value - 1.0f)          < 0.01f ? 10
              : 0));
}

void video_dialog::crop_ar_changed()
{
    if (!_lock)
    {
        int i = _crop_ar_combobox->currentIndex();
        float ar =
              i == 1 ? 16.0f / 10.0f
            : i == 2 ? 16.0f / 9.0f
            : i == 3 ? 1.85f
            : i == 4 ? 2.21f
            : i == 5 ? 2.35f
            : i == 6 ? 2.39f
            : i == 7 ? 5.0f / 3.0f
            : i == 8 ? 4.0f / 3.0f
            : i == 9 ? 5.0f / 4.0f
            : i == 10 ? 1.0f
            : 0.0f;
        send_cmd(command::set_crop_aspect_ratio, ar);
    }
}

void video_dialog::set_source_ar(float value)
{
    _source_ar_combobox->setCurrentIndex(
              ( std::abs(value - 16.0f / 10.0f) < 0.01f ? 1
              : std::abs(value - 16.0f / 9.0f)  < 0.01f ? 2
              : std::abs(value - 1.85f)         < 0.01f ? 3
              : std::abs(value - 2.21f)         < 0.01f ? 4
              : std::abs(value - 2.35f)         < 0.01f ? 5
              : std::abs(value - 2.39f)         < 0.01f ? 6
              : std::abs(value - 5.0f / 3.0f)   < 0.01f ? 7
              : std::abs(value - 4.0f / 3.0f)   < 0.01f ? 8
              : std::abs(value - 5.0f / 4.0f)   < 0.01f ? 9
              : std::abs(value - 1.0f)          < 0.01f ? 10
              : 0));
}

void video_dialog::source_ar_changed()
{
    if (!_lock)
    {
        int i = _source_ar_combobox->currentIndex();
        float ar =
              i == 1 ? 16.0f / 10.0f
            : i == 2 ? 16.0f / 9.0f
            : i == 3 ? 1.85f
            : i == 4 ? 2.21f
            : i == 5 ? 2.35f
            : i == 6 ? 2.39f
            : i == 7 ? 5.0f / 3.0f
            : i == 8 ? 4.0f / 3.0f
            : i == 9 ? 5.0f / 4.0f
            : i == 10 ? 1.0f
            : 0.0f;
        send_cmd(command::set_source_aspect_ratio, ar);
    }
}

void video_dialog::p_slider_changed(int val)
{
    if (!_lock)
        send_cmd(command::set_parallax, val / 1000.0f);
}

void video_dialog::p_spinbox_changed(double val)
{
    if (!_lock)
        send_cmd(command::set_parallax, static_cast<float>(val));
}

void video_dialog::sp_slider_changed(int val)
{
    if (!_lock)
        send_cmd(command::set_subtitle_parallax, val / 1000.0f);
}

void video_dialog::sp_spinbox_changed(double val)
{
    if (!_lock)
        send_cmd(command::set_subtitle_parallax, static_cast<float>(val));
}

void video_dialog::g_slider_changed(int val)
{
    if (!_lock)
        send_cmd(command::set_ghostbust, val / 1000.0f);
}

void video_dialog::g_spinbox_changed(double val)
{
    if (!_lock)
        send_cmd(command::set_ghostbust, static_cast<float>(val));
}

void video_dialog::receive_notification(const notification &note)
{
    switch (note.type)
    {
    case notification::crop_aspect_ratio:
    case notification::source_aspect_ratio:
    case notification::parallax:
    case notification::subtitle_parallax:
    case notification::ghostbust:
        update();
        break;
    default:
        /* not handled */
        break;
    }
}


sdi_output_dialog::sdi_output_dialog(QWidget *parent) : QDialog(parent),
    _lock(false)
{
#if HAVE_LIBXNVCTRL

    setModal(false);
    setWindowTitle(_("SDI Output Settings"));

    QLabel *sdi_output_format_label = new QLabel(_("SDI Output Format:"));
    sdi_output_format_label->setToolTip(_("<p>Select output format used for SDI output.</p>"));
    _sdi_output_format_combobox = new QComboBox();
    _sdi_output_format_combobox->setToolTip(sdi_output_format_label->toolTip());
    for (int i=NV_CTRL_GVIO_VIDEO_FORMAT_487I_59_94_SMPTE259_NTSC; i < NV_CTRL_GVIO_VIDEO_FORMAT_2048I_47_96_SMPTE372+1; ++i) {
        _sdi_output_format_combobox->addItem(decodeSignalFormat(i));
    }
    connect(_sdi_output_format_combobox, SIGNAL(currentIndexChanged(int)), this, SLOT(sdi_output_format_changed(int)));

    QLabel *sdi_output_left_stereo_mode_label = new QLabel(_("Left stereo mode:"));
    sdi_output_left_stereo_mode_label->setToolTip(_("<p>Select stereo mode used for left SDI output.</p>"));
    _sdi_output_left_stereo_mode_combobox = new QComboBox();
    _sdi_output_left_stereo_mode_combobox->setToolTip(sdi_output_left_stereo_mode_label->toolTip());
    _sdi_output_left_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-mono-left.png"), _("Left view"));
    _sdi_output_left_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-mono-right.png"), _("Right view"));
    _sdi_output_left_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-top-bottom.png"), _("Top/bottom"));
    _sdi_output_left_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-top-bottom-half.png"), _("Top/bottom, half height"));
    _sdi_output_left_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-left-right.png"), _("Left/right"));
    _sdi_output_left_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-left-right-half.png"), _("Left/right, half width"));
    _sdi_output_left_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-even-odd-rows.png"), _("Even/odd rows"));
    _sdi_output_left_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-even-odd-columns.png"), _("Even/odd columns"));
    _sdi_output_left_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-checkerboard.png"), _("Checkerboard pattern"));
    _sdi_output_left_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-red-cyan.png"), _("Red/cyan glasses, monochrome"));
    _sdi_output_left_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-red-cyan.png"), _("Red/cyan glasses, half color"));
    _sdi_output_left_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-red-cyan.png"), _("Red/cyan glasses, full color"));
    _sdi_output_left_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-red-cyan.png"), _("Red/cyan glasses, Dubois"));
    _sdi_output_left_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-green-magenta.png"), _("Green/magenta glasses, monochrome"));
    _sdi_output_left_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-green-magenta.png"), _("Green/magenta glasses, half color"));
    _sdi_output_left_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-green-magenta.png"), _("Green/magenta glasses, full color"));
    _sdi_output_left_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-green-magenta.png"), _("Green/magenta glasses, Dubois"));
    _sdi_output_left_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-amber-blue.png"), _("Amber/blue glasses, monochrome"));
    _sdi_output_left_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-amber-blue.png"), _("Amber/blue glasses, half color"));
    _sdi_output_left_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-amber-blue.png"), _("Amber/blue glasses, full color"));
    _sdi_output_left_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-amber-blue.png"), _("Amber/blue glasses, Dubois"));
    _sdi_output_left_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-red-green.png"), _("Red/green glasses, monochrome"));
    _sdi_output_left_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-red-blue.png"), _("Red/blue glasses, monochrome"));
    connect(_sdi_output_left_stereo_mode_combobox, SIGNAL(currentIndexChanged(int)), this, SLOT(sdi_output_left_stereo_mode_changed(int)));

    QLabel *sdi_output_right_stereo_mode_label = new QLabel(_("Right stereo mode:"));
    sdi_output_right_stereo_mode_label->setToolTip(_("<p>Select stereo mode used for right SDI output.</p>"));
    _sdi_output_right_stereo_mode_combobox = new QComboBox();
    _sdi_output_right_stereo_mode_combobox->setToolTip(sdi_output_right_stereo_mode_label->toolTip());
    _sdi_output_right_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-mono-left.png"), _("Left view"));
    _sdi_output_right_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-mono-right.png"), _("Right view"));
    _sdi_output_right_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-top-bottom.png"), _("Top/bottom"));
    _sdi_output_right_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-top-bottom-half.png"), _("Top/bottom, half height"));
    _sdi_output_right_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-left-right.png"), _("Left/right"));
    _sdi_output_right_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-left-right-half.png"), _("Left/right, half width"));
    _sdi_output_right_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-even-odd-rows.png"), _("Even/odd rows"));
    _sdi_output_right_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-even-odd-columns.png"), _("Even/odd columns"));
    _sdi_output_right_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-checkerboard.png"), _("Checkerboard pattern"));
    _sdi_output_right_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-red-cyan.png"), _("Red/cyan glasses, monochrome"));
    _sdi_output_right_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-red-cyan.png"), _("Red/cyan glasses, half color"));
    _sdi_output_right_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-red-cyan.png"), _("Red/cyan glasses, full color"));
    _sdi_output_right_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-red-cyan.png"), _("Red/cyan glasses, Dubois"));
    _sdi_output_right_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-green-magenta.png"), _("Green/magenta glasses, monochrome"));
    _sdi_output_right_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-green-magenta.png"), _("Green/magenta glasses, half color"));
    _sdi_output_right_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-green-magenta.png"), _("Green/magenta glasses, full color"));
    _sdi_output_right_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-green-magenta.png"), _("Green/magenta glasses, Dubois"));
    _sdi_output_right_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-amber-blue.png"), _("Amber/blue glasses, monochrome"));
    _sdi_output_right_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-amber-blue.png"), _("Amber/blue glasses, half color"));
    _sdi_output_right_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-amber-blue.png"), _("Amber/blue glasses, full color"));
    _sdi_output_right_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-amber-blue.png"), _("Amber/blue glasses, Dubois"));
    _sdi_output_right_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-red-green.png"), _("Red/green glasses, monochrome"));
    _sdi_output_right_stereo_mode_combobox->addItem(QIcon(":icons-local/output-type-red-blue.png"), _("Red/blue glasses, monochrome"));
    connect(_sdi_output_right_stereo_mode_combobox, SIGNAL(currentIndexChanged(int)), this, SLOT(sdi_output_right_stereo_mode_changed(int)));

    QPushButton *ok_button = new QPushButton(_("OK"));
    ok_button->setDefault(true);
    connect(ok_button, SIGNAL(clicked()), this, SLOT(close()));

    QGridLayout *layout = new QGridLayout;
    layout->addWidget(sdi_output_format_label, 0, 0);
    layout->addWidget(_sdi_output_format_combobox, 0, 1, 1, 1);
    layout->addWidget(sdi_output_left_stereo_mode_label, 1, 0);
    layout->addWidget(_sdi_output_left_stereo_mode_combobox, 1, 1);
    layout->addWidget(sdi_output_right_stereo_mode_label, 2, 0);
    layout->addWidget(_sdi_output_right_stereo_mode_combobox, 2, 1);
    layout->addWidget(ok_button, 3, 0, 1, 2);
    setLayout(layout);

    update();
#endif // HAVE_LIBXNVCTRL
}

void sdi_output_dialog::update()
{
#if HAVE_LIBXNVCTRL
    _lock = true;
    set_sdi_output_format(dispatch::parameters().sdi_output_format());
    set_sdi_output_left_stereo_mode(dispatch::parameters().sdi_output_left_stereo_mode());
    set_sdi_output_right_stereo_mode(dispatch::parameters().sdi_output_right_stereo_mode());
    _lock = false;
#endif // HAVE_LIBXNVCTRL
}

void sdi_output_dialog::set_sdi_output_format(int val)
{
#if HAVE_LIBXNVCTRL
    _sdi_output_format_combobox->setCurrentIndex(val-1);
#else
    (void)val;
#endif // HAVE_LIBXNVCTRL
}

void sdi_output_dialog::set_sdi_output_left_stereo_mode(parameters::stereo_mode_t stereo_mode)
{
#if HAVE_LIBXNVCTRL
    switch (stereo_mode)
    {
    default:
    case parameters::mode_mono_left:
        _sdi_output_left_stereo_mode_combobox->setCurrentIndex(0);
        break;
    case parameters::mode_mono_right:
        _sdi_output_left_stereo_mode_combobox->setCurrentIndex(1);
        break;
    case parameters::mode_top_bottom:
        _sdi_output_left_stereo_mode_combobox->setCurrentIndex(2);
        break;
    case parameters::mode_top_bottom_half:
        _sdi_output_left_stereo_mode_combobox->setCurrentIndex(3);
        break;
    case parameters::mode_left_right:
        _sdi_output_left_stereo_mode_combobox->setCurrentIndex(4);
        break;
    case parameters::mode_left_right_half:
        _sdi_output_left_stereo_mode_combobox->setCurrentIndex(5);
        break;
    case parameters::mode_even_odd_rows:
        _sdi_output_left_stereo_mode_combobox->setCurrentIndex(6);
        break;
    case parameters::mode_even_odd_columns:
        _sdi_output_left_stereo_mode_combobox->setCurrentIndex(7);
        break;
    case parameters::mode_checkerboard:
        _sdi_output_left_stereo_mode_combobox->setCurrentIndex(8);
        break;
    case parameters::mode_red_cyan_monochrome:
        _sdi_output_left_stereo_mode_combobox->setCurrentIndex(9);
        break;
    case parameters::mode_red_cyan_half_color:
        _sdi_output_left_stereo_mode_combobox->setCurrentIndex(10);
        break;
    case parameters::mode_red_cyan_full_color:
        _sdi_output_left_stereo_mode_combobox->setCurrentIndex(11);
        break;
    case parameters::mode_red_cyan_dubois:
        _sdi_output_left_stereo_mode_combobox->setCurrentIndex(12);
        break;
    case parameters::mode_green_magenta_monochrome:
        _sdi_output_left_stereo_mode_combobox->setCurrentIndex(13);
        break;
    case parameters::mode_green_magenta_half_color:
        _sdi_output_left_stereo_mode_combobox->setCurrentIndex(14);
        break;
    case parameters::mode_green_magenta_full_color:
        _sdi_output_left_stereo_mode_combobox->setCurrentIndex(15);
        break;
    case parameters::mode_green_magenta_dubois:
        _sdi_output_left_stereo_mode_combobox->setCurrentIndex(16);
        break;
    case parameters::mode_amber_blue_monochrome:
        _sdi_output_left_stereo_mode_combobox->setCurrentIndex(17);
        break;
    case parameters::mode_amber_blue_half_color:
        _sdi_output_left_stereo_mode_combobox->setCurrentIndex(18);
        break;
    case parameters::mode_amber_blue_full_color:
        _sdi_output_left_stereo_mode_combobox->setCurrentIndex(19);
        break;
    case parameters::mode_amber_blue_dubois:
        _sdi_output_left_stereo_mode_combobox->setCurrentIndex(20);
        break;
    case parameters::mode_red_green_monochrome:
        _sdi_output_left_stereo_mode_combobox->setCurrentIndex(21);
        break;
    case parameters::mode_red_blue_monochrome:
        _sdi_output_left_stereo_mode_combobox->setCurrentIndex(22);
        break;
    }
#else
    (void)stereo_mode;
#endif // HAVE_LIBXNVCTRL
}

void sdi_output_dialog::set_sdi_output_right_stereo_mode(parameters::stereo_mode_t stereo_mode)
{
#if HAVE_LIBXNVCTRL
    switch (stereo_mode)
    {
    default:
    case parameters::mode_mono_left:
        _sdi_output_right_stereo_mode_combobox->setCurrentIndex(0);
        break;
    case parameters::mode_mono_right:
        _sdi_output_right_stereo_mode_combobox->setCurrentIndex(1);
        break;
    case parameters::mode_top_bottom:
        _sdi_output_right_stereo_mode_combobox->setCurrentIndex(2);
        break;
    case parameters::mode_top_bottom_half:
        _sdi_output_right_stereo_mode_combobox->setCurrentIndex(3);
        break;
    case parameters::mode_left_right:
        _sdi_output_right_stereo_mode_combobox->setCurrentIndex(4);
        break;
    case parameters::mode_left_right_half:
        _sdi_output_right_stereo_mode_combobox->setCurrentIndex(5);
        break;
    case parameters::mode_even_odd_rows:
        _sdi_output_right_stereo_mode_combobox->setCurrentIndex(6);
        break;
    case parameters::mode_even_odd_columns:
        _sdi_output_right_stereo_mode_combobox->setCurrentIndex(7);
        break;
    case parameters::mode_checkerboard:
        _sdi_output_right_stereo_mode_combobox->setCurrentIndex(8);
        break;
    case parameters::mode_red_cyan_monochrome:
        _sdi_output_right_stereo_mode_combobox->setCurrentIndex(9);
        break;
    case parameters::mode_red_cyan_half_color:
        _sdi_output_right_stereo_mode_combobox->setCurrentIndex(10);
        break;
    case parameters::mode_red_cyan_full_color:
        _sdi_output_right_stereo_mode_combobox->setCurrentIndex(11);
        break;
    case parameters::mode_red_cyan_dubois:
        _sdi_output_right_stereo_mode_combobox->setCurrentIndex(12);
        break;
    case parameters::mode_green_magenta_monochrome:
        _sdi_output_right_stereo_mode_combobox->setCurrentIndex(13);
        break;
    case parameters::mode_green_magenta_half_color:
        _sdi_output_right_stereo_mode_combobox->setCurrentIndex(14);
        break;
    case parameters::mode_green_magenta_full_color:
        _sdi_output_right_stereo_mode_combobox->setCurrentIndex(15);
        break;
    case parameters::mode_green_magenta_dubois:
        _sdi_output_right_stereo_mode_combobox->setCurrentIndex(16);
        break;
    case parameters::mode_amber_blue_monochrome:
        _sdi_output_right_stereo_mode_combobox->setCurrentIndex(17);
        break;
    case parameters::mode_amber_blue_half_color:
        _sdi_output_right_stereo_mode_combobox->setCurrentIndex(18);
        break;
    case parameters::mode_amber_blue_full_color:
        _sdi_output_right_stereo_mode_combobox->setCurrentIndex(19);
        break;
    case parameters::mode_amber_blue_dubois:
        _sdi_output_right_stereo_mode_combobox->setCurrentIndex(20);
        break;
    case parameters::mode_red_green_monochrome:
        _sdi_output_right_stereo_mode_combobox->setCurrentIndex(21);
        break;
    case parameters::mode_red_blue_monochrome:
        _sdi_output_right_stereo_mode_combobox->setCurrentIndex(22);
        break;
    }
#else
    (void)stereo_mode;
#endif // HAVE_LIBXNVCTRL
}

void sdi_output_dialog::sdi_output_format_changed(int val)
{
#if HAVE_LIBXNVCTRL
    if (!_lock)
        send_cmd(command::set_sdi_output_format, val+1);
#else
    (void)val;
#endif // HAVE_LIBXNVCTRL
}

void sdi_output_dialog::sdi_output_left_stereo_mode_changed(int val)
{
#if HAVE_LIBXNVCTRL
    parameters::stereo_mode_t stereo_mode = parameters::mode_mono_left;

    switch (val)
    {
    case 0:
        stereo_mode = parameters::mode_mono_left;
        break;
    case 1:
        stereo_mode = parameters::mode_mono_right;
        break;
    case 2:
        stereo_mode = parameters::mode_top_bottom;
        break;
    case 3:
        stereo_mode = parameters::mode_top_bottom_half;
        break;
    case 4:
        stereo_mode = parameters::mode_left_right;
        break;
    case 5:
        stereo_mode = parameters::mode_left_right_half;
        break;
    case 6:
        stereo_mode = parameters::mode_even_odd_rows;
        break;
    case 7:
        stereo_mode = parameters::mode_even_odd_columns;
        break;
    case 8:
        stereo_mode = parameters::mode_checkerboard;
        break;
    case 9:
        stereo_mode = parameters::mode_red_cyan_monochrome;
        break;
    case 10:
        stereo_mode = parameters::mode_red_cyan_half_color;
        break;
    case 11:
        stereo_mode = parameters::mode_red_cyan_full_color;
        break;
    case 12:
        stereo_mode = parameters::mode_red_cyan_dubois;
        break;
    case 13:
        stereo_mode = parameters::mode_green_magenta_monochrome;
        break;
    case 14:
        stereo_mode = parameters::mode_green_magenta_half_color;
        break;
    case 15:
        stereo_mode = parameters::mode_green_magenta_full_color;
        break;
    case 16:
        stereo_mode = parameters::mode_green_magenta_dubois;
        break;
    case 17:
        stereo_mode = parameters::mode_amber_blue_monochrome;
        break;
    case 18:
        stereo_mode = parameters::mode_amber_blue_half_color;
        break;
    case 19:
        stereo_mode = parameters::mode_amber_blue_full_color;
        break;
    case 20:
        stereo_mode = parameters::mode_amber_blue_dubois;
        break;
    case 21:
        stereo_mode = parameters::mode_red_green_monochrome;
        break;
    case 22:
        stereo_mode = parameters::mode_red_blue_monochrome;
        break;
    }

    if (!_lock)
        send_cmd(command::set_sdi_output_left_stereo_mode, static_cast<int>(stereo_mode));
#else
    (void)val;
#endif // HAVE_LIBXNVCTRL
}

void sdi_output_dialog::sdi_output_right_stereo_mode_changed(int val)
{
#if HAVE_LIBXNVCTRL
    parameters::stereo_mode_t stereo_mode = parameters::mode_mono_left;

    switch (val)
    {
    case 0:
        stereo_mode = parameters::mode_mono_left;
        break;
    case 1:
        stereo_mode = parameters::mode_mono_right;
        break;
    case 2:
        stereo_mode = parameters::mode_top_bottom;
        break;
    case 3:
        stereo_mode = parameters::mode_top_bottom_half;
        break;
    case 4:
        stereo_mode = parameters::mode_left_right;
        break;
    case 5:
        stereo_mode = parameters::mode_left_right_half;
        break;
    case 6:
        stereo_mode = parameters::mode_even_odd_rows;
        break;
    case 7:
        stereo_mode = parameters::mode_even_odd_columns;
        break;
    case 8:
        stereo_mode = parameters::mode_checkerboard;
        break;
    case 9:
        stereo_mode = parameters::mode_red_cyan_monochrome;
        break;
    case 10:
        stereo_mode = parameters::mode_red_cyan_half_color;
        break;
    case 11:
        stereo_mode = parameters::mode_red_cyan_full_color;
        break;
    case 12:
        stereo_mode = parameters::mode_red_cyan_dubois;
        break;
    case 13:
        stereo_mode = parameters::mode_green_magenta_monochrome;
        break;
    case 14:
        stereo_mode = parameters::mode_green_magenta_half_color;
        break;
    case 15:
        stereo_mode = parameters::mode_green_magenta_full_color;
        break;
    case 16:
        stereo_mode = parameters::mode_green_magenta_dubois;
        break;
    case 17:
        stereo_mode = parameters::mode_amber_blue_monochrome;
        break;
    case 18:
        stereo_mode = parameters::mode_amber_blue_half_color;
        break;
    case 19:
        stereo_mode = parameters::mode_amber_blue_full_color;
        break;
    case 20:
        stereo_mode = parameters::mode_amber_blue_dubois;
        break;
    case 21:
        stereo_mode = parameters::mode_red_green_monochrome;
        break;
    case 22:
        stereo_mode = parameters::mode_red_blue_monochrome;
        break;
    }

    if (!_lock)
        send_cmd(command::set_sdi_output_right_stereo_mode, static_cast<int>(stereo_mode));
#else
    (void)val;
#endif // HAVE_LIBXNVCTRL
}

void sdi_output_dialog::receive_notification(const notification &note)
{
#if HAVE_LIBXNVCTRL
    switch (note.type)
    {
    case notification::sdi_output_format:
        update();
        break;
    case notification::sdi_output_left_stereo_mode:
        update();
        break;
    case notification::sdi_output_right_stereo_mode:
        update();
        break;
    default:
        /* not handled */
        break;
    }
#else
    (void)note;
#endif // HAVE_LIBXNVCTRL
}


open_device_dialog::open_device_dialog(
        const QStringList &default_devices,
        const QStringList &firewire_devices,
        const QStringList &last_devices,
        const device_request &dev_request,
        QWidget *parent) :
    QDialog(parent)
{
    setWindowTitle(_("Open device(s)"));

    _type_combobox = new QComboBox();
    _type_combobox->setToolTip(_("<p>Choose a device type.</p>"));
    _type_combobox->addItem(_("Default"));
    _type_combobox->addItem(_("Firewire"));
    _type_combobox->addItem(_("X11"));
    for (int i = 0; i < 2; i++) {
        _default_device_combobox[i] = new QComboBox();
        _default_device_combobox[i]->setToolTip(_("<p>Choose a device.</p>"));
        _default_device_combobox[i]->addItems(default_devices);
        _firewire_device_combobox[i] = new QComboBox();
        _firewire_device_combobox[i]->setToolTip(_("<p>Choose a device.</p>"));
        _firewire_device_combobox[i]->addItems(firewire_devices);
        _x11_device_field[i] = new QLineEdit();
        _x11_device_field[i]->setToolTip(_("<p>Set the X11 device string. "
                    "Refer to the manual for details.</p>"));
        _x11_device_field[i]->setText("localhost:0.0+0,0");
        _device_chooser_stack[i] = new QStackedWidget();
        _device_chooser_stack[i]->addWidget(_default_device_combobox[i]);
        _device_chooser_stack[i]->addWidget(_firewire_device_combobox[i]);
        _device_chooser_stack[i]->addWidget(_x11_device_field[i]);
        if (last_devices.size() > i) {
            int j;
            if (dev_request.device == device_request::sys_default
                    && (j = _default_device_combobox[i]->findText(last_devices[i])) >= 0) {
                _default_device_combobox[i]->setCurrentIndex(j);
            } else if (dev_request.device == device_request::firewire
                    && (j = _firewire_device_combobox[i]->findText(last_devices[i])) >= 0) {
                _firewire_device_combobox[i]->setCurrentIndex(j);
            } else if (dev_request.device == device_request::x11) {
                _x11_device_field[i]->setText(last_devices[i]);
            }
        }
    }
    connect(_type_combobox, SIGNAL(currentIndexChanged(int)), _device_chooser_stack[0], SLOT(setCurrentIndex(int)));
    connect(_type_combobox, SIGNAL(currentIndexChanged(int)), _device_chooser_stack[1], SLOT(setCurrentIndex(int)));
    _type_combobox->setCurrentIndex(dev_request.device == device_request::firewire ? 1
            : dev_request.device == device_request::x11 ? 2 : 0);
    QLabel *first_device_label = new QLabel(_("First device:"));
    _second_device_checkbox = new QCheckBox(_("Second device:"));
    _second_device_checkbox->setChecked(last_devices.size() > 1);
    _device_chooser_stack[1]->setEnabled(last_devices.size() > 1);
    connect(_second_device_checkbox, SIGNAL(toggled(bool)), _device_chooser_stack[1], SLOT(setEnabled(bool)));
    _frame_size_groupbox = new QGroupBox(_("Request frame size"));
    _frame_size_groupbox->setToolTip(_("<p>Request a specific frame size from the device, e.g. 640x480. "
                "The device must support this frame size. Some devices require a frame size to be selected.</p>"));
    _frame_size_groupbox->setCheckable(true);
    _frame_size_groupbox->setChecked(dev_request.width != 0 && dev_request.height != 0);
    connect(_frame_size_groupbox, SIGNAL(clicked(bool)), this, SLOT(frame_size_groupbox_clicked(bool)));
    _frame_width_spinbox = new QSpinBox();
    _frame_width_spinbox->setRange(_frame_size_groupbox->isChecked() ? 1 : 0, 65535);
    _frame_width_spinbox->setValue(dev_request.width);
    _frame_height_spinbox = new QSpinBox();
    _frame_height_spinbox->setRange(_frame_size_groupbox->isChecked() ? 1 : 0, 65535);
    _frame_height_spinbox->setValue(dev_request.height);
    _frame_rate_groupbox = new QGroupBox(_("Request frame rate"));
    _frame_rate_groupbox->setToolTip(_("<p>Request a specific frame rate from the device, e.g. 25/1. "
                "The device must support this frame rate. Some devices require a frame rate to be selected.</p>"));
    _frame_rate_groupbox->setCheckable(true);
    _frame_rate_groupbox->setChecked(dev_request.frame_rate_num != 0 && dev_request.frame_rate_den != 0);
    connect(_frame_rate_groupbox, SIGNAL(clicked(bool)), this, SLOT(frame_rate_groupbox_clicked(bool)));
    _frame_rate_num_spinbox = new QSpinBox();
    _frame_rate_num_spinbox->setRange(_frame_rate_groupbox->isChecked() ? 1 : 0, 65535);
    _frame_rate_num_spinbox->setValue(dev_request.frame_rate_num);
    _frame_rate_den_spinbox = new QSpinBox();
    _frame_rate_den_spinbox->setRange(_frame_rate_groupbox->isChecked() ? 1 : 0, 65535);
    _frame_rate_den_spinbox->setValue(dev_request.frame_rate_den);
    _mjpeg_checkbox = new QCheckBox(_("Request MJPEG format"));
    _mjpeg_checkbox->setToolTip(_("<p>Request MJPEG data from the input device. "
                "The device may ignore this request.</p>"));
    _mjpeg_checkbox->setChecked(dev_request.request_mjpeg);
    QPushButton *cancel_btn = new QPushButton(_("Cancel"));
    connect(cancel_btn, SIGNAL(clicked()), this, SLOT(reject()));
    QPushButton *ok_btn = new QPushButton(_("OK"));
    connect(ok_btn, SIGNAL(clicked()), this, SLOT(accept()));

    QGridLayout *frame_size_layout = new QGridLayout;
    frame_size_layout->addWidget(_frame_width_spinbox, 0, 0);
    frame_size_layout->addWidget(new QLabel("x"), 0, 1);
    frame_size_layout->addWidget(_frame_height_spinbox, 0, 2);
    _frame_size_groupbox->setLayout(frame_size_layout);
    QGridLayout *frame_rate_layout = new QGridLayout;
    frame_rate_layout->addWidget(_frame_rate_num_spinbox, 0, 0);
    frame_rate_layout->addWidget(new QLabel("/"), 0, 1);
    frame_rate_layout->addWidget(_frame_rate_den_spinbox, 0, 2);
    _frame_rate_groupbox->setLayout(frame_rate_layout);

    QGridLayout *layout = new QGridLayout;
    layout->addWidget(_type_combobox, 0, 0, 1, 2);
    layout->addWidget(first_device_label, 1, 0);
    layout->addWidget(_device_chooser_stack[0], 1, 1);
    layout->addWidget(_second_device_checkbox, 2, 0);
    layout->addWidget(_device_chooser_stack[1], 2, 1);
    layout->addWidget(_frame_size_groupbox, 3, 0, 1, 2);
    layout->addWidget(_frame_rate_groupbox, 4, 0, 1, 2);
    layout->addWidget(_mjpeg_checkbox, 5, 0, 1, 2);
    layout->addWidget(cancel_btn, 6, 0);
    layout->addWidget(ok_btn, 6, 1);
    layout->setRowStretch(1, 1);
    setLayout(layout);
}

void open_device_dialog::frame_size_groupbox_clicked(bool checked)
{
    _frame_width_spinbox->setRange(checked ? 1 : 0, 65535);
    _frame_width_spinbox->setValue(checked ? 640 : 0);
    _frame_height_spinbox->setRange(checked ? 1 : 0, 65535);
    _frame_height_spinbox->setValue(checked ? 480 : 0);
}

void open_device_dialog::frame_rate_groupbox_clicked(bool checked)
{
    _frame_rate_num_spinbox->setRange(checked ? 1 : 0, 65535);
    _frame_rate_num_spinbox->setValue(checked ? 25 : 0);
    _frame_rate_den_spinbox->setRange(checked ? 1 : 0, 65535);
    _frame_rate_den_spinbox->setValue(checked ? 1 : 0);
}

void open_device_dialog::request(QStringList &devices, device_request &dev_request)
{
    if (_type_combobox->currentIndex() == 1)
    {
        dev_request.device = device_request::firewire;
        devices.push_back(_firewire_device_combobox[0]->currentText());
        if (_second_device_checkbox->isChecked())
            devices.push_back(_firewire_device_combobox[1]->currentText());
    }
    else if (_type_combobox->currentIndex() == 2)
    {
        dev_request.device = device_request::x11;
        devices.push_back(_x11_device_field[0]->text());
        if (_second_device_checkbox->isChecked())
            devices.push_back(_x11_device_field[1]->text());
    }
    else
    {
        dev_request.device = device_request::sys_default;
        devices.push_back(_default_device_combobox[0]->currentText());
        if (_second_device_checkbox->isChecked())
            devices.push_back(_default_device_combobox[1]->currentText());
    }
    dev_request.width = _frame_size_groupbox->isChecked() ? _frame_width_spinbox->value() : 0;
    dev_request.height = _frame_size_groupbox->isChecked() ? _frame_height_spinbox->value() : 0;
    dev_request.frame_rate_num = _frame_rate_groupbox->isChecked() ? _frame_rate_num_spinbox->value() : 0;
    dev_request.frame_rate_den = _frame_rate_groupbox->isChecked() ? _frame_rate_den_spinbox->value() : 0;
    dev_request.request_mjpeg = _mjpeg_checkbox->isChecked();
}

main_window::main_window(QSettings *settings) :
    _settings(settings),
    _fullscreen_dialog(NULL),
    _color_dialog(NULL),
    _crosstalk_dialog(NULL),
    _quality_dialog(NULL),
    _zoom_dialog(NULL),
    _audio_dialog(NULL),
    _subtitle_dialog(NULL),
    _video_dialog(NULL),
#if HAVE_LIBXNVCTRL
    _sdi_output_dialog(NULL),
#endif // HAVE_LIBXNVCTRL
    _max_recent_files(5)
{
    // Application properties
    setWindowTitle(PACKAGE_NAME);
    setWindowIcon(QIcon(":logo/64x64/bino.png"));

    // Load preferences
    QString saved_session_params = _settings->value("Session/parameters", QString("")).toString();
    if (!saved_session_params.isEmpty())
    {
        parameters session_params;
        session_params.load_session_parameters(saved_session_params.toStdString());
        if (!dispatch::parameters().audio_device_is_set() && !session_params.audio_device_is_default())
            send_cmd(command::set_audio_device, session_params.audio_device());
        if (!dispatch::parameters().audio_delay_is_set() && !session_params.audio_delay_is_default())
            send_cmd(command::set_audio_delay, session_params.audio_delay());
        if (!dispatch::parameters().quality_is_set() && !session_params.quality_is_default())
            send_cmd(command::set_quality, session_params.quality());
        if (!dispatch::parameters().contrast_is_set() && !session_params.contrast_is_default())
            send_cmd(command::set_contrast, session_params.contrast());
        if (!dispatch::parameters().brightness_is_set() && !session_params.brightness_is_default())
            send_cmd(command::set_brightness, session_params.brightness());
        if (!dispatch::parameters().hue_is_set() && !session_params.hue_is_default())
            send_cmd(command::set_hue, session_params.hue());
        if (!dispatch::parameters().saturation_is_set() && !session_params.saturation_is_default())
            send_cmd(command::set_saturation, session_params.saturation());
        if (!dispatch::parameters().crosstalk_r_is_set()
                && !dispatch::parameters().crosstalk_g_is_set()
                && !dispatch::parameters().crosstalk_b_is_set()
                && !(session_params.crosstalk_r_is_default()
                    && session_params.crosstalk_g_is_default()
                    && session_params.crosstalk_b_is_default())) {
            std::ostringstream v;
            s11n::save(v, session_params.crosstalk_r());
            s11n::save(v, session_params.crosstalk_g());
            s11n::save(v, session_params.crosstalk_b());
            send_cmd(command::set_crosstalk, v.str());
        }
        if (!dispatch::parameters().subtitle_encoding_is_set() && !session_params.subtitle_encoding_is_default()) {
            std::ostringstream v;
            s11n::save(v, session_params.subtitle_encoding());
            send_cmd(command::set_subtitle_encoding, v.str());
        }
        if (!dispatch::parameters().subtitle_font_is_set() && !session_params.subtitle_font_is_default()) {
            std::ostringstream v;
            s11n::save(v, session_params.subtitle_font());
            send_cmd(command::set_subtitle_font, v.str());
        }
        if (!dispatch::parameters().subtitle_size_is_set() && !session_params.subtitle_size_is_default())
            send_cmd(command::set_subtitle_size, session_params.subtitle_size());
        if (!dispatch::parameters().subtitle_scale_is_set() && !session_params.subtitle_scale_is_default())
            send_cmd(command::set_subtitle_scale, session_params.subtitle_scale());
        if (!dispatch::parameters().subtitle_color_is_set() && !session_params.subtitle_color_is_default())
            send_cmd(command::set_subtitle_color, session_params.subtitle_color());
        if (!dispatch::parameters().fullscreen_screens_is_set() && !session_params.fullscreen_screens_is_default())
            send_cmd(command::set_fullscreen_screens, session_params.fullscreen_screens());
        if (!dispatch::parameters().fullscreen_flip_left_is_set() && !session_params.fullscreen_flip_left_is_default())
            send_cmd(command::set_fullscreen_flip_left, session_params.fullscreen_flip_left());
        if (!dispatch::parameters().fullscreen_flop_left_is_set() && !session_params.fullscreen_flop_left_is_default())
            send_cmd(command::set_fullscreen_flop_left, session_params.fullscreen_flop_left());
        if (!dispatch::parameters().fullscreen_flip_right_is_set() && !session_params.fullscreen_flip_right_is_default())
            send_cmd(command::set_fullscreen_flip_right, session_params.fullscreen_flip_right());
        if (!dispatch::parameters().fullscreen_flop_right_is_set() && !session_params.fullscreen_flop_right_is_default())
            send_cmd(command::set_fullscreen_flop_right, session_params.fullscreen_flop_right());
        if (!dispatch::parameters().fullscreen_inhibit_screensaver_is_set() && !session_params.fullscreen_inhibit_screensaver_is_default())
            send_cmd(command::set_fullscreen_inhibit_screensaver, session_params.fullscreen_inhibit_screensaver());
        if (!dispatch::parameters().fullscreen_3d_ready_sync_is_set() && !session_params.fullscreen_3d_ready_sync_is_default())
            send_cmd(command::set_fullscreen_3d_ready_sync, session_params.fullscreen_3d_ready_sync());
        if (!dispatch::parameters().zoom_is_set() && !session_params.zoom_is_default())
            send_cmd(command::set_zoom, session_params.zoom());
#if HAVE_LIBXNVCTRL
        if (!dispatch::parameters().sdi_output_format_is_set() && !session_params.sdi_output_format_is_default())
            send_cmd(command::set_sdi_output_format, session_params.sdi_output_format());
        if (!dispatch::parameters().sdi_output_left_stereo_mode_is_set() && !session_params.sdi_output_left_stereo_mode_is_default())
            send_cmd(command::set_sdi_output_left_stereo_mode, session_params.sdi_output_left_stereo_mode());
        if (!dispatch::parameters().sdi_output_right_stereo_mode_is_set() && !session_params.sdi_output_right_stereo_mode_is_default())
            send_cmd(command::set_sdi_output_right_stereo_mode, session_params.sdi_output_right_stereo_mode());
#endif // HAVE_LIBXNVCTRL
    }
    else
    {
        // This code block is only for compatibility with Bino <= 1.2.x:
        _settings->beginGroup("Session");
        if (!dispatch::parameters().audio_device_is_set() && _settings->contains("audio-device"))
        {
            send_cmd(command::set_audio_device, _settings->value("audio-device").toInt());
            _settings->remove("audio-device");
        }
        if (!dispatch::parameters().audio_delay_is_set() && _settings->contains("audio-delay"))
        {
            send_cmd(command::set_audio_delay, static_cast<int64_t>(_settings->value("audio-delay").toLongLong()));
            _settings->remove("audio-delay");
        }
        if (!dispatch::parameters().contrast_is_set() && _settings->contains("contrast"))
        {
            send_cmd(command::set_contrast, _settings->value("contrast").toFloat());
            _settings->remove("contrast");
        }
        if (!dispatch::parameters().brightness_is_set() && _settings->contains("brightness"))
        {
            send_cmd(command::set_brightness, _settings->value("brightness").toFloat());
            _settings->remove("brightness");
        }
        if (!dispatch::parameters().hue_is_set() && _settings->contains("hue"))
        {
            send_cmd(command::set_hue, _settings->value("hue").toFloat());
            _settings->remove("hue");
        }
        if (!dispatch::parameters().saturation_is_set() && _settings->contains("saturation"))
        {
            send_cmd(command::set_saturation, _settings->value("saturation").toFloat());
            _settings->remove("saturation");
        }
        if (!dispatch::parameters().crosstalk_r_is_set()
                && !dispatch::parameters().crosstalk_g_is_set()
                && !dispatch::parameters().crosstalk_b_is_set()
                && _settings->contains("crosstalk_r")
                && _settings->contains("crosstalk_g")
                && _settings->contains("crosstalk_b"))
        {
            std::ostringstream v;
            s11n::save(v, _settings->value("crosstalk_r").toFloat());
            s11n::save(v, _settings->value("crosstalk_g").toFloat());
            s11n::save(v, _settings->value("crosstalk_b").toFloat());
            send_cmd(command::set_crosstalk, v.str());
            _settings->remove("crosstalk_r");
            _settings->remove("crosstalk_g");
            _settings->remove("crosstalk_b");
        }
        if (!dispatch::parameters().fullscreen_screens_is_set() && _settings->contains("fullscreen-screens"))
        {
            send_cmd(command::set_fullscreen_screens, _settings->value("fullscreen-screens").toInt());
            _settings->remove("fullscreen-screens");
        }
        if (!dispatch::parameters().fullscreen_flip_left_is_set() && _settings->contains("fullscreen-flip-left"))
        {
            send_cmd(command::set_fullscreen_flip_left, _settings->value("fullscreen-flip-left").toInt());
            _settings->remove("fullscreen-flip-left");
        }
        if (!dispatch::parameters().fullscreen_flop_left_is_set() && _settings->contains("fullscreen-flop-left"))
        {
            send_cmd(command::set_fullscreen_flop_left, _settings->value("fullscreen-flop-left").toInt());
            _settings->remove("fullscreen-flop-left");
        }
        if (!dispatch::parameters().fullscreen_flip_right_is_set() && _settings->contains("fullscreen-flip-right"))
        {
            send_cmd(command::set_fullscreen_flip_right, _settings->value("fullscreen-flip-right").toInt());
            _settings->remove("fullscreen-flip-right");
        }
        if (!dispatch::parameters().fullscreen_flop_right_is_set() && _settings->contains("fullscreen-flop-right"))
        {
            send_cmd(command::set_fullscreen_flop_right, _settings->value("fullscreen-flop-right").toInt());
            _settings->remove("fullscreen-flop-right");
        }
        if (!dispatch::parameters().subtitle_encoding_is_set() && _settings->contains("subtitle-encoding"))
        {
            std::ostringstream v;
            s11n::save(v, _settings->value("subtitle-encoding").toString().toStdString());
            send_cmd(command::set_subtitle_encoding, v.str());
            _settings->remove("subtitle-encoding");
        }
        if (!dispatch::parameters().subtitle_font_is_set() && _settings->contains("subtitle-font"))
        {
            std::ostringstream v;
            s11n::save(v, _settings->value("subtitle-font").toString().toStdString());
            send_cmd(command::set_subtitle_font, v.str());
            _settings->remove("subtitle-font");
        }
        if (!dispatch::parameters().subtitle_size_is_set() && _settings->contains("subtitle-size"))
        {
            send_cmd(command::set_subtitle_size, _settings->value("subtitle-size").toInt());
            _settings->remove("subtitle-size");
        }
        if (!dispatch::parameters().subtitle_scale_is_set() && _settings->contains("subtitle-scale"))
        {
            send_cmd(command::set_subtitle_scale, _settings->value("subtitle-scale").toFloat());
            _settings->remove("subtitle-scale");
        }
        if (!dispatch::parameters().subtitle_color_is_set() && _settings->contains("subtitle-color"))
        {
            send_cmd(command::set_subtitle_color, static_cast<uint64_t>(_settings->value("subtitle-color").toULongLong()));
            _settings->remove("subtitle-color");
        }
        if (!dispatch::parameters().zoom_is_set() && _settings->contains("zoom"))
        {
            send_cmd(command::set_zoom, _settings->value("zoom").toFloat());
            _settings->remove("zoom");
        }
        _settings->endGroup();
    }

    // Central widget and timer
    QWidget *central_widget = new QWidget(this);
    QGridLayout *layout = new QGridLayout();
    _video_container_widget = new video_container_widget(central_widget);
    layout->addWidget(_video_container_widget, 0, 0);
    _timer = new QTimer(this);
    connect(_timer, SIGNAL(timeout()), this, SLOT(playloop_step()));
    _in_out_widget = new in_out_widget(_settings, central_widget);
    layout->addWidget(_in_out_widget, 1, 0);
    _controls_widget = new controls_widget(_settings, central_widget);
    layout->addWidget(_controls_widget, 2, 0);
    layout->setRowStretch(0, 1);
    layout->setColumnStretch(0, 1);
    central_widget->setLayout(layout);
    setCentralWidget(central_widget);

    // Menus
    QMenu *file_menu = menuBar()->addMenu(_("&File"));
    QAction *file_open_act = new QAction(_("&Open file(s)..."), this);
    file_open_act->setShortcut(QKeySequence::Open);
    file_open_act->setIcon(get_icon("document-open"));
    connect(file_open_act, SIGNAL(triggered()), this, SLOT(file_open()));
    file_menu->addAction(file_open_act);
    QAction *file_open_urls_act = new QAction(_("Open &URL(s)..."), this);
    file_open_urls_act->setIcon(get_icon("document-open"));
    connect(file_open_urls_act, SIGNAL(triggered()), this, SLOT(file_open_urls()));
    file_menu->addAction(file_open_urls_act);
    QAction *file_open_device_act = new QAction(_("Open &device(s)..."), this);
    file_open_device_act->setIcon(get_icon("camera-web"));
    connect(file_open_device_act, SIGNAL(triggered()), this, SLOT(file_open_device()));
    file_menu->addAction(file_open_device_act);

    _recent_files_separator = file_menu->addSeparator();

    for (int i = 0; i < _max_recent_files; ++i) {
        QAction * recent = new QAction(this);
        recent->setVisible(false);
        file_menu->addAction(recent);
        connect(recent, SIGNAL(triggered()),
                this, SLOT(open_recent_file()));
        _recent_file_actions.append(recent);
    }
    _clear_recent_files_act = new QAction(_("&Clear recent files"), this);
    connect(_clear_recent_files_act,SIGNAL(triggered()), this, SLOT(clear_recent_files()));

    _clear_recent_separator = file_menu->addSeparator();
    file_menu->addAction(_clear_recent_files_act);
    update_recent_file_actions();

    file_menu->addSeparator();
    QAction *file_quit_act = new QAction(_("&Quit"), this);
    file_quit_act->setShortcut(tr("Ctrl+Q")); // QKeySequence::Quit is not reliable
    file_quit_act->setMenuRole(QAction::QuitRole);
    file_quit_act->setIcon(get_icon("application-exit"));
    connect(file_quit_act, SIGNAL(triggered()), this, SLOT(close()));
    file_menu->addAction(file_quit_act);
    QMenu *preferences_menu = menuBar()->addMenu(_("&Preferences"));
    // note: whenever the preferences menu becomes a preferences panel, don't forget
    // to preferences_act->setMenuRole(QAction::PreferencesRole) on the menu item
    QAction *preferences_fullscreen_act = new QAction(_("&Fullscreen Settings..."), this);
    connect(preferences_fullscreen_act, SIGNAL(triggered()), this, SLOT(preferences_fullscreen()));
    preferences_menu->addAction(preferences_fullscreen_act);
    QAction *preferences_colors_act = new QAction(_("Display &Color Adjustments..."), this);
    connect(preferences_colors_act, SIGNAL(triggered()), this, SLOT(preferences_colors()));
    preferences_menu->addAction(preferences_colors_act);
    QAction *preferences_crosstalk_act = new QAction(_("Display Cross&talk Calibration..."), this);
    connect(preferences_crosstalk_act, SIGNAL(triggered()), this, SLOT(preferences_crosstalk()));
    preferences_menu->addAction(preferences_crosstalk_act);
    preferences_menu->addSeparator();
    QAction *preferences_quality_act = new QAction(_("Quality Adjustments..."), this);
    connect(preferences_quality_act, SIGNAL(triggered()), this, SLOT(preferences_quality()));
    preferences_menu->addAction(preferences_quality_act);
    preferences_menu->addSeparator();
    QAction *preferences_zoom_act = new QAction(_("&Zoom for wide videos..."), this);
    connect(preferences_zoom_act, SIGNAL(triggered()), this, SLOT(preferences_zoom()));
    preferences_menu->addAction(preferences_zoom_act);
    preferences_menu->addSeparator();
    QAction *preferences_audio_act = new QAction(_("&Audio Settings..."), this);
    connect(preferences_audio_act, SIGNAL(triggered()), this, SLOT(preferences_audio()));
    preferences_menu->addAction(preferences_audio_act);
    preferences_menu->addSeparator();
    QAction *preferences_subtitle_act = new QAction(_("&Subtitle Settings..."), this);
    connect(preferences_subtitle_act, SIGNAL(triggered()), this, SLOT(preferences_subtitle()));
    preferences_menu->addAction(preferences_subtitle_act);
    preferences_menu->addSeparator();
    QAction *preferences_video_act = new QAction(_("Current &Video Settings..."), this);
    connect(preferences_video_act, SIGNAL(triggered()), this, SLOT(preferences_video()));
    preferences_menu->addAction(preferences_video_act);
#if HAVE_LIBXNVCTRL
    preferences_menu->addSeparator();
    QAction *preferences_sdi_output_act = new QAction(_("SDI &Output Settings..."), this);
    connect(preferences_sdi_output_act, SIGNAL(triggered()), this, SLOT(preferences_sdi_output()));
    preferences_menu->addAction(preferences_sdi_output_act);
#endif // HAVE_LIBXNVCTRL
    QMenu *help_menu = menuBar()->addMenu(_("&Help"));
    QAction *help_manual_act = new QAction(_("&Manual..."), this);
    help_manual_act->setShortcut(QKeySequence::HelpContents);
    help_manual_act->setIcon(get_icon("help-contents"));
    connect(help_manual_act, SIGNAL(triggered()), this, SLOT(help_manual()));
    help_menu->addAction(help_manual_act);
    QAction *help_website_act = new QAction(_("&Website..."), this);
    help_website_act->setIcon(get_icon("applications-internet"));
    connect(help_website_act, SIGNAL(triggered()), this, SLOT(help_website()));
    help_menu->addAction(help_website_act);
    QAction *help_keyboard_act = new QAction(_("&Keyboard Shortcuts"), this);
    help_keyboard_act->setIcon(get_icon("preferences-desktop-keyboard"));
    connect(help_keyboard_act, SIGNAL(triggered()), this, SLOT(help_keyboard()));
    help_menu->addAction(help_keyboard_act);
    QAction *help_about_act = new QAction(_("&About"), this);
    help_about_act->setMenuRole(QAction::AboutRole);
    help_about_act->setIcon(get_icon("help-about"));
    connect(help_about_act, SIGNAL(triggered()), this, SLOT(help_about()));
    help_menu->addAction(help_about_act);

    // Handle FileOpen events
    QApplication::instance()->installEventFilter(this);
    // Handle Drop events
    setAcceptDrops(true);

    // Restore Mainwindow state
    _settings->beginGroup("Mainwindow");
    restoreGeometry(_settings->value("geometry").toByteArray());
    restoreState(_settings->value("windowstate").toByteArray());
    _settings->endGroup();

    // Work around a Qt/X11 problem with Qt4: if the GL thread is running and a QComboBox
    // shows its popup for the first time, then the application sometimes hangs
    // and sometimes aborts with the message "Unknown request in queue while dequeuing".
    // So we briefly show a QComboBox popup before doing anything else.
    // This seems to be another X11 thing; maybe Wayland will solve this stuff...
#ifdef Q_WS_X11
    {
        QComboBox* box = new QComboBox();
        box->addItem("foo");
        box->addItem("bar");
        box->show();
        box->showPopup();
        box->close();
    }
#endif

#ifdef Q_OS_MAC
    // Since this is a single-window app, don't let the user close the main window on OS X
    setWindowFlags(Qt::Window | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);
#endif

    // Show window. Must happen before opening initial files!
    show();
    raise();

    // Start the event and play loop
    _timer->start(0);
}

main_window::~main_window()
{
}

void main_window::open_recent_file()
{
     QAction *action = qobject_cast<QAction *>(sender());
     if (action)
         open(action->data().toStringList());
}

void main_window::update_recent_file_actions()
{
     QVariantList files = _settings->value("Session/recent-files").toList();

     int num_recent_files = qMin(files.size(), _max_recent_files);
     bool have_recent_files = num_recent_files > 0;

     for (int i = 0; i < num_recent_files; ++i) {
         QStringList recentFilesEntry = files[i].toStringList();
         QString text = QString("&%1 %2").arg(i + 1).arg(stripped_name(recentFilesEntry));
         _recent_file_actions[i]->setText(text);
         _recent_file_actions[i]->setData(recentFilesEntry);
         _recent_file_actions[i]->setVisible(true);
     }
     for (int j = num_recent_files; j < _max_recent_files; ++j)
         _recent_file_actions[j]->setVisible(false);

     _recent_files_separator->setVisible(have_recent_files);
     _clear_recent_separator->setVisible(have_recent_files);
     _clear_recent_files_act->setVisible(have_recent_files);

}

void main_window::clear_recent_files()
{
     _settings->remove("Session/recent-files");
     update_recent_file_actions();
}

QString main_window::stripped_name(const QStringList & filenames)
{
    QStringList stripped;
    foreach(QString file, filenames) {
      stripped.append(QFileInfo(file).fileName());
    }
    QString strippedNames = stripped.join(", ");
    if(strippedNames.length() >= 50)
      strippedNames = strippedNames.left(47).append("...");
    return strippedNames;
}

static QString urls_hash(const std::vector<std::string>& urls)
{
    // Return SHA1 hash of the name of the current file as a hex string
    QString name;
    for (size_t i = 0; i < urls.size(); i++) {
        if (i > 0)
            name += QString("/");       // '/' can never be part of a file name
        name += QFileInfo(QFile::decodeName(urls[i].c_str())).fileName();
    }
    QString hash = QCryptographicHash::hash(name.toUtf8(), QCryptographicHash::Sha1).toHex();
    return hash;
}

void main_window::receive_notification(const notification &note)
{
    switch (note.type) {
    case notification::quit:
        _timer->stop();
        QApplication::quit();
        break;
    case notification::play:
        if (dispatch::playing()) {
            // Force a size adjustment of the main window
            adjustSize();
            // Remember what we're playing
            _now_playing.clear();
            assert(dispatch::media_input());
            for (size_t i = 0; i < dispatch::media_input()->urls(); i++) {
                _now_playing.push_back(dispatch::media_input()->url(i));
            }
        } else {
            // Remember the settings of this video
            std::string video_parameters = dispatch::parameters().save_video_parameters();
            if (!video_parameters.empty())
                _settings->setValue("Video/" + urls_hash(_now_playing), QString::fromStdString(video_parameters));
            // Remember the 2D or 3D video output mode.
            _settings->setValue((dispatch::parameters().stereo_layout() == parameters::layout_mono
                        ? "Session/2d-stereo-mode" : "Session/3d-stereo-mode"),
                    QString::fromStdString(parameters::stereo_mode_to_string(
                            dispatch::parameters().stereo_mode(),
                            dispatch::parameters().stereo_mode_swap())));
        }
        break;
    default:
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

void main_window::moveEvent(QMoveEvent*)
{
    send_cmd(command::update_display_pos);
}

void main_window::closeEvent(QCloseEvent *event)
{
    // Close a currently opened video. This also stops the video if it is
    // playing, so that its settings are remembered.
    send_cmd(command::close);
    // Stop the event and play loop
    _timer->stop();
    // Remember the Session preferences
    _settings->setValue("Session/parameters", QString::fromStdString(dispatch::parameters().save_session_parameters()));
    // Remember the Mainwindow state
    _settings->beginGroup("Mainwindow");
    _settings->setValue("geometry", saveGeometry());
    _settings->setValue("windowState", saveState());
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

void main_window::playloop_step()
{
    try {
        dispatch::step();
        dispatch::process_all_events();
    }
    catch (std::exception& e) {
        send_cmd(command::close);
        QMessageBox::critical(this, "Error", e.what());
    }
}

void main_window::open(QStringList filenames,
        const device_request &dev_request,
        const parameters& initial_params)
{
    try {
        open_input_data input_data;
        input_data.dev_request = dev_request;
        for (int i = 0; i < filenames.size(); i++) {
            input_data.urls.push_back(filenames[i].toLocal8Bit().constData());
        }
        // Get the settings for this video
        QString saved_video_parameters = _settings->value("Video/" + urls_hash(input_data.urls), QString("")).toString();
        if (!saved_video_parameters.isEmpty()) {
            input_data.params.load_video_parameters(saved_video_parameters.toStdString());
        } else {
            // This code block is only for compatibility with Bino <= 1.2.x:
            _settings->beginGroup("Video/" + urls_hash(input_data.urls));
            if (_settings->contains("stereo-layout")) {
                QString layout_name = _settings->value("stereo-layout").toString();
                parameters::stereo_layout_t stereo_layout;
                bool stereo_layout_swap;
                parameters::stereo_layout_from_string(layout_name.toStdString(), stereo_layout, stereo_layout_swap);
                input_data.params.set_stereo_layout(stereo_layout);
                input_data.params.set_stereo_layout_swap(stereo_layout_swap);
                _settings->remove("stereo-layout");
            }
            if (_settings->contains("video-stream")) {
                input_data.params.set_video_stream(_settings->value("video-stream").toInt());
                _settings->remove("video-stream");
            }
            if (_settings->contains("audio-stream")) {
                input_data.params.set_audio_stream(_settings->value("audio-stream").toInt());
                _settings->remove("audio-stream");
            }
            if (_settings->contains("subtitle-stream")) {
                input_data.params.set_subtitle_stream(_settings->value("subtitle-stream").toInt());
                _settings->remove("subtitle-stream");
            }
            if (_settings->contains("parallax")) {
                input_data.params.set_parallax(_settings->value("parallax").toFloat());
                _settings->remove("parallax");
            }
            if (_settings->contains("ghostbust")) {
                input_data.params.set_ghostbust(_settings->value("ghostbust").toFloat());
                _settings->remove("ghostbust");
            }
            if (_settings->contains("subtitle-parallax")) {
                input_data.params.set_subtitle_parallax(_settings->value("subtitle-parallax").toFloat());
                _settings->remove("subtitle-parallax");
            }
            _settings->endGroup();
        }
        // Allow the initial parameters to override our remembered per-video parameters.
        // This is intended to be used to enforce command line parameters.
        if (initial_params.video_stream_is_set())
            input_data.params.set_video_stream(initial_params.video_stream());
        if (initial_params.audio_stream_is_set())
            input_data.params.set_audio_stream(initial_params.audio_stream());
        if (initial_params.subtitle_stream_is_set())
            input_data.params.set_subtitle_stream(initial_params.subtitle_stream());
        if (initial_params.stereo_layout_is_set())
            input_data.params.set_stereo_layout(initial_params.stereo_layout());
        if (initial_params.stereo_layout_swap_is_set())
            input_data.params.set_stereo_layout_swap(initial_params.stereo_layout_swap());
        if (initial_params.crop_aspect_ratio_is_set())
            input_data.params.set_crop_aspect_ratio(initial_params.crop_aspect_ratio());
        if (initial_params.source_aspect_ratio_is_set())
            input_data.params.set_source_aspect_ratio(initial_params.source_aspect_ratio());
        if (initial_params.parallax_is_set())
            input_data.params.set_parallax(initial_params.parallax());
        if (initial_params.ghostbust_is_set())
            input_data.params.set_ghostbust(initial_params.ghostbust());
        if (initial_params.subtitle_parallax_is_set())
            input_data.params.set_subtitle_parallax(initial_params.subtitle_parallax());
        // Now open the input data
        std::ostringstream v;
        s11n::save(v, input_data);
        controller::send_cmd(command::open, v.str());
        // Get stereo mode
        bool mono = (dispatch::media_input()->video_frame_template().stereo_layout == parameters::layout_mono);
        if (initial_params.stereo_mode_is_set()) {
            send_cmd(command::set_stereo_mode, initial_params.stereo_mode());
            send_cmd(command::set_stereo_mode_swap, initial_params.stereo_mode_swap());
        } else if (_settings->contains(mono ? "Session/2d-stereo-mode" : "Session/3d-stereo-mode")) {
            parameters::stereo_mode_t stereo_mode;
            bool stereo_mode_swap;
            QString mode_name = _settings->value(
                    mono ? "Session/2d-stereo-mode" : "Session/3d-stereo-mode").toString();
            parameters::stereo_mode_from_string(mode_name.toStdString(), stereo_mode, stereo_mode_swap);
            if (initial_params.stereo_mode_swap_is_set())
                stereo_mode_swap = initial_params.stereo_mode_swap();
            send_cmd(command::set_stereo_mode, stereo_mode);
            send_cmd(command::set_stereo_mode_swap, stereo_mode_swap);
        }
        // Automatically start playing
        send_cmd(command::toggle_play);
    }
    catch (std::exception& e) {
        QMessageBox::critical(this, "Error", e.what());
    }
}

void main_window::file_open()
{
    if (_settings->value("Session/show-multi-open-tip", true).toBool())
    {
        QDialog *dlg = new QDialog(this);
        dlg->setModal(true);
        dlg->setWindowTitle(_("Tip"));
        QLabel *dlg_icon_label = new QLabel;
        dlg_icon_label->setPixmap(get_icon("dialog-information").pixmap(48));
        QLabel *dlg_tip_label = new QLabel(_("<p>You can select and open multiple files at once.</p>"
                    "<p>This is useful for example if you have extra subtitle files,<br>"
                    "or if left and right view are stored in separate files.</p>"));
        QCheckBox *dlg_checkbox = new QCheckBox(_("Do not show this message again."));
        QPushButton *dlg_ok_button = new QPushButton(_("OK"));
        connect(dlg_ok_button, SIGNAL(clicked()), dlg, SLOT(accept()));
        QGridLayout *dlg_layout = new QGridLayout;
        dlg_layout->addWidget(dlg_icon_label, 0, 0);
        dlg_layout->addWidget(dlg_tip_label, 0, 1);
        dlg_layout->addWidget(new QLabel, 1, 0, 1, 2);
        dlg_layout->addWidget(dlg_checkbox, 2, 0, 1, 2, Qt::AlignHCenter);
        dlg_layout->addWidget(dlg_ok_button, 3, 0, 1, 2, Qt::AlignHCenter);
        dlg_layout->setColumnStretch(1, 1);
        dlg_layout->setRowStretch(1, 1);
        dlg->setLayout(dlg_layout);
        dlg_ok_button->setFocus();
        dlg->exec();
        _settings->setValue("Session/show-multi-open-tip", !dlg_checkbox->isChecked());
    }

    QStringList file_names = QFileDialog::getOpenFileNames(this, _("Open files"),
            _settings->value("Session/file-open-dir", QDir::currentPath()).toString());
    if (file_names.empty())
    {
        return;
    }

    _settings->setValue("Session/file-open-dir", QFileInfo(file_names[0]).path());
    open(file_names);

    QVariantList files = _settings->value("Session/recent-files").toList();

    files.removeAll(file_names);
    files.prepend(file_names);

    while (files.size() > _max_recent_files)
        files.removeLast();
    _settings->setValue("Session/recent-files", files);
    update_recent_file_actions();
}

void main_window::file_open_urls()
{
    QDialog *url_dialog = new QDialog(this);
    url_dialog->setWindowTitle(_("Open URL(s)"));
    QLabel *url_label = new QLabel(_("URL(s):"));
    url_label->setToolTip(_("<p>Enter one or more space separated URLs.</p>"));
    QLineEdit *url_edit = new QLineEdit("");
    url_edit->setToolTip(url_label->toolTip());
    url_edit->setMinimumWidth(256);
    QPushButton *cancel_btn = new QPushButton(_("Cancel"));
    QPushButton *ok_btn = new QPushButton(_("OK"));
    ok_btn->setDefault(true);
    connect(cancel_btn, SIGNAL(clicked()), url_dialog, SLOT(reject()));
    connect(ok_btn, SIGNAL(clicked()), url_dialog, SLOT(accept()));
    QGridLayout *layout = new QGridLayout();
    layout->addWidget(url_label, 0, 0);
    layout->addWidget(url_edit, 0, 1, 1, 3);
    layout->addWidget(cancel_btn, 2, 2);
    layout->addWidget(ok_btn, 2, 3);
    layout->setColumnStretch(1, 1);
    url_dialog->setLayout(layout);
    url_dialog->exec();
    if (url_dialog->result() == QDialog::Accepted
            && !url_edit->text().isEmpty())
    {
        open(url_edit->text().split(" ", QString::SkipEmptyParts));
    }
}

void main_window::file_open_device()
{
    // Gather list of available devices
    QStringList default_devices;
    QStringList firewire_devices;
#ifdef Q_OS_WIN
    // libavdevice supports devices "0" through "9" in the vfwcap driver on Windows.
    // Simply list them all here since I don't know how to find out which are actually available.
    for (int i = 0; i <= 9; i++)
    {
        default_devices << QString(QChar('0' + i));
    }
    firewire_devices << "/dev/video1394";       // TODO: this is definitely not correct
#else
    // video4linux2 devices dynamically create device files /dev/video0 - /dev/video63.
    // TODO: does this also work on BSD and Mac OS?
    for (int i = 0; i < 64; i++)
    {
        QString device = QString("/dev/video") + QString::number(i);
        if (QFileInfo(device).exists())
        {
            default_devices << device;
        }
    }
    if (default_devices.empty())
    {
        // add a fallback entry
        default_devices << QString("/dev/video0");
    }
    firewire_devices << "/dev/video1394";       // TODO: this is probably not correct
#endif

    // Get saved settings
    QStringList last_devices;
    device_request dev_request;
    _settings->beginGroup("Session");
    last_devices = _settings->value("device-last").toStringList();
    if (last_devices.empty() && _settings->contains("device-x11")) {
        // For compatibility with Bino <= 1.3.2:
        last_devices.push_back(_settings->value("device-x11").toString());
        _settings->remove("device-x11");
    }
    QString saved_type = _settings->value("device-type", QString("default")).toString();
    dev_request.device = (saved_type == "firewire" ? device_request::firewire
            : saved_type == "x11" ? device_request::x11 : device_request::sys_default);
    dev_request.width = _settings->value("device-request-frame-width", QString("0")).toInt();
    dev_request.height = _settings->value("device-request-frame-height", QString("0")).toInt();
    dev_request.frame_rate_num = _settings->value("device-request-frame-rate-num", QString("0")).toInt();
    dev_request.frame_rate_den = _settings->value("device-request-frame-rate-den", QString("0")).toInt();
    dev_request.request_mjpeg = _settings->value("device-request-mjpeg", QString("0")).toInt();
    _settings->endGroup();

    // Run dialog
    open_device_dialog *dlg = new open_device_dialog(default_devices, firewire_devices, last_devices, dev_request, this);
    dlg->exec();
    if (dlg->result() != QDialog::Accepted)
    {
        return;
    }
    QStringList devices;
    dlg->request(devices, dev_request);

    // Save settings
    _settings->beginGroup("Session");
    _settings->setValue("device-last", devices);
    _settings->setValue("device-type", QString(dev_request.device == device_request::firewire ? "firewire"
                : dev_request.device == device_request::x11 ? "x11" : "default"));
    _settings->setValue("device-request-frame-width", QVariant(dev_request.width).toString());
    _settings->setValue("device-request-frame-height", QVariant(dev_request.height).toString());
    _settings->setValue("device-request-frame-rate-num", QVariant(dev_request.frame_rate_num).toString());
    _settings->setValue("device-request-frame-rate-den", QVariant(dev_request.frame_rate_den).toString());
    _settings->setValue("device-request-mjpeg", QVariant(dev_request.request_mjpeg ? 1 : 0).toString());
    _settings->endGroup();

    // Open device
    open(devices, dev_request);
}

void main_window::preferences_fullscreen()
{
    if (!_fullscreen_dialog)
        _fullscreen_dialog = new fullscreen_dialog(this);
    _fullscreen_dialog->show();
    _fullscreen_dialog->raise();
    _fullscreen_dialog->activateWindow();
}

void main_window::preferences_colors()
{
    if (!_color_dialog)
    {
        _color_dialog = new color_dialog(this);
    }
    _color_dialog->show();
    _color_dialog->raise();
    _color_dialog->activateWindow();
}

void main_window::preferences_crosstalk()
{
    if (!_crosstalk_dialog)
    {
        _crosstalk_dialog = new crosstalk_dialog(this);
    }
    _crosstalk_dialog->show();
    _crosstalk_dialog->raise();
    _crosstalk_dialog->activateWindow();
}

void main_window::preferences_quality()
{
    if (!_quality_dialog)
    {
        _quality_dialog = new quality_dialog(this);
    }
    _quality_dialog->show();
    _quality_dialog->raise();
    _quality_dialog->activateWindow();
}

void main_window::preferences_zoom()
{
    if (!_zoom_dialog)
    {
        _zoom_dialog = new zoom_dialog(this);
    }
    _zoom_dialog->show();
    _zoom_dialog->raise();
    _zoom_dialog->activateWindow();
}

void main_window::preferences_audio()
{
    if (!_audio_dialog)
    {
        _audio_dialog = new audio_dialog(this);
    }
    _audio_dialog->show();
    _audio_dialog->raise();
    _audio_dialog->activateWindow();
}

void main_window::preferences_subtitle()
{
    if (!_subtitle_dialog)
    {
        _subtitle_dialog = new subtitle_dialog(this);
    }
    _subtitle_dialog->show();
    _subtitle_dialog->raise();
    _subtitle_dialog->activateWindow();
}

void main_window::preferences_video()
{
    if (!_video_dialog)
    {
        _video_dialog = new video_dialog(this);
    }
    _video_dialog->show();
    _video_dialog->raise();
    _video_dialog->activateWindow();
}

void main_window::preferences_sdi_output()
{
#if HAVE_LIBXNVCTRL
    if (!_sdi_output_dialog)
    {
        _sdi_output_dialog = new sdi_output_dialog(this);
    }
    _sdi_output_dialog->show();
    _sdi_output_dialog->raise();
    _sdi_output_dialog->activateWindow();
#endif // HAVE_LIBXNVCTRL
}

void main_window::help_manual()
{
    QUrl manual_url;
#if defined(Q_OS_WIN)
    manual_url = QUrl::fromLocalFile(QCoreApplication::applicationDirPath() + "/../doc/bino.html");
#elif defined(Q_OS_MAC)
    manual_url = QUrl::fromLocalFile(QCoreApplication::applicationDirPath() + "/../Resources/Bino Help/bino.html");
#else
    manual_url = QUrl::fromLocalFile(HTMLDIR "/bino.html");
#endif
    if (!QDesktopServices::openUrl(manual_url))
    {
        QMessageBox::critical(this, _("Error"), _("Cannot open manual."));
    }
}

void main_window::help_website()
{
    if (!QDesktopServices::openUrl(QUrl(PACKAGE_URL)))
    {
        QMessageBox::critical(this, _("Error"), _("Cannot open website."));
    }
}

void main_window::help_keyboard()
{
    static QMessageBox* msgbox = NULL;
    if (!msgbox) {
        msgbox = new QMessageBox(QMessageBox::Information, _("Keyboard Shortcuts"),
                // A list of keyboard shortcuts. Keep the translatable strings in sync with
                // the --help output in main.cpp, to reduce the burden for translators.
                QString("<p>") + _("Keyboard control:") + "<br>"
                "<table>"
                "<tr><td>q, " + _("ESC") + "</td><td>" + _("Stop") + "</td></tr>"
                "<tr><td>p / " + _("SPACE") + "</td><td>" + _("Pause / unpause") + "</td></tr>"
                "<tr><td>f</td><td>" + _("Toggle fullscreen") + "</td></tr>"
                "<tr><td>c</td><td>" + _("Center window") + "</td></tr>"
                "<tr><td>e / F7</td><td>" + _("Swap left/right eye") + "</td></tr>"
                "<tr><td>v</td><td>" + _("Cycle through available video streams") + "</td></tr>"
                "<tr><td>a</td><td>" + _("Cycle through available audio streams") + "</td></tr>"
                "<tr><td>s</td><td>" + _("Cycle through available subtitle streams") + "</td></tr>"
                "<tr><td>1, 2</td><td>" + _("Adjust contrast") + "</td></tr>"
                "<tr><td>3, 4</td><td>" + _("Adjust brightness") + "</td></tr>"
                "<tr><td>5, 6</td><td>" + _("Adjust hue") + "</td></tr>"
                "<tr><td>7, 8</td><td>" + _("Adjust saturation") + "</td></tr>"
                "<tr><td>[, ]</td><td>" + _("Adjust parallax") + "</td></tr>"
                "<tr><td>(, )</td><td>" + _("Adjust ghostbusting") + "</td></tr>"
                "<tr><td>&lt;, &gt;</td><td>" + _("Adjust zoom for wide videos") + "</td></tr>"
                "<tr><td>/, *</td><td>" + _("Adjust audio volume") + "</td></tr>"
                "<tr><td>m</td><td>" + _("Toggle audio mute") + "</td></tr>"
                "<tr><td>.</td><td>" + _("Step a single video frame forward") + "</td></tr>"
                "<tr><td>" + _("left, right") + "</td><td>" + _("Seek 10 seconds backward / forward") + "</td></tr>"
                "<tr><td>" + _("down, up") + "</td><td>" + _("Seek 1 minute backward / forward") + "</td></tr>"
                "<tr><td>" + _("page down, page up") + "</td><td>" + _("Seek 10 minutes backward / forward") + "</td></tr>"
                "<tr><td>" + _("Media keys") + "</td><td>" + _("Media keys should work as expected") + "</td></tr>"
                "</table>"
                "</p>", QMessageBox::Ok, this);
        msgbox->setModal(false);
    }
    msgbox->show();
    msgbox->raise();
    msgbox->activateWindow();
}

void main_window::help_about()
{
    static QDialog* dialog = NULL;

    if (!dialog) {
        dialog = new QDialog(this);
        dialog->setWindowTitle(str::asprintf(_("About %s"), PACKAGE_NAME).c_str());

        QLabel* logo = new QLabel();
        logo->setPixmap(QPixmap(":logo/bino_logo.png").scaledToHeight(128, Qt::SmoothTransformation));
        QLabel* spacer = new QLabel();
        spacer->setMinimumWidth(16);
        QLabel* title = new QLabel(str::asprintf(_("The %s 3D video player"), PACKAGE_NAME).c_str());

        QTextBrowser* about_text = new QTextBrowser(this);
        about_text->setOpenExternalLinks(true);
        about_text->setText(str::asprintf(_(
                        "<p>%s version %s.</p>"
                        "<p>Copyright (C) 2014 the Bino developers.</p>"
                        "<p>This is free software. You may redistribute copies of it "
                        "under the terms of the <a href=\"http://www.gnu.org/licenses/gpl.html\">"
                        "GNU General Public License</a>. "
                        "There is NO WARRANTY, to the extent permitted by law.</p>"
                        "<p>See <a href=\"%s\">%s</a> for more information on this software.</p>"),
                    PACKAGE_NAME, VERSION, PACKAGE_URL, PACKAGE_URL).c_str());

        QTextBrowser* libs_text = new QTextBrowser(this);
        libs_text->setOpenExternalLinks(true);
        libs_text->setWordWrapMode(QTextOption::NoWrap);
        QString libs_blurb = QString(str::asprintf("<p>%s<ul><li>%s</li></ul></p>", _("Platform:"), PLATFORM).c_str());
        libs_blurb += QString("<p>") + QString(_("Libraries used:"));
        std::vector<std::string> libs = lib_versions(true);
        for (size_t i = 0; i < libs.size(); i++)
            libs_blurb += libs[i].c_str();
        libs_blurb += QString("</p>");
        libs_text->setText(libs_blurb);

        QTextBrowser* team_text = new QTextBrowser(this);
        team_text->setOpenExternalLinks(false);
        team_text->setWordWrapMode(QTextOption::NoWrap);
        QFile team_file(":AUTHORS");
        team_file.open(QIODevice::ReadOnly | QIODevice::Text);
        team_text->setText(QTextCodec::codecForName("UTF-8")->toUnicode(team_file.readAll()));

        QTextBrowser* license_text = new QTextBrowser(this);
        license_text->setOpenExternalLinks(false);
        license_text->setWordWrapMode(QTextOption::NoWrap);
        QFile license_file(":LICENSE");
        license_file.open(QIODevice::ReadOnly | QIODevice::Text);
        license_text->setText(license_file.readAll());

        QTabWidget* tab_widget = new QTabWidget();
        tab_widget->addTab(about_text, _("About"));
        tab_widget->addTab(libs_text, _("Libraries"));
        tab_widget->addTab(team_text, _("Team"));
        tab_widget->addTab(license_text, _("License"));

        QPushButton* ok_btn = new QPushButton(_("OK"));
        connect(ok_btn, SIGNAL(clicked()), dialog, SLOT(accept()));

        QGridLayout* layout = new QGridLayout();
        layout->addWidget(logo, 0, 0);
        layout->addWidget(spacer, 0, 1);
        layout->addWidget(title, 0, 2, 1, 2);
        layout->addWidget(spacer, 0, 4);
        layout->addWidget(tab_widget, 1, 0, 1, 5);
        layout->addWidget(ok_btn, 2, 3, 1, 2);
        layout->setColumnStretch(1, 1);
        layout->setColumnStretch(4, 1);
        layout->setRowStretch(1, 1);
        dialog->setLayout(layout);
        about_text->setMinimumWidth(384);
    }

    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

gui::gui()
{
    QCoreApplication::setOrganizationName(PACKAGE_NAME);
    QCoreApplication::setApplicationName(PACKAGE_NAME);
    _settings = new QSettings;
    _main_window = new main_window(_settings);
}

gui::~gui()
{
    delete _main_window;
    delete _settings;
}

void gui::open(const open_input_data& input_data)
{
     if (input_data.urls.size() > 0) {
         QStringList urls;
         for (size_t i = 0; i < input_data.urls.size(); i++)
             urls.push_back(QFile::decodeName(input_data.urls[i].c_str()));
         parameters initial_params = input_data.params;
         if (dispatch::parameters().stereo_mode_is_set())
             initial_params.set_stereo_mode(dispatch::parameters().stereo_mode());
         if (dispatch::parameters().stereo_mode_swap_is_set())
             initial_params.set_stereo_mode_swap(dispatch::parameters().stereo_mode_swap());
         _main_window->open(urls, input_data.dev_request, initial_params);
     }
}
