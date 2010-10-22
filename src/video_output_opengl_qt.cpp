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

#include <GL/glew.h>

#include <cmath>

#include <QApplication>
#include <QDesktopWidget>
#include <QKeyEvent>

#include "exc.h"
#include "msg.h"
#include "str.h"

#include "video_output_opengl_qt.h"


video_output_opengl_qt_widget::video_output_opengl_qt_widget(
        video_output_opengl_qt *vo, const QGLFormat &format, QWidget *parent)
    : QGLWidget(format, parent), _vo(vo), _have_valid_data(false)
{
    setFocusPolicy(Qt::StrongFocus);
}

video_output_opengl_qt_widget::~video_output_opengl_qt_widget()
{
}

void video_output_opengl_qt_widget::activate()
{
    _have_valid_data = true;
}

void video_output_opengl_qt_widget::initializeGL()
{
    GLenum err = glewInit();
    if (err != GLEW_OK)
    {
        throw exc(std::string("cannot initialize GLEW: ")
                + reinterpret_cast<const char *>(glewGetErrorString(err)));
    }
    _vo->initialize(
            glewIsSupported("GL_ARB_texture_non_power_of_two"),
            glewIsSupported("GL_ARB_fragment_shader"));
}

void video_output_opengl_qt_widget::paintGL()
{
    if (_have_valid_data)
    {
        _vo->display();
    }
    else
    {
        glClear(GL_COLOR_BUFFER_BIT);
    }
}

void video_output_opengl_qt_widget::resizeGL(int w, int h)
{
    _vo->reshape(w, h);
}

void video_output_opengl_qt_widget::closeEvent(QCloseEvent *)
{
    _vo->send_cmd(command::quit);
}

void video_output_opengl_qt_widget::keyPressEvent(QKeyEvent *event)
{
    switch (event->key())
    {
    case Qt::Key_Escape:
    case Qt::Key_Q:
        _vo->send_cmd(command::quit);
        break;
    case Qt::Key_S:
        _vo->send_cmd(command::toggle_swap_eyes);
        break;
    case Qt::Key_F:
        _vo->send_cmd(command::toggle_fullscreen);
        break;
    case Qt::Key_Space:
    case Qt::Key_P:
        _vo->send_cmd(command::toggle_pause);
        break;
    case Qt::Key_1:
        _vo->send_cmd(command::adjust_contrast, -0.05f);
        break;
    case Qt::Key_2:
        _vo->send_cmd(command::adjust_contrast, +0.05f);
        break;
    case Qt::Key_3:
        _vo->send_cmd(command::adjust_brightness, -0.05f);
        break;
    case Qt::Key_4:
        _vo->send_cmd(command::adjust_brightness, +0.05f);
        break;
    case Qt::Key_5:
        _vo->send_cmd(command::adjust_hue, -0.05f);
        break;
    case Qt::Key_6:
        _vo->send_cmd(command::adjust_hue, +0.05f);
        break;
    case Qt::Key_7:
        _vo->send_cmd(command::adjust_saturation, -0.05f);
        break;
    case Qt::Key_8:
        _vo->send_cmd(command::adjust_saturation, +0.05f);
        break;
    case Qt::Key_Left:
        _vo->send_cmd(command::seek, -10.0f);
        break;
    case Qt::Key_Right:
        _vo->send_cmd(command::seek, +10.0f);
        break;
    case Qt::Key_Up:
        _vo->send_cmd(command::seek, -60.0f);
        break;
    case Qt::Key_Down:
        _vo->send_cmd(command::seek, +60.0f);
        break;
    case Qt::Key_PageUp:
        _vo->send_cmd(command::seek, -600.0f);
        break;
    case Qt::Key_PageDown:
        _vo->send_cmd(command::seek, +600.0f);
        break;
    default:
        QGLWidget::keyPressEvent(event);
        break;
    }
}


video_output_opengl_qt::video_output_opengl_qt(QWidget *parent) throw ()
    : video_output_opengl(), _parent(parent), _app(NULL), _widget(NULL)
{
    _argc = 1;
    _argv[0] = PACKAGE_NAME;
    _argv[1] = NULL;

    // Initialize Qt if necessary, i.e. if there is no parent widget.
    // This cannot wait until open() because we need it for supports_stereo().
    if (!_parent)
    {
        _app = new QApplication(_argc, const_cast<char **>(_argv));
    }
}

video_output_opengl_qt::~video_output_opengl_qt()
{
    delete _widget;
    delete _app;
}

bool video_output_opengl_qt::supports_stereo()
{
    QGLFormat fmt;
    fmt.setDoubleBuffer(true);
    fmt.setAlpha(true);
    fmt.setStereo(true);
    QGLWidget *tmpwidget = new QGLWidget(fmt);
    bool ret = tmpwidget->format().stereo();
    delete tmpwidget;
    return ret;
}

void video_output_opengl_qt::open(
        video_frame_format preferred_frame_format,
        int src_width, int src_height, float src_aspect_ratio,
        int mode, const video_output_state &state, unsigned int flags,
        int win_width, int win_height)
{
    set_mode(static_cast<video_output::mode>(mode));
    set_source_info(src_width, src_height, src_aspect_ratio, preferred_frame_format);

    // Initialize widget
    QGLFormat fmt;
    fmt.setAlpha(true);
    fmt.setDoubleBuffer(true);
    if (mode == stereo)
    {
        fmt.setStereo(true);
    }
    else if (mode == even_odd_rows || mode == even_odd_columns || mode == checkerboard)
    {
        fmt.setStencil(true);
    }
    _widget = new video_output_opengl_qt_widget(this, fmt);
    if (!_widget->format().alpha() || !_widget->format().doubleBuffer()
            || (mode == stereo && !_widget->format().stereo())
            || ((mode == even_odd_rows || mode == even_odd_columns || mode == checkerboard) &&
                !_widget->format().stencil()))
    {
        throw exc("cannot set GL context format");
    }

    int screen_width = QApplication::desktop()->screenGeometry().width();
    int screen_height = QApplication::desktop()->screenGeometry().height();
    float screen_pixel_aspect_ratio =
        static_cast<float>(QApplication::desktop()->logicalDpiY())
        / static_cast<float>(QApplication::desktop()->logicalDpiX());
    if (std::fabs(screen_pixel_aspect_ratio - 1.0f) < 0.03f)
    {
        // This screen most probably has square pixels, and the difference to 1.0
        // is only due to slightly inaccurate measurements and rounding. Force
        // 1.0 so that the user gets expected results.
        screen_pixel_aspect_ratio = 1.0f;
    }
    msg::inf("display:");
    msg::inf("    resolution %dx%d, pixel aspect ratio %g:1",
            screen_width, screen_height, screen_pixel_aspect_ratio);
    set_screen_info(screen_width, screen_height, screen_pixel_aspect_ratio);
    compute_win_size(win_width, win_height);

    if (state.fullscreen)
    {
        _widget->setWindowState(_widget->windowState() ^ Qt::WindowFullScreen);
        _widget->setCursor(Qt::BlankCursor);
    }
    else
    {
        _widget->resize(video_output_opengl::win_width(), video_output_opengl::win_height());
    }
    if ((flags & center) && !state.fullscreen)
    {
        _widget->move((screen_width - video_output_opengl::win_width()) / 2,
                (screen_height - video_output_opengl::win_height()) / 2);
    }
    set_state(state);

    // Initialize OpenGL by forcing a call to initializeGL()
    _widget->update();
    _widget->show();
}

void video_output_opengl_qt::activate()
{
    swap_tex_set();
    _widget->activate();
    _widget->update();
}

void video_output_opengl_qt::process_events()
{
    QApplication::processEvents();
    QCoreApplication::sendPostedEvents();
}

void video_output_opengl_qt::close()
{
    delete _widget;
    _widget = NULL;
}

void video_output_opengl_qt::receive_notification(const notification &note)
{
    switch (note.type)
    {
    case notification::swap_eyes:
        state().swap_eyes = note.current.flag;
        _widget->update();
        break;
    case notification::fullscreen:
        if (note.previous.flag != note.current.flag)
        {
            if (note.previous.flag)
            {
                state().fullscreen = false;
                _widget->setWindowState(_widget->windowState() ^ Qt::WindowFullScreen);
                _widget->setCursor(Qt::ArrowCursor);
            }
            else
            {
                state().fullscreen = true;
                _widget->setWindowState(_widget->windowState() ^ Qt::WindowFullScreen);
                _widget->setCursor(Qt::BlankCursor);
            }
        }
        break;
    case notification::pause:
        break;
    case notification::contrast:
        state().contrast = note.current.value;
        _widget->update();
        break;
    case notification::brightness:
        state().brightness = note.current.value;
        _widget->update();
        break;
    case notification::hue:
        state().hue = note.current.value;
        _widget->update();
        break;
    case notification::saturation:
        state().saturation = note.current.value;
        _widget->update();
        break;
    case notification::pos:
        break;
    }
}
