/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010, 2011, 2012
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

#ifndef VIDEO_OUTPUT_QT_H
#define VIDEO_OUTPUT_QT_H

#include <GL/glew.h>

#include <QWidget>
#include <QGLWidget>
#include <QGLFormat>
#include <QTimer>

#include "video_output.h"


/* Internal interface */

class video_output_qt;

class video_output_qt_widget : public QGLWidget
{
    Q_OBJECT

private:
    video_output_qt *_vo;

public:
    video_output_qt_widget(video_output_qt *vo, const QGLFormat &format, QWidget *parent = NULL);
    ~video_output_qt_widget();

protected:
    virtual void paintGL();
    virtual void resizeGL(int width, int height);
    virtual void keyPressEvent(QKeyEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent *event);
    virtual void mouseDoubleClickEvent(QMouseEvent *event);
    virtual void focusOutEvent(QFocusEvent *event);
};

/* Public interface */

class video_container_widget : public QWidget, public controller
{
    Q_OBJECT
    int _w, _h;
    QTimer *_timer;
private slots:
    void playloop_step();
public:
    video_container_widget(QWidget *parent = NULL);
    void start_timer();
    void set_recommended_size(int w, int h);
signals:
protected:
    virtual QSize sizeHint() const;
    virtual void closeEvent(QCloseEvent *event);
    virtual void receive_notification(const notification& note);
};

/* Public interface. See the video_output documentation. */

class video_output_qt : public video_output
{
private:
    video_container_widget *_container_widget;
    bool _container_is_external;
    video_output_qt_widget *_widget;
    QGLFormat _format;

    void create_widget();
    void mouse_set_pos(float dest);
    void mouse_toggle_fullscreen();
    void suspend_screensaver();
    void resume_screensaver();

protected:
    virtual void make_context_current();
    virtual bool context_is_stereo();
    virtual void recreate_context(bool stereo);
    virtual void trigger_update();
    virtual void trigger_resize(int w, int h);

public:
    /* Constructor, Destructor */
    /* If a container widget is given, then it is assumed that this widget is
     * part of another widget (e.g. a main window). If no container widget is
     * given, we will use our own, and it will be a top-level window. */
    video_output_qt(video_container_widget *container_widget = NULL);
    virtual ~video_output_qt();

    /* Grab the keyboard focus for the video widget, to enable keyboard shortcuts */
    void grab_focus();

    virtual void init();
    virtual int64_t wait_for_subtitle_renderer();
    virtual void deinit();

    virtual bool supports_stereo() const;
    virtual int screen_width();
    virtual int screen_height();
    virtual float screen_pixel_aspect_ratio();
    virtual int width();
    virtual int height();
    virtual int pos_x();
    virtual int pos_y();

    virtual void center();
    virtual void enter_fullscreen();
    virtual void exit_fullscreen();

    virtual void process_events();

    friend class video_output_qt_widget;
};

#endif
