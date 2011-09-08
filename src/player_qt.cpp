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

#include "config.h"

#include <limits>
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

#include "gettext.h"
#define _(string) gettext(string)

#include "player_qt.h"
#include "video_output_qt.h"
#include "lib_versions.h"

#include "dbg.h"
#include "msg.h"


// Helper function: Get the icon with the given name from the icon theme.
// If unavailable, fall back to the built-in icon. Icon names conform to this specification:
// http://standards.freedesktop.org/icon-naming-spec/icon-naming-spec-latest.html
static QIcon get_icon(const QString &name)
{
    return QIcon::fromTheme(name, QIcon(QString(":icons/") + name));
}


player_qt_internal::player_qt_internal(video_output_qt *video_output) :
    player(player::master), _playing(false), _video_output(video_output)
{
}

player_qt_internal::~player_qt_internal()
{
}

video_output *player_qt_internal::create_video_output()
{
    return _video_output;
}

void player_qt_internal::destroy_video_output(video_output *)
{
    // do nothing; we reuse our video output later
}

void player_qt_internal::receive_cmd(const command &cmd)
{
    if (cmd.type == command::toggle_play && has_media_input() && !_playing)
    {
        controller::notify_all(notification::play, false, true);
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

video_output_qt *player_qt_internal::get_video_output()
{
    return _video_output;
}

bool player_qt_internal::is_playing() const
{
    return _playing;
}

bool player_qt_internal::playloop_step()
{
    return run_step();
}

void player_qt_internal::force_stop()
{
    controller::notify_all(notification::play, false, false);
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
    _output_combobox->addItem(QIcon(":icons-local/output-type-top-bottom.png"), _("Top/bottom"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-top-bottom-half.png"), _("Top/bottom, half height"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-left-right.png"), _("Left/right"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-left-right-half.png"), _("Left/right, half width"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-even-odd-rows.png"), _("Even/odd rows"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-even-odd-columns.png"), _("Even/odd columns"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-checkerboard.png"), _("Checkerboard pattern"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-hdmi-frame-pack.png"), _("HDMI frame packing mode"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-red-cyan.png"), _("Red/cyan glasses, monochrome method"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-red-cyan.png"), _("Red/cyan glasses, half-color method"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-red-cyan.png"), _("Red/cyan glasses, full-color method"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-red-cyan.png"), _("Red/cyan glasses, high-quality Dubois method"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-green-magenta.png"), _("Green/magenta glasses, monochrome method"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-green-magenta.png"), _("Green/magenta glasses, half-color method"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-green-magenta.png"), _("Green/magenta glasses, full-color method"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-green-magenta.png"), _("Green/magenta glasses, high-quality Dubois method"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-amber-blue.png"), _("Amber/blue glasses, monochrome method"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-amber-blue.png"), _("Amber/blue glasses, half-color method"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-amber-blue.png"), _("Amber/blue glasses, full-color method"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-amber-blue.png"), _("Amber/blue glasses, high-quality Dubois method"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-red-green.png"), _("Red/green glasses, monochrome method"));
    _output_combobox->addItem(QIcon(":icons-local/output-type-red-blue.png"), _("Red/blue glasses, monochrome method"));
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
    _video_combobox->setEnabled(stereo_layout != video_frame::separate);
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
    case parameters::stereo:
        _output_combobox->setCurrentIndex(2);
        break;
    case parameters::top_bottom:
        _output_combobox->setCurrentIndex(3);
        break;
    case parameters::top_bottom_half:
        _output_combobox->setCurrentIndex(4);
        break;
    case parameters::left_right:
        _output_combobox->setCurrentIndex(5);
        break;
    case parameters::left_right_half:
        _output_combobox->setCurrentIndex(6);
        break;
    case parameters::even_odd_rows:
        _output_combobox->setCurrentIndex(7);
        break;
    case parameters::even_odd_columns:
        _output_combobox->setCurrentIndex(8);
        break;
    case parameters::checkerboard:
        _output_combobox->setCurrentIndex(9);
        break;
    case parameters::hdmi_frame_pack:
        _output_combobox->setCurrentIndex(10);
        break;
    case parameters::red_cyan_monochrome:
        _output_combobox->setCurrentIndex(11);
        break;
    case parameters::red_cyan_half_color:
        _output_combobox->setCurrentIndex(12);
        break;
    case parameters::red_cyan_full_color:
        _output_combobox->setCurrentIndex(13);
        break;
    case parameters::red_cyan_dubois:
        _output_combobox->setCurrentIndex(14);
        break;
    case parameters::green_magenta_monochrome:
        _output_combobox->setCurrentIndex(15);
        break;
    case parameters::green_magenta_half_color:
        _output_combobox->setCurrentIndex(16);
        break;
    case parameters::green_magenta_full_color:
        _output_combobox->setCurrentIndex(17);
        break;
    case parameters::green_magenta_dubois:
        _output_combobox->setCurrentIndex(18);
        break;
    case parameters::amber_blue_monochrome:
        _output_combobox->setCurrentIndex(19);
        break;
    case parameters::amber_blue_half_color:
        _output_combobox->setCurrentIndex(20);
        break;
    case parameters::amber_blue_full_color:
        _output_combobox->setCurrentIndex(21);
        break;
    case parameters::amber_blue_dubois:
        _output_combobox->setCurrentIndex(22);
        break;
    case parameters::red_green_monochrome:
        _output_combobox->setCurrentIndex(23);
        break;
    case parameters::red_blue_monochrome:
        _output_combobox->setCurrentIndex(24);
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
    video_frame::stereo_layout_t stereo_layout;
    bool stereo_layout_swap;
    get_stereo_layout(stereo_layout, stereo_layout_swap);
    if (stereo_layout == video_frame::separate)
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
    _video_combobox->setEnabled(have_valid_input);
    _audio_combobox->setEnabled(have_valid_input);
    _subtitle_combobox->setEnabled(have_valid_input);
    _input_combobox->setEnabled(have_valid_input);
    _output_combobox->setEnabled(have_valid_input);
    _swap_checkbox->setEnabled(have_valid_input);
    _video_combobox->clear();
    _audio_combobox->clear();
    _subtitle_combobox->clear();
    if (have_valid_input)
    {
        for (int i = 0; i < _player->get_media_input().video_streams(); i++)
        {
            _video_combobox->addItem(_player->get_media_input().video_stream_name(i).c_str());
        }
        for (int i = 0; i < _player->get_media_input().audio_streams(); i++)
        {
            _audio_combobox->addItem(_player->get_media_input().audio_stream_name(i).c_str());
        }
        _subtitle_combobox->addItem(_("Off"));
        for (int i = 0; i < _player->get_media_input().subtitle_streams(); i++)
        {
            _subtitle_combobox->addItem(_player->get_media_input().subtitle_stream_name(i).c_str());
        }
        _video_combobox->setCurrentIndex(init_data.video_stream);
        _audio_combobox->setCurrentIndex(init_data.audio_stream);
        _subtitle_combobox->setCurrentIndex(init_data.subtitle_stream + 1);
        // Disable unsupported input modes
        for (int i = 0; i < _input_combobox->count(); i++)
        {
            _input_combobox->setCurrentIndex(i);
            video_frame::stereo_layout_t layout;
            bool swap;
            get_stereo_layout(layout, swap);
            qobject_cast<QStandardItemModel *>(_input_combobox->model())->item(i)->setEnabled(
                    _player->get_media_input().stereo_layout_is_supported(layout, swap));
        }
        // Disable unsupported output modes
        if (!_player->get_video_output()->supports_stereo())
        {
            set_stereo_mode(parameters::stereo, false);
            qobject_cast<QStandardItemModel *>(_output_combobox->model())->item(_output_combobox->currentIndex())->setEnabled(false);
        }
        set_stereo_layout(init_data.stereo_layout, init_data.stereo_layout_swap);
        set_stereo_mode(init_data.stereo_mode, init_data.stereo_mode_swap);
    }
    _lock = false;
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

int in_out_widget::get_subtitle_stream()
{
    return _subtitle_combobox->currentIndex() - 1;
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
        stereo_mode = parameters::stereo;
        break;
    case 3:
        stereo_mode = parameters::top_bottom;
        break;
    case 4:
        stereo_mode = parameters::top_bottom_half;
        break;
    case 5:
        stereo_mode = parameters::left_right;
        break;
    case 6:
        stereo_mode = parameters::left_right_half;
        break;
    case 7:
        stereo_mode = parameters::even_odd_rows;
        break;
    case 8:
        stereo_mode = parameters::even_odd_columns;
        break;
    case 9:
        stereo_mode = parameters::checkerboard;
        break;
    case 10:
        stereo_mode = parameters::hdmi_frame_pack;
        break;
    case 11:
        stereo_mode = parameters::red_cyan_monochrome;
        break;
    case 12:
        stereo_mode = parameters::red_cyan_half_color;
        break;
    case 13:
        stereo_mode = parameters::red_cyan_full_color;
        break;
    case 14:
        stereo_mode = parameters::red_cyan_dubois;
        break;
    case 15:
        stereo_mode = parameters::green_magenta_monochrome;
        break;
    case 16:
        stereo_mode = parameters::green_magenta_half_color;
        break;
    case 17:
        stereo_mode = parameters::green_magenta_full_color;
        break;
    case 18:
        stereo_mode = parameters::green_magenta_dubois;
        break;
    case 19:
        stereo_mode = parameters::amber_blue_monochrome;
        break;
    case 20:
        stereo_mode = parameters::amber_blue_half_color;
        break;
    case 21:
        stereo_mode = parameters::amber_blue_full_color;
        break;
    case 22:
        stereo_mode = parameters::amber_blue_dubois;
        break;
    case 23:
        stereo_mode = parameters::red_green_monochrome;
        break;
    case 24:
        stereo_mode = parameters::red_blue_monochrome;
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
    case notification::subtitle_stream:
        s11n::load(current, stream);
        _lock = true;
        _subtitle_combobox->setCurrentIndex(stream + 1);
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


controls_widget::controls_widget(QSettings *settings, const player_init_data &init_data, QWidget *parent)
    : QWidget(parent), _lock(false), _settings(settings), _playing(false)
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
    row0_layout->setColumnStretch(0, 1);

    QGridLayout *row1_layout = new QGridLayout;
    _play_button = new QPushButton(get_icon("media-playback-start"), "");
    _play_button->setToolTip(_("<p>Play.</p>"));
    connect(_play_button, SIGNAL(pressed()), this, SLOT(play_pressed()));
    row1_layout->addWidget(_play_button, 1, 0);
    _pause_button = new QPushButton(get_icon("media-playback-pause"), "");
    _pause_button->setToolTip(_("<p>Pause.</p>"));
    connect(_pause_button, SIGNAL(pressed()), this, SLOT(pause_pressed()));
    row1_layout->addWidget(_pause_button, 1, 1);
    _stop_button = new QPushButton(get_icon("media-playback-stop"), "");
    _stop_button->setToolTip(_("<p>Stop.</p>"));
    connect(_stop_button, SIGNAL(pressed()), this, SLOT(stop_pressed()));
    row1_layout->addWidget(_stop_button, 1, 2);
    row1_layout->addWidget(new QWidget, 1, 3);
    _loop_button = new QPushButton(get_icon("media-playlist-repeat"), "");
    _loop_button->setToolTip(_("<p>Toggle loop mode.</p>"));
    _loop_button->setCheckable(true);
    _loop_button->setChecked(init_data.params.loop_mode != parameters::no_loop);
    connect(_loop_button, SIGNAL(toggled(bool)), this, SLOT(loop_pressed()));
    row1_layout->addWidget(_loop_button, 1, 4);
    row1_layout->addWidget(new QWidget, 1, 5);
    _fullscreen_button = new QPushButton(get_icon("view-fullscreen"), "");
    _fullscreen_button->setToolTip(_("<p>Switch to fullscreen mode. "
                "You can leave fullscreen mode by pressing the f key.</p>"));
    _fullscreen_button->setCheckable(true);
    connect(_fullscreen_button, SIGNAL(pressed()), this, SLOT(fullscreen_pressed()));
    row1_layout->addWidget(_fullscreen_button, 1, 6);
    _center_button = new QPushButton(get_icon("view-restore"), "");
    _center_button->setToolTip(_("<p>Center the video area on your screen.</p>"));
    connect(_center_button, SIGNAL(pressed()), this, SLOT(center_pressed()));
    row1_layout->addWidget(_center_button, 1, 7);
    row1_layout->addWidget(new QWidget, 1, 8);
    _bbb_button = new QPushButton(get_icon("media-seek-backward"), "");
    _bbb_button->setFixedSize(_bbb_button->minimumSizeHint());
    _bbb_button->setIconSize(_bbb_button->iconSize() * 12 / 10);
    _bbb_button->setToolTip(_("<p>Seek backward 10 minutes.</p>"));
    connect(_bbb_button, SIGNAL(pressed()), this, SLOT(bbb_pressed()));
    row1_layout->addWidget(_bbb_button, 1, 9);
    _bb_button = new QPushButton(get_icon("media-seek-backward"), "");
    _bb_button->setFixedSize(_bb_button->minimumSizeHint());
    _bb_button->setToolTip(_("<p>Seek backward 1 minute.</p>"));
    connect(_bb_button, SIGNAL(pressed()), this, SLOT(bb_pressed()));
    row1_layout->addWidget(_bb_button, 1, 10);
    _b_button = new QPushButton(get_icon("media-seek-backward"), "");
    _b_button->setFixedSize(_b_button->minimumSizeHint());
    _b_button->setIconSize(_b_button->iconSize() * 8 / 10);
    _b_button->setToolTip(_("<p>Seek backward 10 seconds.</p>"));
    connect(_b_button, SIGNAL(pressed()), this, SLOT(b_pressed()));
    row1_layout->addWidget(_b_button, 1, 11);
    _f_button = new QPushButton(get_icon("media-seek-forward"), "");
    _f_button->setFixedSize(_f_button->minimumSizeHint());
    _f_button->setIconSize(_f_button->iconSize() * 8 / 10);
    _f_button->setToolTip(_("<p>Seek forward 10 seconds.</p>"));
    connect(_f_button, SIGNAL(pressed()), this, SLOT(f_pressed()));
    row1_layout->addWidget(_f_button, 1, 12);
    _ff_button = new QPushButton(get_icon("media-seek-forward"), "");
    _ff_button->setFixedSize(_ff_button->minimumSizeHint());
    _ff_button->setToolTip(_("<p>Seek forward 1 minute.</p>"));
    connect(_ff_button, SIGNAL(pressed()), this, SLOT(ff_pressed()));
    row1_layout->addWidget(_ff_button, 1, 13);
    _fff_button = new QPushButton(get_icon("media-seek-forward"), "");
    _fff_button->setFixedSize(_fff_button->minimumSizeHint());
    _fff_button->setIconSize(_fff_button->iconSize() * 12 / 10);
    _fff_button->setToolTip(_("<p>Seek forward 10 minutes.</p>"));
    connect(_fff_button, SIGNAL(pressed()), this, SLOT(fff_pressed()));
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

void controls_widget::loop_pressed()
{
    if (!_lock)
    {
        parameters::loop_mode_t loop_mode = _loop_button->isChecked()
            ? parameters::loop_current : parameters::no_loop;
        send_cmd(command::set_loop_mode, static_cast<int>(loop_mode));
    }
}

void controls_widget::fullscreen_pressed()
{
    if (!_lock)
    {
        send_cmd(command::toggle_fullscreen);
    }
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

void controls_widget::update(const player_init_data &, bool have_valid_input, bool playing, int64_t input_duration)
{
    if (have_valid_input)
    {
        _play_button->setDefault(true);
        _loop_button->setEnabled(true);
        _fullscreen_button->setEnabled(true);
        receive_notification(notification(notification::play, !playing, playing));
        _input_duration = input_duration;
        std::string hr_duration = str::human_readable_time(_input_duration);
        _pos_label->setText((hr_duration + '/' + hr_duration).c_str());
        _pos_label->setMinimumSize(_pos_label->minimumSizeHint());
        _pos_label->setText(hr_duration.c_str());
    }
    else
    {
        _playing = false;
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
        if (flag)
        {
            _lock = true;
            parameters::loop_mode_t loop_mode = _loop_button->isChecked()
                ? parameters::loop_current : parameters::no_loop;
            send_cmd(command::set_loop_mode, static_cast<int>(loop_mode));
            _lock = false;
        }
        if (flag && _fullscreen_button->isChecked())
        {
            _lock = true;
            send_cmd(command::toggle_fullscreen);
            _lock = false;
        }
        _center_button->setEnabled(flag);
        _bbb_button->setEnabled(flag);
        _bb_button->setEnabled(flag);
        _b_button->setEnabled(flag);
        _f_button->setEnabled(flag);
        _ff_button->setEnabled(flag);
        _fff_button->setEnabled(flag);
        _seek_slider->setEnabled(flag);
        _pos_label->setEnabled(flag);
        if (!flag)
        {
            _seek_slider->setValue(0);
            _pos_label->setText(str::human_readable_time(_input_duration).c_str());
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
            _pos_label->setText((str::human_readable_time(
                            static_cast<int64_t>(value * 1000.0f) * _input_duration / 1000)
                        + '/' + str::human_readable_time(_input_duration)).c_str());
            _lock = false;
        }
        break;
    case notification::fullscreen:
        s11n::load(current, flag);
        _lock = true;
        _fullscreen_button->setChecked(!flag);
        _lock = false;
        break;
    default:
        break;
    }
}


color_dialog::color_dialog(const parameters &params, QWidget *parent) : QDialog(parent),
    _lock(false)
{
    setModal(false);
    setWindowTitle(_("Display Color Adjustments"));

    QLabel *c_label = new QLabel(_("Contrast:"));
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
    QLabel *b_label = new QLabel(_("Brightness:"));
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
    QLabel *h_label = new QLabel(_("Hue:"));
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
    QLabel *s_label = new QLabel(_("Saturation:"));
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

    QPushButton *ok_button = new QPushButton(_("OK"));
    connect(ok_button, SIGNAL(pressed()), this, SLOT(close()));

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
    setWindowTitle(_("Display Crosstalk Calibration"));

    /* TRANSLATORS: Please keep the lines short using <br> where necessary. */
    QLabel *rtfm_label = new QLabel(_("<p>Please read the manual to find out<br>"
                "how to measure the crosstalk levels<br>"
                "of your display.</p>"));

    QLabel *r_label = new QLabel(_("Red:"));
    _r_spinbox = new QDoubleSpinBox();
    _r_spinbox->setRange(0.0, +1.0);
    _r_spinbox->setValue(params->crosstalk_r);
    _r_spinbox->setDecimals(2);
    _r_spinbox->setSingleStep(0.01);
    connect(_r_spinbox, SIGNAL(valueChanged(double)), this, SLOT(spinbox_changed()));
    QLabel *g_label = new QLabel(_("Green:"));
    _g_spinbox = new QDoubleSpinBox();
    _g_spinbox->setRange(0.0, +1.0);
    _g_spinbox->setValue(params->crosstalk_g);
    _g_spinbox->setDecimals(2);
    _g_spinbox->setSingleStep(0.01);
    connect(_g_spinbox, SIGNAL(valueChanged(double)), this, SLOT(spinbox_changed()));
    QLabel *b_label = new QLabel(_("Blue:"));
    _b_spinbox = new QDoubleSpinBox();
    _b_spinbox->setRange(0.0, +1.0);
    _b_spinbox->setValue(params->crosstalk_b);
    _b_spinbox->setDecimals(2);
    _b_spinbox->setSingleStep(0.01);
    connect(_b_spinbox, SIGNAL(valueChanged(double)), this, SLOT(spinbox_changed()));

    QPushButton *ok_button = new QPushButton(_("OK"));
    connect(ok_button, SIGNAL(pressed()), this, SLOT(close()));

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


subtitle_dialog::subtitle_dialog(parameters *params, QWidget *parent) : QDialog(parent),
    _params(params), _lock(false)
{
    setModal(false);
    setWindowTitle(_("Subtitle Settings"));

    QLabel *info_label = new QLabel(_("<p>These settings apply to soft subtitles, but not to bitmaps.</p>"));

    _encoding_checkbox = new QCheckBox(_("Encoding:"));
    _encoding_checkbox->setToolTip(_("<p>Set the subtitle character set encoding.</p>"));
    _encoding_checkbox->setChecked(params->subtitle_encoding != "");
    connect(_encoding_checkbox, SIGNAL(stateChanged(int)), this, SLOT(encoding_changed()));
    _encoding_combobox = new QComboBox();
    _encoding_combobox->setToolTip(_encoding_checkbox->toolTip());
    QList<QTextCodec *> codecs = find_codecs();
    foreach (QTextCodec *codec, codecs)
    {
        _encoding_combobox->addItem(codec->name(), codec->mibEnum());
    }
    _encoding_combobox->setCurrentIndex(_encoding_combobox->findText(
                params->subtitle_encoding == "" ? "UTF-8" : params->subtitle_encoding.c_str()));
    connect(_encoding_combobox, SIGNAL(currentIndexChanged(int)), this, SLOT(encoding_changed()));

    _font_checkbox = new QCheckBox(_("Override font:"));
    _font_checkbox->setToolTip(_("<p>Override the subtitle font family.</p>"));
    _font_checkbox->setChecked(params->subtitle_font != "");
    connect(_font_checkbox, SIGNAL(stateChanged(int)), this, SLOT(font_changed()));
    _font_combobox = new QFontComboBox();
    _font_combobox->setToolTip(_font_checkbox->toolTip());
    _font_combobox->setCurrentFont(QFont(QString(params->subtitle_font == "" ? "sans-serif" : params->subtitle_font.c_str())));
    connect(_font_combobox, SIGNAL(currentFontChanged(const QFont &)), this, SLOT(font_changed()));

    _size_checkbox = new QCheckBox(_("Override font size:"));
    _size_checkbox->setToolTip(_("<p>Override the subtitle font size.</p>"));
    _size_checkbox->setChecked(params->subtitle_size > 0);
    connect(_size_checkbox, SIGNAL(stateChanged(int)), this, SLOT(size_changed()));
    _size_spinbox = new QSpinBox();
    _size_spinbox->setToolTip(_size_checkbox->toolTip());
    _size_spinbox->setRange(1, 999);
    _size_spinbox->setValue(params->subtitle_size <= 0 ? 12 : params->subtitle_size);
    connect(_size_spinbox, SIGNAL(valueChanged(int)), this, SLOT(size_changed()));

    _scale_checkbox = new QCheckBox(_("Override scale factor:"));
    _scale_checkbox->setToolTip(_("<p>Override the subtitle scale factor.</p>"));
    _scale_checkbox->setChecked(params->subtitle_scale >= 0.0f);
    connect(_scale_checkbox, SIGNAL(stateChanged(int)), this, SLOT(scale_changed()));
    _scale_spinbox = new QDoubleSpinBox();
    _scale_spinbox->setToolTip(_scale_checkbox->toolTip());
    _scale_spinbox->setRange(0.01, 100.0);
    _scale_spinbox->setSingleStep(0.1);
    _scale_spinbox->setValue(params->subtitle_scale < 0.0f ? 1.0f : params->subtitle_scale);
    connect(_scale_spinbox, SIGNAL(valueChanged(double)), this, SLOT(scale_changed()));

    _color_checkbox = new QCheckBox(_("Override color:"));
    _color_checkbox->setToolTip(_("<p>Override the subtitle color.</p>"));
    _color_checkbox->setChecked(params->subtitle_color <= std::numeric_limits<uint32_t>::max());
    connect(_color_checkbox, SIGNAL(stateChanged(int)), this, SLOT(color_changed()));
    _color_button = new QPushButton();
    _color_button->setToolTip(_color_checkbox->toolTip());
    set_color_button(params->subtitle_color > std::numeric_limits<uint32_t>::max() ? 0xffffffu : params->subtitle_color);
    connect(_color_button, SIGNAL(pressed()), this, SLOT(color_button_pressed()));

    QPushButton *ok_button = new QPushButton(_("OK"));
    connect(ok_button, SIGNAL(pressed()), this, SLOT(close()));

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
    layout->addWidget(ok_button, 6, 0, 1, 2);
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

void subtitle_dialog::color_button_pressed()
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
        /* Also set in init data, because this must also work in the absence of a player
         * that interpretes the above command (i.e. when no video is currently playing). */
        _params->subtitle_encoding = encoding;
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
        /* Also set in init data, because this must also work in the absence of a player
         * that interpretes the above command (i.e. when no video is currently playing). */
        _params->subtitle_font = font;
    }
}

void subtitle_dialog::size_changed()
{
    if (!_lock)
    {
        int size = _size_checkbox->isChecked() ? _size_spinbox->value() : -1;
        std::ostringstream v;
        s11n::save(v, size);
        send_cmd(command::set_subtitle_size, v.str());
        /* Also set in init data, because this must also work in the absence of a player
         * that interpretes the above command (i.e. when no video is currently playing). */
        _params->subtitle_size = size;
    }
}

void subtitle_dialog::scale_changed()
{
    if (!_lock)
    {
        float scale = _scale_checkbox->isChecked() ? _scale_spinbox->value() : -1.0f;
        std::ostringstream v;
        s11n::save(v, scale);
        send_cmd(command::set_subtitle_scale, v.str());
        /* Also set in init data, because this must also work in the absence of a player
         * that interpretes the above command (i.e. when no video is currently playing). */
        _params->subtitle_scale = scale;
    }
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
        std::ostringstream v;
        s11n::save(v, color);
        send_cmd(command::set_subtitle_color, v.str());
        /* Also set in init data, because this must also work in the absence of a player
         * that interpretes the above command (i.e. when no video is currently playing). */
        _params->subtitle_color = color;
    }
}

void subtitle_dialog::receive_notification(const notification &note)
{
    std::istringstream current(note.current);

    switch (note.type)
    {
    case notification::subtitle_encoding:
        {
            std::string encoding;
            s11n::load(current, encoding);
            _lock = true;
            _encoding_checkbox->setChecked(encoding != "");
            if (encoding != "")
            {
                _encoding_combobox->setCurrentIndex(_encoding_combobox->findText(encoding.c_str()));
            }
            _lock = false;
        }
        break;
    case notification::subtitle_font:
        {
            std::string font;
            s11n::load(current, font);
            _lock = true;
            _font_checkbox->setChecked(font != "");
            if (font != "")
            {
                _font_combobox->setCurrentFont(QFont(QString(font.c_str())));
            }
            _lock = false;
        }
        break;
    case notification::subtitle_size:
        {
            int size;
            s11n::load(current, size);
            _lock = true;
            _size_checkbox->setChecked(size > 0);
            if (size > 0)
            {
                _size_spinbox->setValue(size);
            }
            _lock = false;
        }
        break;
    case notification::subtitle_scale:
        {
            float scale;
            s11n::load(current, scale);
            _lock = true;
            _scale_checkbox->setChecked(scale >= 0.0f);
            if (scale >= 0.0f)
            {
                _scale_spinbox->setValue(scale);
            }
            _lock = false;
        }
        break;
    case notification::subtitle_color:
        {
            uint64_t color;
            s11n::load(current, color);
            _lock = true;
            _color_checkbox->setChecked(color <= std::numeric_limits<uint32_t>::max());
            if (color <= std::numeric_limits<uint32_t>::max())
            {
                set_color_button(color);
            }
            _lock = false;
        }
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
    setWindowTitle(_("Stereoscopic Video Settings"));

    QLabel *p_label = new QLabel(_("Parallax:"));
    p_label->setToolTip(_("<p>Adjust parallax, from -1 to +1. This changes the separation of left and right view, "
                "and thus the perceived distance of the scene.</p>"));
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

    QLabel *sp_label = new QLabel(_("Subtitle parallax:"));
    sp_label->setToolTip(_("<p>Adjust subtitle parallax, from -1 to +1. This changes the perceived distance "
                "of the subtitles.</p>"));
    _sp_slider = new QSlider(Qt::Horizontal);
    _sp_slider->setToolTip(sp_label->toolTip());
    _sp_slider->setRange(-1000, 1000);
    _sp_slider->setValue(params.subtitle_parallax * 1000.0f);
    connect(_sp_slider, SIGNAL(valueChanged(int)), this, SLOT(sp_slider_changed(int)));
    _sp_spinbox = new QDoubleSpinBox();
    _sp_spinbox->setToolTip(sp_label->toolTip());
    _sp_spinbox->setRange(-1.0, +1.0);
    _sp_spinbox->setValue(params.subtitle_parallax);
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
    _g_slider->setValue(params.ghostbust * 1000.0f);
    connect(_g_slider, SIGNAL(valueChanged(int)), this, SLOT(g_slider_changed(int)));
    _g_spinbox = new QDoubleSpinBox();
    _g_spinbox->setToolTip(g_label->toolTip());
    _g_spinbox->setRange(0.0, +1.0);
    _g_spinbox->setValue(params.ghostbust);
    _g_spinbox->setDecimals(2);
    _g_spinbox->setSingleStep(0.01);
    connect(_g_spinbox, SIGNAL(valueChanged(double)), this, SLOT(g_spinbox_changed(double)));

    QPushButton *ok_button = new QPushButton(_("OK"));
    connect(ok_button, SIGNAL(pressed()), this, SLOT(close()));

    QGridLayout *layout = new QGridLayout;
    layout->addWidget(p_label, 0, 0);
    layout->addWidget(_p_slider, 0, 1);
    layout->addWidget(_p_spinbox, 0, 2);
    layout->addWidget(sp_label, 1, 0);
    layout->addWidget(_sp_slider, 1, 1);
    layout->addWidget(_sp_spinbox, 1, 2);
    layout->addWidget(g_label, 2, 0);
    layout->addWidget(_g_slider, 2, 1);
    layout->addWidget(_g_spinbox, 2, 2);
    layout->addWidget(ok_button, 3, 0, 1, 3);
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

void stereoscopic_dialog::sp_slider_changed(int val)
{
    if (!_lock)
    {
        send_cmd(command::set_subtitle_parallax, val / 1000.0f);
    }
}

void stereoscopic_dialog::sp_spinbox_changed(double val)
{
    if (!_lock)
    {
        send_cmd(command::set_subtitle_parallax, static_cast<float>(val));
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
    case notification::subtitle_parallax:
        s11n::load(current, value);
        _lock = true;
        _sp_slider->setValue(value * 1000.0f);
        _sp_spinbox->setValue(value);
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

open_device_dialog::open_device_dialog(
        const QStringList &default_devices,
        const QStringList &firewire_devices,
        const QString &x11_device,
        const device_request &dev_request,
        QWidget *parent) :
    QDialog(parent)
{
    setWindowTitle(_("Open device"));

    _type_combobox = new QComboBox();
    _type_combobox->setToolTip(_("<p>Choose a device type.</p>"));
    _type_combobox->addItem(_("Default"));
    _type_combobox->addItem(_("Firewire"));
    _type_combobox->addItem(_("X11"));
    _default_device_combobox = new QComboBox();
    _default_device_combobox->setToolTip(_("<p>Choose a device.</p>"));
    _default_device_combobox->addItems(default_devices);
    _firewire_device_combobox = new QComboBox();
    _firewire_device_combobox->setToolTip(_("<p>Choose a device.</p>"));
    _firewire_device_combobox->addItems(firewire_devices);
    _x11_device_field = new QLineEdit(x11_device);
    _x11_device_field->setToolTip(_("<p>Set the X11 device string. "
                "Refer to the manual for details.</p>"));
    _device_chooser_stack = new QStackedWidget();
    _device_chooser_stack->addWidget(_default_device_combobox);
    _device_chooser_stack->addWidget(_firewire_device_combobox);
    _device_chooser_stack->addWidget(_x11_device_field);
    connect(_type_combobox, SIGNAL(currentIndexChanged(int)), _device_chooser_stack, SLOT(setCurrentIndex(int)));
    _type_combobox->setCurrentIndex(dev_request.device == device_request::firewire ? 1
            : dev_request.device == device_request::x11 ? 2 : 0);
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
    QPushButton *cancel_btn = new QPushButton(_("Cancel"));
    connect(cancel_btn, SIGNAL(pressed()), this, SLOT(reject()));
    QPushButton *ok_btn = new QPushButton(_("OK"));
    connect(ok_btn, SIGNAL(pressed()), this, SLOT(accept()));

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
    layout->addWidget(_device_chooser_stack, 1, 0, 1, 2);
    layout->addWidget(_frame_size_groupbox, 2, 0, 1, 2);
    layout->addWidget(_frame_rate_groupbox, 3, 0, 1, 2);
    layout->addWidget(cancel_btn, 4, 0);
    layout->addWidget(ok_btn, 4, 1);
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

void open_device_dialog::request(QString &device, device_request &dev_request)
{
    if (_type_combobox->currentIndex() == 1)
    {
        dev_request.device = device_request::firewire;
        device = _firewire_device_combobox->currentText();
    }
    else if (_type_combobox->currentIndex() == 2)
    {
        dev_request.device = device_request::x11;
        device = _x11_device_field->text();
    }
    else
    {
        dev_request.device = device_request::sys_default;
        device = _default_device_combobox->currentText();
    }
    dev_request.width = _frame_size_groupbox->isChecked() ? _frame_width_spinbox->value() : 0;
    dev_request.height = _frame_size_groupbox->isChecked() ? _frame_height_spinbox->value() : 0;
    dev_request.frame_rate_num = _frame_rate_groupbox->isChecked() ? _frame_rate_num_spinbox->value() : 0;
    dev_request.frame_rate_den = _frame_rate_groupbox->isChecked() ? _frame_rate_den_spinbox->value() : 0;
}

main_window::main_window(QSettings *settings, const player_init_data &init_data) :
    _settings(settings),
    _color_dialog(NULL),
    _crosstalk_dialog(NULL),
    _subtitle_dialog(NULL),
    _stereoscopic_dialog(NULL),
    _player(NULL),
    _init_data(init_data),
    _init_data_template(init_data),
    _stop_request(false)
{
    // Application properties
    setWindowTitle(PACKAGE_NAME);
    setWindowIcon(QIcon(":logo/64x64/bino.png"));

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
    if (_init_data.params.subtitle_encoding.length() == 1 && _init_data.params.subtitle_encoding[0] == '\0')
    {
        _init_data.params.subtitle_encoding = _settings->value("subtitle-encoding", QString(1, '\0')).toString().toLocal8Bit().constData();
    }
    if (_init_data.params.subtitle_font.length() == 1 && _init_data.params.subtitle_font[0] == '\0')
    {
        _init_data.params.subtitle_font = _settings->value("subtitle-font", QString(1, '\0')).toString().toLocal8Bit().constData();
    }
    if (_init_data.params.subtitle_size < 0)
    {
        _init_data.params.subtitle_size = _settings->value("subtitle-size", QString("-1")).toInt();
    }
    if (_init_data.params.subtitle_scale < 0.0f)
    {
        _init_data.params.subtitle_scale = _settings->value("subtitle-scale", QString("-1")).toFloat();
    }
    if (_init_data.params.subtitle_color > std::numeric_limits<uint32_t>::max())
    {
        _init_data.params.subtitle_color = _settings->value("subtitle-color",
                QString(str::from(std::numeric_limits<uint64_t>::max()).c_str())).toULongLong();
    }
    if (_init_data.params.fullscreen_screens < 0)
    {
        _init_data.params.fullscreen_screens = _settings->value("fullscreen-screens", QString("-1")).toInt();
    }
    if (_init_data.params.fullscreen_flip_left < 0)
    {
        _init_data.params.fullscreen_flip_left = _settings->value("fullscreen-flip-left", QString("-1")).toInt();
    }
    if (_init_data.params.fullscreen_flop_left < 0)
    {
        _init_data.params.fullscreen_flop_left = _settings->value("fullscreen-flop-left", QString("-1")).toInt();
    }
    if (_init_data.params.fullscreen_flip_right < 0)
    {
        _init_data.params.fullscreen_flip_right = _settings->value("fullscreen-flip-right", QString("-1")).toInt();
    }
    if (_init_data.params.fullscreen_flop_right < 0)
    {
        _init_data.params.fullscreen_flop_right = _settings->value("fullscreen-flop-right", QString("-1")).toInt();
    }
    _settings->endGroup();
    _init_data.params.set_defaults();

    // Central widget, player, and timer
    QWidget *central_widget = new QWidget(this);
    QGridLayout *layout = new QGridLayout();
    _video_container_widget = new video_container_widget(central_widget);
    connect(_video_container_widget, SIGNAL(move_event()), this, SLOT(move_event()));
    layout->addWidget(_video_container_widget, 0, 0);
    _video_output = new video_output_qt(_init_data.benchmark, _video_container_widget);
    _player = new player_qt_internal(_video_output);
    _timer = new QTimer(this);
    connect(_timer, SIGNAL(timeout()), this, SLOT(playloop_step()));
    _in_out_widget = new in_out_widget(_settings, _player, central_widget);
    layout->addWidget(_in_out_widget, 1, 0);
    _controls_widget = new controls_widget(_settings, _init_data, central_widget);
    layout->addWidget(_controls_widget, 2, 0);
    layout->setRowStretch(0, 1);
    layout->setColumnStretch(0, 1);
    central_widget->setLayout(layout);
    setCentralWidget(central_widget);

    // Menus
    QMenu *file_menu = menuBar()->addMenu(_("&File"));
    QAction *file_open_act = new QAction(_("&Open..."), this);
    file_open_act->setShortcut(QKeySequence::Open);
    file_open_act->setIcon(get_icon("document-open"));
    connect(file_open_act, SIGNAL(triggered()), this, SLOT(file_open()));
    file_menu->addAction(file_open_act);
    QAction *file_open_url_act = new QAction(_("Open &URL..."), this);
    file_open_url_act->setIcon(get_icon("document-open"));
    connect(file_open_url_act, SIGNAL(triggered()), this, SLOT(file_open_url()));
    file_menu->addAction(file_open_url_act);
    QAction *file_open_device_act = new QAction(_("Open &device..."), this);
    file_open_device_act->setIcon(get_icon("camera-web"));
    connect(file_open_device_act, SIGNAL(triggered()), this, SLOT(file_open_device()));
    file_menu->addAction(file_open_device_act);
    file_menu->addSeparator();
    QAction *file_quit_act = new QAction(_("&Quit..."), this);
    file_quit_act->setShortcut(QKeySequence::Quit);
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
    QAction *preferences_subtitle_act = new QAction(_("&Subtitle Settings..."), this);
    connect(preferences_subtitle_act, SIGNAL(triggered()), this, SLOT(preferences_subtitle()));
    preferences_menu->addAction(preferences_subtitle_act);
    preferences_menu->addSeparator();
    QAction *preferences_stereoscopic_act = new QAction(_("Stereoscopic Video Settings..."), this);
    connect(preferences_stereoscopic_act, SIGNAL(triggered()), this, SLOT(preferences_stereoscopic()));
    preferences_menu->addAction(preferences_stereoscopic_act);
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

    // Update widget contents
    _in_out_widget->update(_init_data, false, false);
    _controls_widget->update(_init_data, false, false, -1);

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
        open(urls, init_data.dev_request);
    }

    // Start the event and play loop
    _timer->start(0);
}

main_window::~main_window()
{
    if (_player)
    {
        try { _player->close(); } catch (...) { }
        delete _player;
    }
    delete _video_output;
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
        QMessageBox::critical(this, _("Error"), e.what());
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
            _init_data.audio_stream = std::max(0, _in_out_widget->get_audio_stream());
            _init_data.subtitle_stream = std::max(-1, _in_out_widget->get_subtitle_stream());
            _init_data.stereo_mode_override = true;
            _in_out_widget->get_stereo_mode(_init_data.stereo_mode, _init_data.stereo_mode_swap);
            if (!open_player())
            {
                _stop_request = true;
            }
            // Remember the settings of this video
            _settings->beginGroup("Video/" + current_file_hash());
            _settings->setValue("stereo-layout", QString(video_frame::stereo_layout_to_string(_init_data.stereo_layout, _init_data.stereo_layout_swap).c_str()));
            _settings->setValue("video-stream", QVariant(_init_data.video_stream).toString());
            _settings->setValue("audio-stream", QVariant(_init_data.audio_stream).toString());
            _settings->setValue("subtitle-stream", QVariant(_init_data.subtitle_stream).toString());
            _settings->setValue("parallax", QVariant(_init_data.params.parallax).toString());
            _settings->setValue("ghostbust", QVariant(_init_data.params.ghostbust).toString());
            _settings->setValue("subtitle-parallax", QVariant(_init_data.params.subtitle_parallax).toString());
            _settings->endGroup();
            // Remember the 2D or 3D video output mode.
            _settings->setValue((_init_data.stereo_layout == video_frame::mono ? "Session/2d-stereo-mode" : "Session/3d-stereo-mode"),
                    QString(parameters::stereo_mode_to_string(_init_data.stereo_mode, _init_data.stereo_mode_swap).c_str()));
            // Update widgets: we're now playing
            _in_out_widget->update(_init_data, true, true);
            _controls_widget->update(_init_data, true, true, _player->get_media_input().duration());
            // Give the keyboard focus to the video widget
            _player->get_video_output()->grab_focus();
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

    case notification::subtitle_stream:
        s11n::load(current, _init_data.subtitle_stream);
        _settings->beginGroup("Video/" + current_file_hash());
        _settings->setValue("subtitle-stream", QVariant(_init_data.subtitle_stream).toString());
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

    case notification::subtitle_encoding:
        s11n::load(current, _init_data.params.subtitle_encoding);
        break;

    case notification::subtitle_font:
        s11n::load(current, _init_data.params.subtitle_font);
        break;

    case notification::subtitle_size:
        s11n::load(current, _init_data.params.subtitle_size);
        break;

    case notification::subtitle_scale:
        s11n::load(current, _init_data.params.subtitle_scale);
        break;

    case notification::subtitle_color:
        s11n::load(current, _init_data.params.subtitle_color);
        break;

    case notification::subtitle_parallax:
        s11n::load(current, _init_data.params.subtitle_parallax);
        _settings->beginGroup("Video/" + current_file_hash());
        _settings->setValue("subtitle-parallax", QVariant(_init_data.params.subtitle_parallax).toString());
        _settings->endGroup();
        break;

    case notification::loop_mode:
        {
            int loop_mode;
            s11n::load(current, loop_mode);
            _init_data.params.loop_mode = static_cast<parameters::loop_mode_t>(loop_mode);
        }
        break;

    case notification::fullscreen_screens:
        s11n::load(current, _init_data.params.fullscreen_screens);
        break;

    case notification::fullscreen_flip_left:
        s11n::load(current, _init_data.params.fullscreen_flip_left);
        break;

    case notification::fullscreen_flop_left:
        s11n::load(current, _init_data.params.fullscreen_flop_left);
        break;

    case notification::fullscreen_flip_right:
        s11n::load(current, _init_data.params.fullscreen_flip_right);
        break;

    case notification::fullscreen_flop_right:
        s11n::load(current, _init_data.params.fullscreen_flop_right);
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
    // Stop the event and play loop
    _timer->stop();
    // Remember the Session preferences
    _settings->beginGroup("Session");
    _settings->setValue("contrast", QVariant(_init_data.params.contrast).toString());
    _settings->setValue("brightness", QVariant(_init_data.params.brightness).toString());
    _settings->setValue("hue", QVariant(_init_data.params.hue).toString());
    _settings->setValue("saturation", QVariant(_init_data.params.saturation).toString());
    _settings->setValue("crosstalk_r", QVariant(_init_data.params.crosstalk_r).toString());
    _settings->setValue("crosstalk_g", QVariant(_init_data.params.crosstalk_g).toString());
    _settings->setValue("crosstalk_b", QVariant(_init_data.params.crosstalk_b).toString());
    _settings->setValue("subtitle-encoding", QVariant(_init_data.params.subtitle_encoding.c_str()).toString());
    _settings->setValue("subtitle-font", QVariant(_init_data.params.subtitle_font.c_str()).toString());
    _settings->setValue("subtitle-size", QVariant(_init_data.params.subtitle_size).toString());
    _settings->setValue("subtitle-scale", QVariant(_init_data.params.subtitle_scale).toString());
    _settings->setValue("subtitle-color", QVariant(static_cast<qulonglong>(_init_data.params.subtitle_color)).toString());
    _settings->setValue("fullscreen-screens", QVariant(_init_data.params.fullscreen_screens).toString());
    _settings->setValue("fullscreen-flip-left", QVariant(_init_data.params.fullscreen_flip_left).toString());
    _settings->setValue("fullscreen-flop-left", QVariant(_init_data.params.fullscreen_flop_left).toString());
    _settings->setValue("fullscreen-flip-right", QVariant(_init_data.params.fullscreen_flip_right).toString());
    _settings->setValue("fullscreen-flop-right", QVariant(_init_data.params.fullscreen_flop_right).toString());
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
    if (_player->is_playing() && _stop_request)
    {
        _player->force_stop();
        _in_out_widget->update(_init_data, false, false);
        _controls_widget->update(_init_data, false, false, -1);
        _stop_request = false;
    }
    else if (_player->is_playing() && !_stop_request)
    {
        try
        {
            _player->playloop_step();
        }
        catch (std::exception &e)
        {
            send_cmd(command::toggle_play);
            _player->playloop_step();   // react on command
            QMessageBox::critical(this, "Error", e.what());
        }
    }
    else
    {
        /* Process controller events. When we're playing, the player does this. */
        controller::process_all_events();
        usleep(5000);   // don't busy loop for controller events
    }
}

void main_window::open(QStringList filenames, const device_request &dev_request)
{
    _player->force_stop();
    _player->close();
    parameters params_bak = _init_data.params;
    _init_data = _init_data_template;
    _init_data.params = params_bak;
    _init_data.dev_request = dev_request;
    _init_data.urls.clear();
    for (int i = 0; i < filenames.size(); i++)
    {
        _init_data.urls.push_back(filenames[i].toLocal8Bit().constData());
    }
    if (open_player())
    {
        // Get the settings for this video
        _settings->beginGroup("Video/" + current_file_hash());
        QString layout_fallback = QString(video_frame::stereo_layout_to_string(
                    _player->get_media_input().video_frame_template().stereo_layout,
                    _player->get_media_input().video_frame_template().stereo_layout_swap).c_str());
        QString layout_name = _settings->value("stereo-layout", layout_fallback).toString();
        video_frame::stereo_layout_from_string(layout_name.toStdString(), _init_data.stereo_layout, _init_data.stereo_layout_swap);
        _init_data.stereo_layout_override = true;
        _init_data.video_stream = QVariant(_settings->value("video-stream", QVariant(_init_data.video_stream)).toString()).toInt();
        _init_data.video_stream = std::max(0, std::min(_init_data.video_stream, _player->get_media_input().video_streams() - 1));
        _init_data.audio_stream = QVariant(_settings->value("audio-stream", QVariant(_init_data.audio_stream)).toString()).toInt();
        _init_data.audio_stream = std::max(0, std::min(_init_data.audio_stream, _player->get_media_input().audio_streams() - 1));
        _init_data.subtitle_stream = QVariant(_settings->value("subtitle-stream", QVariant(_init_data.subtitle_stream)).toString()).toInt();
        _init_data.subtitle_stream = std::max(-1, std::min(_init_data.subtitle_stream, _player->get_media_input().subtitle_streams() - 1));
        _init_data.params.parallax = QVariant(_settings->value("parallax", QVariant(_init_data.params.parallax)).toString()).toFloat();
        _init_data.params.ghostbust = QVariant(_settings->value("ghostbust", QVariant(_init_data.params.ghostbust)).toString()).toFloat();
        _init_data.params.subtitle_parallax = QVariant(_settings->value("subtitle-parallax", QVariant(_init_data.params.parallax)).toString()).toFloat();
        _settings->endGroup();
        // Get stereo mode
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
        _controls_widget->update(_init_data, true, false, _player->get_media_input().duration());
    }
    else
    {
        _in_out_widget->update(_init_data, false, false);
        _controls_widget->update(_init_data, false, false, -1);
    }
}

void main_window::file_open()
{
    QStringList file_names = QFileDialog::getOpenFileNames(this, _("Open files"),
            _settings->value("Session/file-open-dir", QDir::currentPath()).toString());
    if (file_names.empty())
    {
        return;
    }
    _settings->setValue("Session/file-open-dir", QFileInfo(file_names[0]).path());
    open(file_names);
}

void main_window::file_open_url()
{
    QDialog *url_dialog = new QDialog(this);
    url_dialog->setWindowTitle(_("Open URL"));
    QLabel *url_label = new QLabel(_("URL:"));
    QLineEdit *url_edit = new QLineEdit("");
    url_edit->setMinimumWidth(256);
    QPushButton *cancel_btn = new QPushButton(_("Cancel"));
    QPushButton *ok_btn = new QPushButton(_("OK"));
    ok_btn->setDefault(true);
    connect(cancel_btn, SIGNAL(pressed()), url_dialog, SLOT(reject()));
    connect(ok_btn, SIGNAL(pressed()), url_dialog, SLOT(accept()));
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
        QString url = url_edit->text();
        open(QStringList(url));
    }
}

void main_window::file_open_device()
{
    // Gather list of available devices
    QStringList default_devices;
    QStringList firewire_devices;
    QString x11_device;
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
    x11_device = _settings->value("Session/device-x11", QString(":0.0+400,300")).toString();

    // Get saved settings
    device_request dev_request;
    _settings->beginGroup("Session");
    QString saved_type = _settings->value("device-type", QString("default")).toString();
    dev_request.device = (saved_type == "firewire" ? device_request::firewire
            : saved_type == "x11" ? device_request::x11 : device_request::sys_default);
    dev_request.width = _settings->value("device-request-frame-width", QString("0")).toInt();
    dev_request.height = _settings->value("device-request-frame-height", QString("0")).toInt();
    dev_request.frame_rate_num = _settings->value("device-request-frame-rate-num", QString("0")).toInt();
    dev_request.frame_rate_den = _settings->value("device-request-frame-rate-den", QString("0")).toInt();
    _settings->endGroup();

    // Run dialog
    open_device_dialog *dlg = new open_device_dialog(default_devices, firewire_devices, x11_device, dev_request, this);
    dlg->exec();
    if (dlg->result() != QDialog::Accepted)
    {
        return;
    }
    QString device;
    dlg->request(device, dev_request);

    // Save settings
    if (dev_request.device == device_request::x11)
    {
        _settings->setValue("Session/device-x11", device);
    }
    _settings->beginGroup("Session");
    _settings->setValue("device-type", QString(dev_request.device == device_request::firewire ? "firewire"
                : dev_request.device == device_request::x11 ? "x11" : "default"));
    _settings->setValue("device-request-frame-width", QVariant(dev_request.width).toString());
    _settings->setValue("device-request-frame-height", QVariant(dev_request.height).toString());
    _settings->setValue("device-request-frame-rate-num", QVariant(dev_request.frame_rate_num).toString());
    _settings->setValue("device-request-frame-rate-den", QVariant(dev_request.frame_rate_den).toString());
    _settings->endGroup();

    // Open device
    open(QStringList(device), dev_request);
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

void main_window::preferences_subtitle()
{
    if (!_subtitle_dialog)
    {
        _subtitle_dialog = new subtitle_dialog(&_init_data.params, this);
    }
    _subtitle_dialog->show();
    _subtitle_dialog->raise();
    _subtitle_dialog->activateWindow();
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

void main_window::preferences_fullscreen()
{
    int n = QApplication::desktop()->screenCount();

    QDialog *dlg = new QDialog(this);
    dlg->setWindowTitle(_("Fullscreen/Multiscreen Settings"));

    QLabel *lbl = new QLabel(_("Configure fullscreen mode:"));
    lbl->setToolTip(_("<p>Select the screens to use in fullscreen mode.</p>"));

    QRadioButton *single_btn = new QRadioButton(_("Single screen:"));
    single_btn->setToolTip(_("<p>Use a single screen for fullscreen mode.</p>"));
    QComboBox *single_box = new QComboBox();
    single_box->setToolTip(single_btn->toolTip());
    single_box->addItem(_("Primary screen"));
    if (n > 1)
    {
        for (int i = 0; i < n; i++)
        {
            single_box->addItem(str::asprintf(_("Screen %d"), i + 1).c_str());
        }
    }

    QRadioButton *dual_btn = new QRadioButton(_("Dual screen:"));
    dual_btn->setToolTip(_("<p>Use two screens for fullscreen mode.</p>"));
    QComboBox *dual_box0 = new QComboBox();
    dual_box0->setToolTip(dual_btn->toolTip());
    QComboBox *dual_box1 = new QComboBox();
    dual_box1->setToolTip(dual_btn->toolTip());
    if (n > 1)
    {
        for (int i = 0; i < n; i++)
        {
            dual_box0->addItem(str::asprintf(_("Screen %d"), i + 1).c_str());
            dual_box1->addItem(str::asprintf(_("Screen %d"), i + 1).c_str());
        }
    }

    QRadioButton *multi_btn = new QRadioButton(_("Multi screen:"));
    multi_btn->setToolTip(_("<p>Use multiple screens for fullscreen mode.</p>"));
    QLineEdit *multi_edt = new QLineEdit();
    multi_edt->setToolTip(_("<p>Comma-separated list of screens to use for fullscreen mode.</p>"));
    QRegExp rx("\\d{1,2}(,\\d{1,2}){0,15}");
    multi_edt->setValidator(new QRegExpValidator(rx, 0));

    QLabel *lbl2 = new QLabel(_("When in fullscreen mode,"));
    lbl2->setToolTip(_("<p>Set special left/right view handling for fullscreen mode.</p>"));

    QCheckBox *flip_left_box = new QCheckBox(_("flip left view vertically."));
    flip_left_box->setToolTip(lbl2->toolTip());
    QCheckBox *flop_left_box = new QCheckBox(_("flop left view horizontally."));
    flop_left_box->setToolTip(lbl2->toolTip());
    QCheckBox *flip_right_box = new QCheckBox(_("flip right view vertically."));
    flip_right_box->setToolTip(lbl2->toolTip());
    QCheckBox *flop_right_box = new QCheckBox(_("flop right view horizontally."));
    flop_right_box->setToolTip(lbl2->toolTip());

    QPushButton *cancel_btn = new QPushButton(_("Cancel"));
    QPushButton *ok_btn = new QPushButton(_("OK"));
    ok_btn->setDefault(true);
    connect(cancel_btn, SIGNAL(pressed()), dlg, SLOT(reject()));
    connect(ok_btn, SIGNAL(pressed()), dlg, SLOT(accept()));

    QGridLayout *layout0 = new QGridLayout();
    layout0->addWidget(lbl, 0, 0, 1, 3);
    layout0->addWidget(single_btn, 1, 0);
    layout0->addWidget(single_box, 1, 1, 1, 2);
    layout0->addWidget(dual_btn, 2, 0);
    layout0->addWidget(dual_box0, 2, 1);
    layout0->addWidget(dual_box1, 2, 2);
    layout0->addWidget(multi_btn, 3, 0);
    layout0->addWidget(multi_edt, 3, 1, 1, 2);
    layout0->addWidget(lbl2, 4, 0, 1, 3);
    layout0->addWidget(flip_left_box, 5, 0, 1, 3);
    layout0->addWidget(flop_left_box, 6, 0, 1, 3);
    layout0->addWidget(flip_right_box, 7, 0, 1, 3);
    layout0->addWidget(flop_right_box, 8, 0, 1, 3);
    QGridLayout *layout1 = new QGridLayout();
    layout1->addWidget(cancel_btn, 0, 0);
    layout1->addWidget(ok_btn, 0, 1);
    QGridLayout *layout = new QGridLayout();
    layout->addLayout(layout0, 0, 0);
    layout->addLayout(layout1, 1, 0);
    dlg->setLayout(layout);

    // Set initial values from fullscreen_screens value
    if (n < 3)
    {
        multi_btn->setEnabled(false);
        multi_edt->setEnabled(false);
    }
    else
    {
        multi_edt->setText("1,2,3");
    }
    if (n < 2)
    {
        dual_btn->setEnabled(false);
        dual_box0->setEnabled(false);
        dual_box1->setEnabled(false);
    }
    else
    {
        dual_box0->setCurrentIndex(0);
        dual_box1->setCurrentIndex(1);
    }
    std::vector<int> conf_screens;
    for (int i = 0; i < 16; i++)
    {
        if (_init_data.params.fullscreen_screens & (1 << i))
        {
            conf_screens.push_back(i);
        }
    }
    if (conf_screens.size() >= 3 && n >= 3)
    {
        QString screen_list;
        for (size_t i = 0; i < conf_screens.size(); i++)
        {
            screen_list += str::from(conf_screens[i] + 1).c_str();
            if (i < conf_screens.size() - 1)
            {
                screen_list += ',';
            }
        }
        multi_btn->setChecked(true);
        multi_edt->setText(screen_list);
    }
    else if (conf_screens.size() == 2 && n >= 2)
    {
        dual_box0->setCurrentIndex(conf_screens[0]);
        dual_box1->setCurrentIndex(conf_screens[1]);
        dual_btn->setChecked(true);
    }
    else
    {
        if (conf_screens.size() > 0 && conf_screens[0] < n)
        {
            single_box->setCurrentIndex(conf_screens[0] + 1);
        }
        else
        {
            single_box->setCurrentIndex(0);
        }
        single_btn->setChecked(true);
    }
    flip_left_box->setChecked(_init_data.params.fullscreen_flip_left);
    flop_left_box->setChecked(_init_data.params.fullscreen_flop_left);
    flip_right_box->setChecked(_init_data.params.fullscreen_flip_right);
    flop_right_box->setChecked(_init_data.params.fullscreen_flop_right);

    dlg->exec();
    if (dlg->result() == QDialog::Accepted)
    {
        if (single_btn->isChecked())
        {
            if (single_box->currentIndex() == 0)
            {
                _init_data.params.fullscreen_screens = 0;
            }
            else
            {
                _init_data.params.fullscreen_screens = (1 << (single_box->currentIndex() - 1));
            }
        }
        else if (dual_btn->isChecked())
        {
            _init_data.params.fullscreen_screens = (1 << dual_box0->currentIndex());
            _init_data.params.fullscreen_screens |= (1 << dual_box1->currentIndex());
        }
        else
        {
            _init_data.params.fullscreen_screens = 0;
            QStringList screens = multi_edt->text().split(',', QString::SkipEmptyParts);
            for (int i = 0; i < screens.size(); i++)
            {
                int s = str::to<int>(screens[i].toAscii().data());
                if (s >= 1 && s <= 16)
                {
                    _init_data.params.fullscreen_screens |= (1 << (s - 1));
                }
            }
        }
        send_cmd(command::set_fullscreen_screens, _init_data.params.fullscreen_screens);
        _init_data.params.fullscreen_flip_left = flip_left_box->isChecked() ? 1 : 0;
        send_cmd(command::set_fullscreen_flip_left, _init_data.params.fullscreen_flip_left);
        _init_data.params.fullscreen_flop_left = flop_left_box->isChecked() ? 1 : 0;
        send_cmd(command::set_fullscreen_flop_left, _init_data.params.fullscreen_flop_left);
        _init_data.params.fullscreen_flip_right = flip_right_box->isChecked() ? 1 : 0;
        send_cmd(command::set_fullscreen_flip_right, _init_data.params.fullscreen_flip_right);
        _init_data.params.fullscreen_flop_right = flop_right_box->isChecked() ? 1 : 0;
        send_cmd(command::set_fullscreen_flop_right, _init_data.params.fullscreen_flop_right);
    }
}

void main_window::help_manual()
{
    QUrl manual_url;
#if defined(Q_OS_WIN)
    manual_url = QUrl::fromLocalFile(QCoreApplication::applicationDirPath() + "/../doc/bino.html");
#elif defined(Q_OS_MAC)
    manual_url = QUrl::fromLocalFile(QCoreApplication::applicationDirPath() + "/../Resources/Bino Help/bino.html");
#else
    manual_url = QUrl::fromLocalFile(DOCDIR "/bino.html");
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
    QMessageBox::information(this, _("Keyboard Shortcuts"),
            /* TRANSLATORS: This is a HTML table, shown under Help->Keyboard Shortcuts.
               Please make sure that your translation formats properly. */
            _("<p>Keyboard control:<br>"
                "<table>"
                "<tr><td>q or ESC</td><td>Stop</td></tr>"
                "<tr><td>p or SPACE</td><td>Pause / unpause</td></tr>"
                "<tr><td>f</td><td>Toggle fullscreen</td></tr>"
                "<tr><td>c</td><td>Center window</td></tr>"
                "<tr><td>e</td><td>Swap left/right eye</td></tr>"
                "<tr><td>v</td><td>Cycle through available video streams</td></tr>"
                "<tr><td>a</td><td>Cycle through available audio streams</td></tr>"
                "<tr><td>s</td><td>Cycle through available subtitle streams</td></tr>"
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
    QDialog *dialog = new QDialog(this);
    dialog->setModal(true);
    dialog->setWindowTitle(str::asprintf(_("About %s"), PACKAGE_NAME).c_str());

    QLabel *logo = new QLabel();
    logo->setPixmap(QPixmap(":logo/bino_logo.png").scaledToHeight(128, Qt::SmoothTransformation));
    QLabel *spacer = new QLabel();
    spacer->setMinimumWidth(16);
    QLabel *title = new QLabel(str::asprintf(_("The %s 3D video player"), PACKAGE_NAME).c_str());

    QTextBrowser *about_text = new QTextBrowser(this);
    about_text->setOpenExternalLinks(true);
    about_text->setText(str::asprintf(_(
                    "<p>%s version %s.</p>"
                    "<p>Copyright (C) 2011 the Bino developers.</p>"
                    "<p>This is free software. You may redistribute copies of it "
                    "under the terms of the <a href=\"http://www.gnu.org/licenses/gpl.html\">"
                    "GNU General Public License</a>. "
                    "There is NO WARRANTY, to the extent permitted by law.</p>"
                    "<p>See <a href=\"%s\">%s</a> for more information on this software.</p>"),
                PACKAGE_NAME, VERSION, PACKAGE_URL, PACKAGE_URL).c_str());

    QTextBrowser *libs_text = new QTextBrowser(this);
    libs_text->setOpenExternalLinks(true);
    libs_text->setWordWrapMode(QTextOption::NoWrap);
    QString libs_blurb = QString(str::asprintf("<p>%s<ul><li>%s</li></ul></p>", _("Platform:"), PLATFORM).c_str());
    libs_blurb += QString("<p>") + QString(_("Libraries used:"));
    std::vector<std::string> libs = lib_versions(true);
    for (size_t i = 0; i < libs.size(); i++)
    {
        libs_blurb += libs[i].c_str();
    }
    libs_blurb += QString("</p>");
    libs_text->setText(libs_blurb);

    QTextBrowser *team_text = new QTextBrowser(this);
    team_text->setOpenExternalLinks(false);
    team_text->setWordWrapMode(QTextOption::NoWrap);
    QFile team_file(":AUTHORS");
    team_file.open(QIODevice::ReadOnly | QIODevice::Text);
    team_text->setText(QTextCodec::codecForName("UTF-8")->toUnicode(team_file.readAll()));

    QTextBrowser *license_text = new QTextBrowser(this);
    license_text->setOpenExternalLinks(false);
    license_text->setWordWrapMode(QTextOption::NoWrap);
    QFile license_file(":LICENSE");
    license_file.open(QIODevice::ReadOnly | QIODevice::Text);
    license_text->setText(license_file.readAll());

    QTabWidget *tab_widget = new QTabWidget();
    tab_widget->addTab(about_text, _("About"));
    tab_widget->addTab(libs_text, _("Libraries"));
    tab_widget->addTab(team_text, _("Team"));
    tab_widget->addTab(license_text, _("License"));

    QPushButton *ok_btn = new QPushButton(_("OK"));
    connect(ok_btn, SIGNAL(pressed()), dialog, SLOT(accept()));

    QGridLayout *layout = new QGridLayout();
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

    dialog->exec();
}

player_qt::player_qt() : player(player::slave)
{
    QCoreApplication::setOrganizationName(PACKAGE_NAME);
    QCoreApplication::setApplicationName(PACKAGE_NAME);
    _settings = new QSettings;
}

player_qt::~player_qt()
{
    delete _settings;
}

void player_qt::open(const player_init_data &init_data)
{
    msg::set_level(init_data.log_level);
    _main_window = new main_window(_settings, init_data);
}

void player_qt::run()
{
    QApplication::exec();
    delete _main_window;
    _main_window = NULL;
}

void player_qt::close()
{
}
