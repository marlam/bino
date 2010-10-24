/*
 * This file is part of bino, a program to play stereoscopic videos.
 *
 * Copyright (C) 2010  Martin Lambers <marlam@marlam.de>
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

#include <QMainWindow>
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

#include "gui.h"
#include "qt_app.h"
#include "video_output_opengl_qt.h"


video_widget::video_widget(QWidget *parent) : QWidget(parent)
{
}

video_widget::~video_widget()
{
}


in_out_widget::in_out_widget(QWidget *parent) : QWidget(parent)
{
}

in_out_widget::~in_out_widget()
{
}


controls_widget::controls_widget(QWidget *parent) : QWidget(parent)
{
}

controls_widget::~controls_widget()
{
}


main_window::main_window()
    : _last_open_dir(QDir::current())
{
    // Application properties
    setWindowTitle(PACKAGE_NAME);
    setWindowIcon(QIcon(":appicon.png"));

    // Central widget
    QWidget *central_widget = new QWidget(this);
    QGridLayout *layout = new QGridLayout();
    video_widget *video_w = new video_widget(central_widget);
    layout->addWidget(video_w, 0, 0);
    in_out_widget *in_out_w = new in_out_widget(central_widget);
    layout->addWidget(in_out_w, 1, 0);
    controls_widget *controls_w = new controls_widget(central_widget);
    layout->addWidget(controls_w, 2, 0);
    layout->setRowStretch(0, 1);
    layout->setColumnStretch(0, 1);
    central_widget->setLayout(layout);
    setCentralWidget(central_widget);

    // Menus
    QMenu *file_menu = menuBar()->addMenu(tr("&File"));
    QAction *file_open_act = new QAction(tr("&Open..."), this);
    file_open_act->setShortcut(QKeySequence::Open);
    connect(file_open_act, SIGNAL(triggered()), this, SLOT(file_open()));
    file_menu->addAction(file_open_act);
    QAction *file_open_url_act = new QAction(tr("Open &URL..."), this);
    connect(file_open_url_act, SIGNAL(triggered()), this, SLOT(file_open_url()));
    file_menu->addAction(file_open_url_act);
    file_menu->addSeparator();
    QAction *file_quit_act = new QAction(tr("&Quit..."), this);
    file_quit_act->setShortcut(QKeySequence::Quit);
    connect(file_quit_act, SIGNAL(triggered()), this, SLOT(close()));
    file_menu->addAction(file_quit_act);
    QMenu *help_menu = menuBar()->addMenu(tr("&Help"));
    QAction *help_keyboard_act = new QAction(tr("&Keyboard Shortcuts"), this);
    help_keyboard_act->setShortcut(QKeySequence::HelpContents);
    connect(help_keyboard_act, SIGNAL(triggered()), this, SLOT(help_keyboard()));
    help_menu->addAction(help_keyboard_act);
    QAction *help_about_act = new QAction(tr("&About"), this);
    connect(help_about_act, SIGNAL(triggered()), this, SLOT(help_about()));
    help_menu->addAction(help_about_act);

    show();
}

main_window::~main_window()
{
}

void main_window::closeEvent(QCloseEvent *event)
{
    event->accept();
}

void main_window::file_open()
{
    QFileDialog *file_dialog = new QFileDialog(this);
    file_dialog->setDirectory(_last_open_dir);
    file_dialog->setWindowTitle("Open up to three files");
    file_dialog->setAcceptMode(QFileDialog::AcceptOpen);
    file_dialog->setFileMode(QFileDialog::ExistingFiles);
    if (!file_dialog->exec())
    {
        return;
    }
    QStringList dir_names = file_dialog->selectedFiles();
    if (dir_names.empty())
    {
        return;
    }
    _last_open_dir = file_dialog->directory().path();
    if (dir_names.size() > 3)
    {
        QMessageBox::critical(this, "Error", "Cannot open more than 3 files");
        return;
    }
    // TODO: open the files
}

void main_window::file_open_url()
{
    QDialog *url_dialog = new QDialog(this);
    url_dialog->setWindowTitle("Open URL");
    QLabel *url_label = new QLabel("URL:");
    QLineEdit *url_edit = new QLineEdit("");
    url_edit->setMinimumWidth(256);
    QPushButton *ok_btn = new QPushButton("OK");
    QPushButton *cancel_btn = new QPushButton("Cancel");
    connect(ok_btn, SIGNAL(pressed()), url_dialog, SLOT(accept()));
    connect(cancel_btn, SIGNAL(pressed()), url_dialog, SLOT(reject()));
    QGridLayout *layout = new QGridLayout();
    layout->addWidget(url_label, 0, 0);
    layout->addWidget(url_edit, 0, 1, 1, 3);
    layout->addWidget(ok_btn, 2, 2);
    layout->addWidget(cancel_btn, 2, 3);
    layout->setColumnStretch(1, 1);
    url_dialog->setLayout(layout);
    url_dialog->exec();
    if (url_dialog->result() == QDialog::Accepted
            && !url_edit->text().isEmpty())
    {
        QString url = url_edit->text();
        // TODO: open url
    }
}

void main_window::help_keyboard()
{
}

void main_window::help_about()
{
    QMessageBox::about(this, tr("About " PACKAGE_NAME), tr(
                "<p>%1 version %2</p>"
                "<p>Copyright (C) 2010 Martin Lambers and others.<br>"
                "This is free software. You may redistribute copies of it under the terms of "
                "the <a href=\"http://www.gnu.org/licenses/gpl.html\">"
                "GNU General Public License</a>.<br>"
                "There is NO WARRANTY, to the extent permitted by law.</p>"
                "See <a href=\"%3\">%3</a> for more information on this software.</p>")
            .arg(PACKAGE_NAME).arg(VERSION).arg(PACKAGE_URL));
}

gui::gui()
{
    _qt_app_owner = init_qt();
}

gui::~gui()
{
    if (_qt_app_owner)
    {
        exit_qt();
    }
}

int gui::run()
{
    main_window *w = new main_window();
    bool r = exec_qt();
    delete w;
    return r;
}
