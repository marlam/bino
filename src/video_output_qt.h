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

#ifndef VIDEO_OUTPUT_QT_H
#define VIDEO_OUTPUT_QT_H

#include <GL/glew.h>

#include <QWidget>
#include <QGLWidget>
#include <QGLFormat>
#include <QThread>

#include "exc.h"

#include "video_output.h"


/* Internal interface. The display function is called from a separate thread.
 * This avoids the buffer swap operation blocking the main thread in case
 * sync-to-vblank is active. */

class video_output_qt;
class video_output_qt_widget;

class display_thread : public QThread
{
    Q_OBJECT

private:
    video_output_qt_widget *_vw;        // The video output widget. It owns the GL context.
    video_output_qt *_vo;               // The video output. We need its display function.
    bool _enabled;                      // Whether this display thread is enabled
    exc _e;                             // An exception in case displaying failed.

public:
    display_thread(video_output_qt_widget *vw, video_output_qt *vo);
    void enable();                      // Enable displaying via this thread.
    void run();                         // The worker function.
    const exc &get_exc() const          // Get the exception.
    {
        return _e;
    }
};

class video_output_qt_widget : public QGLWidget
{
    Q_OBJECT

private:
    display_thread _display_thread;
    video_output_qt *_vo;

public:
    video_output_qt_widget(video_output_qt *vo, const QGLFormat &format, QWidget *parent = NULL);
    ~video_output_qt_widget();

    void wait_for_display_thread();
    void enable_display_thread();

public slots:
    void move_event();

protected:
    virtual void paintGL();
    virtual void resizeGL(int width, int height);
    virtual void resizeEvent(QResizeEvent *event);
    virtual void paintEvent(QPaintEvent *event);
    virtual void moveEvent(QMoveEvent *event);
    virtual void keyPressEvent(QKeyEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent *event);
    virtual void closeEvent(QCloseEvent *event);
};

/* Public interface. You can use this as a video container widget, to
 * conveniently catch move events and pass them to the video output, as
 * described below (but note that you still must catch move events for
 * parent widgets yourself). */

class video_container_widget : public QWidget, public controller
{
    Q_OBJECT
    int _w, _h;
public:
    video_container_widget(QWidget *parent = NULL);
    void set_recommended_size(int w, int h);
signals:
    void move_event();
protected:
    virtual QSize sizeHint() const;
    virtual void moveEvent(QMoveEvent *event);
    virtual void closeEvent(QCloseEvent *event);
};

/* Public interface. See the video_output documentation. */

class video_output_qt : public video_output
{
private:
    bool _qt_app_owner;
    video_container_widget *_container_widget;
    bool _container_is_external;
    video_output_qt_widget *_widget;
    QGLFormat _format;
    bool _fullscreen;
    bool _playing;

    void create_widget();
    void mouse_set_pos(float dest);

protected:
    virtual bool context_is_stereo();
    virtual void recreate_context(bool stereo);
    virtual void trigger_update();
    virtual void trigger_resize(int w, int h);

public:
    /* Constructor, Destructor */
    /* If a container widget is given, then it is assumed that this widget is
     * part of another widget (e.g. a main window). In this case, you also need
     * to use the move_event() function; see below. If no container widget is
     * given, we will use our own, and it will be a top-level window. */
    video_output_qt(bool benchmark, video_container_widget *container_widget = NULL);
    virtual ~video_output_qt();

    /* If you give a container element to the constructor, you have to call
     * this function for every move events that the container widget or its
     * parent widgets receive. This is required for the masking output modes
     * (even-odd-*, checkerboard). */
    void move_event();

    virtual void init();
    virtual void deinit();

    virtual bool supports_stereo() const;
    virtual int screen_width();
    virtual int screen_height();
    virtual float screen_aspect_ratio();
    virtual int width();
    virtual int height();
    virtual float aspect_ratio();
    virtual int pos_x();
    virtual int pos_y();

    virtual void center();
    virtual void enter_fullscreen();
    virtual void exit_fullscreen();
    virtual bool toggle_fullscreen();

    virtual void process_events();

    virtual void prepare_next_frame(const video_frame &frame);

    virtual void receive_notification(const notification &note);

    friend class display_thread;
    friend class video_output_qt_widget;
};

#endif
