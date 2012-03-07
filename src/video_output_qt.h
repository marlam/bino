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
#include <QThread>
#include <QMutex>

#include "video_output.h"


/* Internal interface */

class video_output_qt;
class video_output_qt_widget;

class gl_thread : public QThread
{
private:
    video_output_qt* _vo_qt;
    video_output_qt_widget* _vo_qt_widget;
    bool _render;
    bool _activate_next_frame;
    bool _resize;
    int _w, _h;
    bool _prepare_next_frame;
    bool _have_prepared_frame;
    video_frame _next_frame;
    void* _next_data[2][3];
    size_t _next_line_size[2][3];
    subtitle_box _next_subtitle;
    QMutex _prepare_next_mutex;
    bool _recreate_context;
    bool _recreate_context_stereo;
    bool _failure;
    exc _e;

public:
    gl_thread(video_output_qt* vo_qt, video_output_qt_widget* vo_qt_widget);
    ~gl_thread();

    void set_render(bool r);
    void activate_next_frame();
    void resize(int w, int h);
    void prepare_next_frame(const video_frame &frame, const subtitle_box &subtitle);
    void recreate_context(bool stereo);

    void run();

    bool recreate_context()
    {
        return _recreate_context;
    }
    bool recreate_context_stereo()
    {
        return _recreate_context_stereo;
    }
    bool failure() const
    {
        return _failure;
    }
    const exc& exception() const
    {
        return _e;
    }
};

class video_output_qt_widget : public QGLWidget
{
    Q_OBJECT
private:
    video_output_qt *_vo;
    class gl_thread _gl_thread;
    QTimer _timer;

private slots:
    void check_gl_thread();

public:
    video_output_qt_widget(video_output_qt *vo, const QGLFormat &format, QWidget *parent = NULL);

    void start_rendering();
    void stop_rendering();
    class gl_thread* gl_thread()
    {
        return &_gl_thread;
    }

protected:
    virtual void paintEvent(QPaintEvent*) {};
    virtual void resizeEvent(QResizeEvent* event);
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
    void grab_focus();
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
    bool _fullscreen;
    bool _screensaver_inhibited;

    void create_widget();
    void mouse_set_pos(float dest);
    void mouse_toggle_fullscreen();
    void suspend_screensaver();
    void resume_screensaver();

protected:
#ifdef GLEW_MX
    virtual GLEWContext* glewGetContext();
#endif
    virtual void make_context_current();
    virtual bool context_is_stereo();
    virtual void recreate_context(bool stereo);
    virtual void trigger_resize(int w, int h);

public:
    /* Constructor, Destructor */
    /* If a container widget is given, then it is assumed that this widget is
     * part of another widget (e.g. a main window). If no container widget is
     * given, we will use our own, and it will be a top-level window. */
    video_output_qt(video_container_widget *container_widget = NULL);
    virtual ~video_output_qt();

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

    virtual void prepare_next_frame(const video_frame &frame, const subtitle_box &subtitle);
    virtual void activate_next_frame();

    virtual void process_events();
    virtual void receive_notification(const notification& note);

    friend class gl_thread;
    friend class video_output_qt_widget;
};

#endif
