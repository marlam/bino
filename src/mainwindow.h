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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "config.h"

#include <QMainWindow>
#include <QWidget>
#include <QDialog>

#include "dispatch.h"

class QLabel;
class QComboBox;
class QPushButton;
class QDoubleSpinBox;
class QSpinBox;
class QCheckBox;
class QFontComboBox;
class QSettings;
class QGroupBox;
class QSlider;
class QRadioButton;
class QLineEdit;
class QStackedWidget;
class QTimer;
class QIcon;

class video_container_widget;
class in_out_widget;
class controls_widget;
class preferences_dialog;

class main_window : public QMainWindow, public controller
{
    Q_OBJECT

private:
    QSettings *_settings;
    video_container_widget *_video_container_widget;
    in_out_widget *_in_out_widget;
    controls_widget *_controls_widget;
    preferences_dialog * _preferences_dialog;
    QTimer *_timer;
    std::vector<std::string> _now_playing;

    int _max_recent_files;
    QList<QAction *> _recent_file_actions;
    QAction *_recent_files_separator;
    QAction *_clear_recent_separator;
    QAction *_clear_recent_files_act;

    void update_recent_file_actions();
    QString stripped_name(const QStringList & filenames);
    QIcon get_icon(const QString & name);

private slots:
    void playloop_step();
    void file_open();
    void file_open_urls();
    void file_open_device();
    void preferences();
    void help_manual();
    void help_website();
    void help_keyboard();
    void help_about();

    void open_recent_file();
    void clear_recent_files();

protected:
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);
    void moveEvent(QMoveEvent* event);
    void closeEvent(QCloseEvent *event);
    bool eventFilter(QObject *obj, QEvent *event);

public:
    main_window(QSettings *settings);
    virtual ~main_window();

    void open(QStringList urls, const device_request &dev_request = device_request(),
            const parameters& initial_params = parameters());
    video_container_widget* container_widget()
    {
        return _video_container_widget;
    }

    virtual void receive_notification(const notification &note);
};

#endif
