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

#ifndef VIDEO_OUTPUT_OPENGL_QT_H
#define VIDEO_OUTPUT_OPENGL_QT_H

#include <QGLWidget>

#include "video_output_opengl.h"


class video_output_opengl_qt;

class video_output_opengl_qt_widget : public QGLWidget
{
    Q_OBJECT

private:
    video_output_opengl_qt *_vo;
    bool _have_valid_data;
    bool _initialized;
    int _last_resize_width, _last_resize_height;

public:
    video_output_opengl_qt_widget(video_output_opengl_qt *vo, const QGLFormat &format, QWidget *parent = NULL);
    ~video_output_opengl_qt_widget();

    void initialize();
    void deinitialize();
    void activate();
    void deactivate();
    virtual QSize sizeHint() const;

protected:
    virtual void initializeGL();
    virtual void paintGL();
    virtual void resizeGL(int width, int height);
    virtual void moveEvent(QMoveEvent *event);
    virtual void closeEvent(QCloseEvent *event);
    virtual void keyPressEvent(QKeyEvent *event);
};

class video_output_opengl_qt : public video_output_opengl
{
private:
    bool _qt_app_owner;
    QWidget *_parent;
    video_output_opengl_qt_widget *_widget;

    void enter_fullscreen();
    void exit_fullscreen();

protected:
    virtual int window_pos_x();
    virtual int window_pos_y();

public:
    video_output_opengl_qt(QWidget *parent = NULL) throw ();
    ~video_output_opengl_qt();

    virtual bool supports_stereo();

    virtual void open(
            video_frame_format preferred_format,
            int src_width, int src_height, float src_aspect_ratio,
            int mode, const video_output_state &state, unsigned int flags,
            int win_width, int win_height);

    virtual void activate();
    virtual void process_events();

    virtual void close();

    virtual void receive_notification(const notification &note);

    video_output_opengl_qt_widget *widget()
    {
        return _widget;
    }

friend class video_output_opengl_qt_widget;
};

#endif
