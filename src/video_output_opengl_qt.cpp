/*
 * This file is part of bino, a 3D video player.
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
#include <QPalette>

#include "exc.h"
#include "msg.h"
#include "str.h"

#include "qt_app.h"
#include "player_qt.h"
#include "video_output_opengl_qt.h"


/* OpenGL version handling */

static std::vector<std::string> opengl_version_vector;

static void set_opengl_version_vector()
{
    opengl_version_vector.push_back(std::string("OpenGL version ") + reinterpret_cast<const char *>(glGetString(GL_VERSION)));
    opengl_version_vector.push_back(std::string("OpenGL renderer ") + reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
    opengl_version_vector.push_back(std::string("OpenGL vendor ") + reinterpret_cast<const char *>(glGetString(GL_VENDOR)));
}

/* The GL widget */

video_output_opengl_qt_widget::video_output_opengl_qt_widget(
        video_output_opengl_qt *vo, const QGLFormat &format, QWidget *parent)
    : QGLWidget(format, parent), _vo(vo)
{
    setFocusPolicy(Qt::StrongFocus);
    setWindowIcon(QIcon(":icons/appicon.png"));
}

video_output_opengl_qt_widget::~video_output_opengl_qt_widget()
{
    makeCurrent();
    _vo->deinitialize();
}

void video_output_opengl_qt_widget::move_event()
{
    // The masking modes must know if the video area starts with an even or
    // odd columns and/or row. If this changes, the display must be updated.
    if (_vo->mode() == video_output::even_odd_rows
            || _vo->mode() == video_output::even_odd_columns
            || _vo->mode() == video_output::checkerboard)
    {
        update();
    }
}

void video_output_opengl_qt_widget::initializeGL()
{
    _vo->clear();
    if (opengl_version_vector.size() == 0)
    {
        set_opengl_version_vector();
    }
    try
    {
        GLenum err = glewInit();
        if (err != GLEW_OK)
        {
            throw exc(std::string("Cannot initialize GLEW: ")
                    + reinterpret_cast<const char *>(glewGetErrorString(err)));
        }
        if (!glewIsSupported("GL_VERSION_2_1 GL_EXT_framebuffer_object"))
        {
            throw exc(std::string("This OpenGL implementation does not support "
                        "OpenGL 2.1 and framebuffer objects"));
        }
        _vo->initialize();
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
    move_event();
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


/* Our own video container widget, used in case that the video_output_opengl_qt
 * constructor is called without an external container widget. */

video_container_widget::video_container_widget(QWidget *parent) : QWidget(parent)
{
    // Set minimum size > 0 so that the container is always visible
    setMinimumSize(64, 64);
    // Always paint the complete widget black
    QPalette p(palette());
    p.setColor(QPalette::Background, Qt::black);
    setPalette(p);
    setAutoFillBackground(true);
}

void video_container_widget::moveEvent(QMoveEvent *)
{
    emit move_event();
}

void video_container_widget::closeEvent(QCloseEvent *)
{
    send_cmd(command::toggle_play);
}


/* The video_output_opengl_qt class */

video_output_opengl_qt::video_output_opengl_qt(video_container_widget *container_widget) throw ()
    : video_output_opengl(), _qt_app_owner(false),
    _container_widget(container_widget), _container_is_external(container_widget != NULL),
    _widget(NULL)
{
    _qt_app_owner = init_qt();
    if (!_container_widget)
    {
        _container_widget = new video_container_widget(NULL);
    }
}

video_output_opengl_qt::~video_output_opengl_qt()
{
    delete _widget;
    if (!_container_is_external)
    {
        delete _container_widget;
    }
    if (_qt_app_owner)
    {
        exit_qt();
    }
}

int video_output_opengl_qt::screen_pos_x()
{
    return _widget->mapToGlobal(QPoint(0, 0)).x();
}

int video_output_opengl_qt::screen_pos_y()
{
    return _widget->mapToGlobal(QPoint(0, 0)).y();
}

void video_output_opengl_qt::move_event()
{
    if (_widget)
    {
        _widget->move_event();
    }
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
        enum decoder::video_frame_format src_format, bool src_mono,
        int src_width, int src_height, float src_aspect_ratio,
        int mode, const video_output_state &state, unsigned int flags,
        int win_width, int win_height)
{
    if (_widget)
    {
        close();
    }

    set_mode(static_cast<enum video_output::mode>(mode));
    set_source_info(src_width, src_height, src_aspect_ratio, src_format, src_mono);

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
    QObject::connect(_container_widget, SIGNAL(move_event()), _widget, SLOT(move_event()));
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


    _widget->resize(video_output_opengl::win_width(), video_output_opengl::win_height());
    if (flags & video_output::center)
    {
        center();
    }

    QGridLayout *container_layout = new QGridLayout();
    container_layout->addWidget(_widget, 0, 0);
    container_layout->setContentsMargins(0, 0, 0, 0);
    container_layout->setRowStretch(0, 1);
    container_layout->setColumnStretch(0, 1);
    delete _container_widget->layout();
    _container_widget->setLayout(container_layout);
    _container_widget->adjustSize();
    if (state.fullscreen)
    {
        enter_fullscreen();
    }
    set_state(state);

    _widget->show();
    _container_widget->show();
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
    exit_fullscreen();
    delete _widget;
    _widget = NULL;
}

void video_output_opengl_qt::center()
{
    if (!state().fullscreen)
    {
        // Move the window, not the widget, so that this also works inside the GUI.
        int dest_screen_pos_x = (video_output_opengl::screen_width() - video_output_opengl::win_width()) / 2;
        int dest_screen_pos_y = (video_output_opengl::screen_height() - video_output_opengl::win_height()) / 2;
        int window_offset_x = _widget->mapTo(_widget->window(), QPoint(0, 0)).x();
        int window_offset_y = _widget->mapTo(_widget->window(), QPoint(0, 0)).y();
        _widget->window()->setGeometry(
                dest_screen_pos_x - window_offset_x,
                dest_screen_pos_y - window_offset_y,
                _widget->window()->width(), _widget->window()->height());
        _widget->setFocus(Qt::OtherFocusReason);
    }
}

void video_output_opengl_qt::enter_fullscreen()
{
    if (!state().fullscreen)
    {
        state().fullscreen = true;
        if (_container_is_external)
        {
            _container_widget->setWindowFlags(Qt::Window);
        }
        _container_widget->setWindowState(_widget->windowState() | Qt::WindowFullScreen);
        _container_widget->setCursor(Qt::BlankCursor);
        _container_widget->show();
        _widget->setFocus(Qt::OtherFocusReason);
    }
}

void video_output_opengl_qt::exit_fullscreen()
{
    if (state().fullscreen)
    {
        state().fullscreen = false;
        if (_container_is_external)
        {
            _container_widget->setWindowFlags(Qt::Widget);
        }
        _container_widget->setWindowState(_widget->windowState() & ~Qt::WindowFullScreen);
        _container_widget->setCursor(Qt::ArrowCursor);
        _widget->resize(video_output_opengl::win_width(), video_output_opengl::win_height());
        _container_widget->show();
        _widget->setFocus(Qt::OtherFocusReason);
    }
}

void video_output_opengl_qt::receive_notification(const notification &note)
{
    if (!_widget)
    {
        return;
    }

    switch (note.type)
    {
    case notification::play:
        if (!note.current.flag)
        {
            exit_fullscreen();
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
