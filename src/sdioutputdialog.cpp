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

#include <QLabel>
#include <QComboBox>
#include <QGridLayout>

#include "sdioutputdialog.h"

#include "gui_common.h"

#if HAVE_LIBXNVCTRL
#include <NVCtrl/NVCtrl.h>
#include "NvSDIutils.h"
#endif // HAVE_LIBXNVCTRL

sdi_output_dialog::sdi_output_dialog(QWidget *parent) : QWidget(parent),
    _lock(false)
{
#if HAVE_LIBXNVCTRL

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

    QGridLayout *layout = new QGridLayout;
    layout->addWidget(sdi_output_format_label, 0, 0);
    layout->addWidget(_sdi_output_format_combobox, 0, 1, 1, 1);
    layout->addWidget(sdi_output_left_stereo_mode_label, 1, 0);
    layout->addWidget(_sdi_output_left_stereo_mode_combobox, 1, 1);
    layout->addWidget(sdi_output_right_stereo_mode_label, 2, 0);
    layout->addWidget(_sdi_output_right_stereo_mode_combobox, 2, 1);
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
