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
#include <QGridLayout>
#include <QPushButton>
#include <QStackedWidget>
#include <QListWidget>

#include "preferences.h"

#include "fullscreendialog.h"
#include "colordialog.h"
#include "crosstalkdialog.h"
#include "qualitydialog.h"
#include "zoomdialog.h"
#include "audiodialog.h"
#include "subtitledialog.h"
#include "videodialog.h"

#if HAVE_LIBXNVCTRL
#include "sdioutputdialog.h"
#endif

#include "gui_common.h"

preferences_dialog::preferences_dialog(QWidget *parent) : 
  QDialog(parent)
{
    setModal(false);
    setWindowTitle(_("Preferences"));
    
    list_widget = new QListWidget(this);
    stacked_widget = new QStackedWidget(this);
    
    add_preferences_page(new fullscreen_dialog(),_("Fullscreen"),"view-fullscreen");
    add_preferences_page(new color_dialog(),_("Display Color"),"fill-color");
    add_preferences_page(new crosstalk_dialog(),_("Display Crosstalk"),"video-display");
    add_preferences_page(new quality_dialog(),_("Rendering Quality"),"rating");
    add_preferences_page(new zoom_dialog(),_("Zoom"),"zoom-in");
    add_preferences_page(new audio_dialog(),_("Audio"),"audio-volume-high");
    add_preferences_page(new subtitle_dialog(),_("Subtitle"),"draw-text");
    add_preferences_page(new video_dialog(),_("Video"),"video-display");

#if HAVE_LIBXNVCTRL
    add_preferences_page(new sdi_output_dialog(),_("SDI Output"),"video-display");
#endif
    
    connect(list_widget,SIGNAL(currentRowChanged(int)),stacked_widget,SLOT(setCurrentIndex(int)));

    QPushButton *ok_button = new QPushButton(_("OK"));
    connect(ok_button, SIGNAL(clicked()), this, SLOT(close()));

    QGridLayout *layout = new QGridLayout;
    layout->addWidget(list_widget, 0, 0, 1, 1);
    layout->addWidget(stacked_widget, 0, 1, 1, 1);
    layout->addWidget(ok_button, 1, 0, 1, 2);
    layout->setColumnStretch(0,1);
    layout->setColumnStretch(1,3);
    
    setLayout(layout);
}

void preferences_dialog::add_preferences_page(QWidget * dialog, const QString& title, const QString& icon_name)
{
    stacked_widget->addWidget(dialog);
    QListWidgetItem *qlistwidgetitem = new QListWidgetItem(title, list_widget);
    qlistwidgetitem->setIcon(QIcon::fromTheme(icon_name, QIcon(QString(":icons/") + icon_name)));
}
