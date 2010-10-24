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

#ifndef GUI_H
#define GUI_H

#include <QMainWindow>
#include <QWidget>
#include <QDir>


class video_widget : public QWidget
{
    Q_OBJECT

public:
    video_widget(QWidget *parent);
    ~video_widget();
};

class in_out_widget : public QWidget
{
    Q_OBJECT

public:
    in_out_widget(QWidget *parent);
    ~in_out_widget();
};

class controls_widget : public QWidget
{
    Q_OBJECT

public:
    controls_widget(QWidget *parent);
    ~controls_widget();
};

class main_window : public QMainWindow
{
    Q_OBJECT

private:
    QDir _last_open_dir;

private slots:
    void file_open();
    void file_open_url();
    void help_keyboard();
    void help_about();

protected:
    void closeEvent(QCloseEvent *event);	

public:
    main_window();
    ~main_window();
};

class gui
{
private:
    bool _qt_app_owner;

public:
    gui();
    ~gui();

    int run();
};

#endif
