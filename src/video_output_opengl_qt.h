/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010-2011
 * Martin Lambers <marlam@marlam.de>
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

#ifndef VIDEO_OUTPUT_OPENGL_QT_H
#define VIDEO_OUTPUT_OPENGL_QT_H

#include <vector>
#include <string>

#include <GL/glew.h>

#include <QWidget>
#include <QGLWidget>

#include "video_output_opengl.h"


/* Internal interface */

class video_output_opengl_qt;

class video_output_opengl_qt_widget : public QGLWidget
{
    Q_OBJECT

private:
    video_output_opengl_qt *_vo;

public:
    video_output_opengl_qt_widget(video_output_opengl_qt *vo, const QGLFormat &format, QWidget *parent = NULL);
    ~video_output_opengl_qt_widget();

    virtual QSize sizeHint() const;

public slots:
    void move_event();

protected:
    virtual void initializeGL();
    virtual void paintGL();
    virtual void resizeGL(int width, int height);
    virtual void moveEvent(QMoveEvent *event);
    virtual void keyPressEvent(QKeyEvent *event);
};

/* Public interface. You can use this as a video container widget, to
 * conveniently catch move events and pass them to the video output, as
 * described below (but note that you still must catch move events for
 * parent widgets yourself). */

class video_container_widget : public QWidget, public controller
{
    Q_OBJECT
public:
    video_container_widget(QWidget *parent = NULL);
signals:
    void move_event();
protected:
    virtual void moveEvent(QMoveEvent *event);
    virtual void closeEvent(QCloseEvent *event);
};

/* Public interface. This subclasses video_output_opengl; see its documentation. */

class video_output_opengl_qt : public video_output_opengl
{
private:
    bool _qt_app_owner;
    video_container_widget *_container_widget;
    bool _container_is_external;
    video_output_opengl_qt_widget *_widget;

    void center();
    void enter_fullscreen();
    void exit_fullscreen();

protected:
    virtual int screen_pos_x();
    virtual int screen_pos_y();

public:
    /* If a container widget is given, then it is assumed that this widget is
     * part of another widget (e.g. a main window). In this case, you also need
     * to use the move_event() function; see below. If no container widget is
     * given, we will use our own, and it will be a top-level window. */
    video_output_opengl_qt(video_container_widget *container_widget = NULL) throw ();
    virtual ~video_output_opengl_qt();

    /* If you give a container element to the constructor, you have to call
     * this function for every move events that the container widget or its
     * parent widgets receive. This is required for the masking output modes
     * (even-odd-*, checkerboard). */
    void move_event();

    virtual bool supports_stereo();

    virtual void open(
            int src_format, bool src_mono,
            int src_width, int src_height, float src_aspect_ratio,
            int mode, const video_output_state &state, unsigned int flags,
            int win_width, int win_height);

    virtual void activate();
    virtual void process_events();

    virtual void close();

    virtual void receive_notification(const notification &note);

friend class video_output_opengl_qt_widget;
};

std::vector<std::string> opengl_versions();

#endif
