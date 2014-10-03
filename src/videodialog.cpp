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

#include <complex>

#include <QLabel>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGridLayout>

#include "videodialog.h"

#include "gui_common.h"

video_dialog::video_dialog(QWidget *parent) : QWidget(parent), _lock(false)
{
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
