/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010, 2011, 2012, 2013, 2014, 2015, 2016
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

#include "mainwindow.h"

#include "inoutwidget.h"
#include "controlswidget.h"
#include "preferences.h"
#include "opendevicedialog.h"

#include "lib_versions.h"
#include "audio_output.h"
#include "media_input.h"
#include "video_output_qt.h"

#include "base/dbg.h"
#include "base/msg.h"
#include "base/str.h"

#include "base/gettext.h"
// Qt requires strings from gettext to be in UTF-8 encoding.
#define _(string) (str::convert(gettext(string), str::localcharset(), "UTF-8").c_str())

#if HAVE_LIBXNVCTRL
#  include <NVCtrl/NVCtrl.h>
#  include "NvSDIutils.h"
#endif // HAVE_LIBXNVCTRL

// for autoconf < 2.65:
#ifndef PACKAGE_URL
#  define PACKAGE_URL "http://bino3d.org"
#endif

main_window::main_window(QSettings *settings) :
    _settings(settings),
    _preferences_dialog(NULL),
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
    QAction *preferences_act = new QAction(_("&Settings..."), this);
    connect(preferences_act, SIGNAL(triggered()), this, SLOT(preferences()));
    preferences_menu->addAction(preferences_act);
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

void main_window::preferences()
{
    if (!_preferences_dialog)
        _preferences_dialog = new preferences_dialog(this);
    _preferences_dialog->show();
    _preferences_dialog->raise();
    _preferences_dialog->activateWindow();
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
                        "<p>Copyright (C) 2016 the Bino developers.</p>"
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

QIcon main_window::get_icon(const QString& name)
{
    return QIcon::fromTheme(name, QIcon(QString(":icons/") + name));
}
