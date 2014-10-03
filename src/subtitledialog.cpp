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

#include <QLabel>
#include <QComboBox>
#include <QFontComboBox>
#include <QCheckBox>
#include <QTextCodec>
#include <QSpinBox>
#include <QPushButton>
#include <QGridLayout>
#include <QColorDialog>

#include "subtitledialog.h"

#include "gui_common.h"

subtitle_dialog::subtitle_dialog(QWidget *parent) : QWidget(parent), _lock(false)
{
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
