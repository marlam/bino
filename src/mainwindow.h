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
class fullscreen_dialog;
class color_dialog;
class crosstalk_dialog;
class quality_dialog;
class zoom_dialog;
class audio_dialog;
class subtitle_dialog;
class video_dialog;
#if HAVE_LIBXNVCTRL
class sdi_output_dialog;
#endif

class main_window : public QMainWindow, public controller
{
    Q_OBJECT

private:
    QSettings *_settings;
    video_container_widget *_video_container_widget;
    in_out_widget *_in_out_widget;
    controls_widget *_controls_widget;
    fullscreen_dialog *_fullscreen_dialog;
    color_dialog *_color_dialog;
    crosstalk_dialog *_crosstalk_dialog;
    quality_dialog *_quality_dialog;
    zoom_dialog *_zoom_dialog;
    audio_dialog *_audio_dialog;
    subtitle_dialog *_subtitle_dialog;
    video_dialog *_video_dialog;
#if HAVE_LIBXNVCTRL
    sdi_output_dialog *_sdi_output_dialog;
#endif // HAVE_LIBXNVCTRL
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
    void preferences_fullscreen();
    void preferences_colors();
    void preferences_crosstalk();
    void preferences_quality();
    void preferences_zoom();
    void preferences_audio();
    void preferences_subtitle();
    void preferences_video();
    void preferences_sdi_output();
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
