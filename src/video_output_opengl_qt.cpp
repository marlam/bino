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
#ifdef GLEW_MX
static GLEWContext _glewContext;
static GLEWContext* glewGetContext() { return &_glewContext; }
#endif

#include <cmath>
#include <cstdlib>

#include <QApplication>
#include <QDesktopWidget>
#include <QGridLayout>
#include <QKeyEvent>
#include <QIcon>
#include <QMessageBox>

#include "exc.h"
#include "msg.h"
#include "str.h"

#include "qt_app.h"
#include "player_qt.h"
#include "video_output_opengl_qt.h"


static std::vector<std::string> opengl_version_vector;

static void set_opengl_version_vector()
{
    opengl_version_vector.push_back(std::string("OpenGL version ") + reinterpret_cast<const char *>(glGetString(GL_VERSION)));
    opengl_version_vector.push_back(std::string("OpenGL renderer ") + reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
    opengl_version_vector.push_back(std::string("OpenGL vendor ") + reinterpret_cast<const char *>(glGetString(GL_VENDOR)));
}

video_output_opengl_qt_widget::video_output_opengl_qt_widget(
        video_output_opengl_qt *vo, const QGLFormat &format, QWidget *parent)
    : QGLWidget(format, parent), _vo(vo)
{
    setFocusPolicy(Qt::StrongFocus);
    setWindowIcon(QIcon(":icons/appicon.png"));
}

video_output_opengl_qt_widget::~video_output_opengl_qt_widget()
{
}

void video_output_opengl_qt_widget::initializeGL()
{
    if (opengl_version_vector.size() == 0)
    {
        set_opengl_version_vector();
    }
    try
    {
        GLenum err = glewInit();
        if (err != GLEW_OK)
        {
            throw exc(std::string("cannot initialize GLEW: ")
                    + reinterpret_cast<const char *>(glewGetErrorString(err)));
        }
        _vo->initialize(
                glewIsSupported("GL_ARB_pixel_buffer_object"),
                glewIsSupported("GL_ARB_texture_non_power_of_two"),
                glewIsSupported("GL_ARB_fragment_shader"));
    }
    catch (std::exception &e)
    {
        QMessageBox::critical(this, "Error", tr("%1").arg(e.what()));
        abort();
    }
}

void video_output_opengl_qt_widget::paintGL()
{
    _vo->display();
}

void video_output_opengl_qt_widget::resizeGL(int w, int h)
{
    _vo->reshape(w, h);
}

void video_output_opengl_qt_widget::moveEvent(QMoveEvent *)
{
    if (_vo->mode() == video_output::even_odd_rows
            || _vo->mode() == video_output::even_odd_columns
            || _vo->mode() == video_output::checkerboard)
    {
        update();
    }
}

void video_output_opengl_qt_widget::closeEvent(QCloseEvent *)
{
    _vo->send_cmd(command::toggle_play);
}

void video_output_opengl_qt_widget::keyPressEvent(QKeyEvent *event)
{
    switch (event->key())
    {
    case Qt::Key_Escape:
    case Qt::Key_Q:
        _vo->send_cmd(command::toggle_play);
        break;
    case Qt::Key_S:
        _vo->send_cmd(command::toggle_swap_eyes);
        break;
    case Qt::Key_F:
        _vo->send_cmd(command::toggle_fullscreen);
        break;
    case Qt::Key_C:
        _vo->send_cmd(command::center);
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

QSize video_output_opengl_qt_widget::sizeHint() const
{
    return QSize(std::max(_vo->win_width(), 128), std::max(_vo->win_height(), 128));
}


video_output_opengl_qt::video_output_opengl_qt(QWidget *container_widget) throw ()
    : video_output_opengl(), _qt_app_owner(false),
    _container_widget(container_widget), _container_is_external(container_widget != NULL),
    _widget(NULL)
{
    _qt_app_owner = init_qt();
    if (!_container_widget)
    {
        _container_widget = new QWidget();
    }
}

video_output_opengl_qt::~video_output_opengl_qt()
{
    delete _widget;
    delete _container_widget;
    if (_qt_app_owner)
    {
        exit_qt();
    }
}

int video_output_opengl_qt::window_pos_x()
{
    int xo = _container_is_external ? _container_widget->geometry().x() : 0;
    return _container_widget->window()->geometry().x() + xo;
}

int video_output_opengl_qt::window_pos_y()
{
    int yo = _container_is_external ? _container_widget->geometry().y() : 0;
    return _container_widget->window()->geometry().y() + yo;
}

bool video_output_opengl_qt::supports_stereo()
{
    QGLFormat fmt;
    fmt.setStereo(true);
    // The following settings are not strictly necessary, but it may be better
    // to try the exact same format that will be used by the real widget later.
    fmt.setAlpha(true);
    fmt.setDoubleBuffer(true);
    fmt.setSwapInterval(1);
    QGLWidget *tmpwidget = new QGLWidget(fmt);
    bool ret = tmpwidget->format().stereo();
    delete tmpwidget;
    return ret;
}

void video_output_opengl_qt::open(
        enum decoder::video_frame_format preferred_frame_format,
        int src_width, int src_height, float src_aspect_ratio,
        int mode, const video_output_state &state, unsigned int flags,
        int win_width, int win_height)
{
    set_mode(static_cast<enum video_output::mode>(mode));
    set_source_info(src_width, src_height, src_aspect_ratio, preferred_frame_format);

    // Initialize widget
    QGLFormat fmt;
    fmt.setAlpha(true);
    fmt.setDoubleBuffer(true);
    fmt.setSwapInterval(1);
    if (mode == stereo)
    {
        fmt.setStereo(true);
    }
    _widget = new video_output_opengl_qt_widget(this, fmt, _container_widget);
    if (!_widget->format().alpha() || !_widget->format().doubleBuffer()
            || (mode == stereo && !_widget->format().stereo()))
    {
        if (mode == stereo)
        {
            // Common failure: display does not support quad buffered stereo
            throw exc("display does not support stereo mode");
        }
        else
        {
            // Should never happen
            throw exc("cannot set GL context format");
        }
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
        enter_fullscreen();
    }
    else
    {
        _widget->resize(video_output_opengl::win_width(), video_output_opengl::win_height());
    }
    if (flags & video_output::center)
    {
        center();
    }
    set_state(state);

    QGridLayout *container_layout = new QGridLayout();
    container_layout->addWidget(_widget, 0, 0);
    container_layout->setContentsMargins(0, 0, 0, 0);
    container_layout->setRowStretch(0, 1);
    container_layout->setColumnStretch(0, 1);
    delete _container_widget->layout();
    _container_widget->setLayout(container_layout);
    _container_widget->adjustSize();

    _widget->show();
    _container_widget->show();
    if (!_container_is_external && (mode == even_odd_rows || mode == even_odd_columns || mode == checkerboard))
    {
        // XXX: This is a workaround for a Qt bug: the geometry() function for the widget (used in window_pos_x() and
        // window_pos_y()) always returns (0,0) until the window is first moved, or until we hide and re-show it.
        // The above output methods depend on a correct geometry value.
        _container_widget->hide();
        _container_widget->show();
    }
}

void video_output_opengl_qt::activate()
{
    swap_tex_set();
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

void video_output_opengl_qt::center()
{
    if (!state().fullscreen)
    {
        // Move the window, not the widget, so that this also works inside the GUI.
        int xo = _container_is_external ? _container_widget->geometry().x() : 0;
        int yo = _container_is_external ? _container_widget->geometry().y() : 0;
        _container_widget->window()->setGeometry(
                (video_output_opengl::screen_width() - video_output_opengl::win_width()) / 2 - xo,
                (video_output_opengl::screen_height() - video_output_opengl::win_height()) / 2 - yo,
                _container_widget->window()->width(), _container_widget->window()->height());
        _widget->setFocus(Qt::OtherFocusReason);
    }
}

void video_output_opengl_qt::enter_fullscreen()
{
    if (!state().fullscreen)
    {
        if (_container_is_external)
        {
            _container_widget->setWindowFlags(Qt::Window);
        }
        _container_widget->setWindowState(_widget->windowState() | Qt::WindowFullScreen);
        _container_widget->setCursor(Qt::BlankCursor);
        _container_widget->show();
        _widget->setFocus(Qt::OtherFocusReason);
        state().fullscreen = true;
    }
}

void video_output_opengl_qt::exit_fullscreen()
{
    if (state().fullscreen)
    {
        if (_container_is_external)
        {
            _container_widget->setWindowFlags(Qt::Widget);
        }
        _container_widget->setWindowState(_widget->windowState() & ~Qt::WindowFullScreen);
        _container_widget->setCursor(Qt::ArrowCursor);
        _widget->resize(video_output_opengl::win_width(), video_output_opengl::win_height());
        _container_widget->show();
        _widget->setFocus(Qt::OtherFocusReason);
        state().fullscreen = false;
    }
}

void video_output_opengl_qt::receive_notification(const notification &note)
{
    switch (note.type)
    {
    case notification::play:
        if (!note.current.flag)
        {
            uint8_t *null_data[3] = { NULL, NULL, NULL };
            size_t null_line_size[3] = { 0, 0, 0 };
            prepare(null_data, null_line_size, null_data, null_line_size);
            if (state().fullscreen)
            {
                exit_fullscreen();
            }
            _widget->update();
        }
        break;
    case notification::pause:
        /* handled by player */
        break;
    case notification::swap_eyes:
        state().swap_eyes = note.current.flag;
        _widget->update();
        break;
    case notification::fullscreen:
        if (note.previous.flag != note.current.flag)
        {
            if (note.previous.flag)
            {
                exit_fullscreen();
            }
            else
            {
                enter_fullscreen();
            }
        }
        break;
    case notification::center:
        center();
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

std::vector<std::string> opengl_versions()
{
    if (opengl_version_vector.size() == 0)
    {
#ifdef Q_WS_X11
        const char *display = getenv("DISPLAY");
        bool have_display = (display && display[0] != '\0');
#else
        bool have_display = true;
#endif
        if (have_display)
        {
            bool qt_app_owner = init_qt();
            QGLWidget *tmpwidget = new QGLWidget();
            tmpwidget->makeCurrent();
            set_opengl_version_vector();
            delete tmpwidget;
            if (qt_app_owner)
            {
                exit_qt();
            }
        }
    }
    if (opengl_version_vector.size() == 0)
    {
        std::vector<std::string> v;
        v.push_back("OpenGL unknown");
        return v;
    }
    else
    {
        return opengl_version_vector;
    }
}
