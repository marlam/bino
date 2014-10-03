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

#ifndef SUBTITLEDIALOG_H
#define SUBTITLEDIALOG_H

#include "config.h"

#include <QWidget>
#include <QList>
#include "dispatch.h"

class QCheckBox;
class QComboBox;
class QFontComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QTextCodec;
class QPushButton;

class subtitle_dialog: public QWidget, public controller
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
    subtitle_dialog(QWidget *parent = 0);

    virtual void receive_notification(const notification &note);
};

#endif
