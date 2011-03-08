/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010-2011
 * Martin Lambers <marlam@marlam.de>
 * Frédéric Devernay <frederic.devernay@inrialpes.fr>
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
#include "dbg.h"

#include "qt_app.h"
#include "video_output_qt.h"
#include "lib_versions.h"


/* The GL widget */

video_output_qt_widget::video_output_qt_widget(
        video_output_qt *vo, const QGLFormat &format, QWidget *parent) :
    QGLWidget(format, parent), _vo(vo)
{
    setFocusPolicy(Qt::StrongFocus);
}

video_output_qt_widget::~video_output_qt_widget()
{
}

void video_output_qt_widget::move_event()
{
    if (_vo->need_redisplay_on_move())
    {
        update();
    }
}

void video_output_qt_widget::paintGL()
{
    try
    {
        _vo->display_current_frame();
    }
    catch (std::exception &e)
    {
        QMessageBox::critical(this, "Error", e.what());
        // Disable further output and stop the player
        _vo->prepare_next_frame(video_frame());
        _vo->activate_next_frame();
        _vo->send_cmd(command::toggle_play);
    }
}

void video_output_qt_widget::resizeGL(int w, int h)
{
    _vo->reshape(w, h);
}

void video_output_qt_widget::moveEvent(QMoveEvent *)
{
    move_event();
}

void video_output_qt_widget::keyPressEvent(QKeyEvent *event)
{
    if (_vo->_container_is_external && !_vo->_playing)
    {
        QGLWidget::keyPressEvent(event);
        return;
    }
    switch (event->key())
    {
    case Qt::Key_Escape:
    case Qt::Key_Q:
        _vo->send_cmd(command::toggle_play);
        break;
    case Qt::Key_S:
        _vo->send_cmd(command::toggle_stereo_mode_swap);
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
    case Qt::Key_V:
        _vo->send_cmd(command::cycle_video_stream);
        break;
    case Qt::Key_A:
        _vo->send_cmd(command::cycle_audio_stream);
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
    case Qt::Key_ParenLeft:
        _vo->send_cmd(command::adjust_ghostbust, -0.01f);
        break;
    case Qt::Key_ParenRight:
        _vo->send_cmd(command::adjust_ghostbust, +0.01f);
        break;
    case Qt::Key_Less:
        _vo->send_cmd(command::adjust_parallax, -0.01f);
        break;
    case Qt::Key_Greater:
        _vo->send_cmd(command::adjust_parallax, +0.01f);
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

void video_output_qt_widget::mouseReleaseEvent(QMouseEvent *event)
{
    _vo->mouse_set_pos(std::max(std::min(static_cast<float>(event->posF().x()) / this->width(), 1.0f), 0.0f));
}


/* Our own video container widget, used in case that the video_output_qt
 * constructor is called without an external container widget. */

video_container_widget::video_container_widget(QWidget *parent) : QWidget(parent), _w(64), _h(64)
{
    setWindowIcon(QIcon(":logo/bino_logo_small_64x64.png"));
    // Set minimum size > 0 so that the container is always visible
    setMinimumSize(_w, _h);
    // Set suitable size policy
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    // Always paint the complete widget black
    QPalette p(palette());
    p.setColor(QPalette::Background, Qt::black);
    setPalette(p);
    setAutoFillBackground(true);
}

void video_container_widget::set_recommended_size(int w, int h)
{
    _w = w;
    _h = h;
}

QSize video_container_widget::sizeHint() const
{
    return QSize(_w, _h);
}

void video_container_widget::moveEvent(QMoveEvent *)
{
    emit move_event();
}

void video_container_widget::closeEvent(QCloseEvent *)
{
    send_cmd(command::toggle_play);
}


/* The video_output_qt class */

video_output_qt::video_output_qt(bool benchmark, video_container_widget *container_widget) :
    video_output(true),
    _qt_app_owner(false),
    _container_widget(container_widget),
    _container_is_external(container_widget != NULL),
    _widget(NULL),
    _fullscreen(false),
    _playing(false)
{
    _qt_app_owner = init_qt();
    if (!_container_widget)
    {
        _container_widget = new video_container_widget(NULL);
    }
    // Alpha is disabled for now, because we do not use it yet and some OpenGL
    // implementations do not support Alpha with a quad-buffer context according to
    // <http://lists.nongnu.org/archive/html/bino-list/2011-03/msg00011.html>.
    // However, we may need to re-enable it for subtitle or OSD support.
    //_format.setAlpha(true);
    _format.setDoubleBuffer(true);
    if (!benchmark)
    {
        _format.setSwapInterval(1);
    }
    _format.setStereo(false);
}

video_output_qt::~video_output_qt()
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

void video_output_qt::init()
{
    if (!_widget)
    {
        create_widget();
        _widget->makeCurrent();
        set_opengl_versions();
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
                            "OpenGL 2.1 and framebuffer objects."));
            }
        }
        catch (std::exception &e)
        {
            QMessageBox::critical(_widget, "Error", e.what());
            abort();
        }
        video_output::init();
        video_output::clear();
        // Initialize GL things
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
    }
}

void video_output_qt::deinit()
{
    if (_widget)
    {
        _widget->makeCurrent();
        video_output::deinit();
        _widget->doneCurrent();
        delete _widget;
        _widget = NULL;
    }
}

void video_output_qt::create_widget()
{
    _widget = new video_output_qt_widget(this, _format, _container_widget);
    QObject::connect(_container_widget, SIGNAL(move_event()), _widget, SLOT(move_event()));
    if ((_format.alpha() && !_widget->format().alpha())
            || (_format.doubleBuffer() && !_widget->format().doubleBuffer())
            || (_format.stereo() && !_widget->format().stereo()))
    {
        try
        {
            if (_format.stereo())
            {
                // Common failure: display does not support quad buffered stereo
                throw exc("The display does not support OpenGL stereo mode.");
            }
            else
            {
                // Should never happen
                throw exc("Cannot set GL context format.");
            }
        }
        catch (std::exception &e)
        {
            QMessageBox::critical(_widget, "Error", e.what());
            abort();
        }
    }
    QGridLayout *container_layout = new QGridLayout();
    container_layout->addWidget(_widget, 0, 0);
    container_layout->setContentsMargins(0, 0, 0, 0);
    container_layout->setRowStretch(0, 1);
    container_layout->setColumnStretch(0, 1);
    delete _container_widget->layout();
    _container_widget->setLayout(container_layout);
    _container_widget->updateGeometry();
    _widget->show();
    _container_widget->show();
    _container_widget->raise();
    process_events();
}

void video_output_qt::make_context_current()
{
    _widget->makeCurrent();
}

bool video_output_qt::context_is_stereo()
{
    return (_format.stereo());
}

void video_output_qt::recreate_context(bool stereo)
{
    deinit();
    _format.setStereo(stereo);
    init();
    clear();
}

void video_output_qt::trigger_update()
{
    _widget->update();
}

void video_output_qt::trigger_resize(int w, int h)
{
    _container_widget->set_recommended_size(w, h);
    _container_widget->updateGeometry();
    _container_widget->window()->adjustSize();
}

void video_output_qt::mouse_set_pos(float dest)
{
    if (_fullscreen || _container_is_external)
    {
        // Disabled in fullscreen and GUI mode
        return;
    }
    if (_playing || !_container_is_external)
    {
        send_cmd(command::set_pos, dest);
    }
}

void video_output_qt::move_event()
{
    if (_widget)
    {
        _widget->move_event();
    }
}

bool video_output_qt::supports_stereo() const
{
    QGLFormat fmt = _format;
    fmt.setStereo(true);
    QGLWidget *tmpwidget = new QGLWidget(fmt);
    bool ret = tmpwidget->format().stereo();
    delete tmpwidget;
    return ret;
}

int video_output_qt::screen_width()
{
    return QApplication::desktop()->screenGeometry().width();
}

int video_output_qt::screen_height()
{
    return QApplication::desktop()->screenGeometry().height();
}

float video_output_qt::screen_aspect_ratio()
{
    return static_cast<float>(QApplication::desktop()->widthMM())
        / static_cast<float>(QApplication::desktop()->heightMM());
}

int video_output_qt::width()
{
    return _widget->width();
}

int video_output_qt::height()
{
    return _widget->height();
}

float video_output_qt::aspect_ratio()
{
    return static_cast<float>(_widget->widthMM())
        / static_cast<float>(_widget->heightMM());
}

int video_output_qt::pos_x()
{
    return _widget->mapToGlobal(QPoint(0, 0)).x();
}

int video_output_qt::pos_y()
{
    return _widget->mapToGlobal(QPoint(0, 0)).y();
}

void video_output_qt::center()
{
    if (!_fullscreen)
    {
        // Move the window, not the widget, so that this also works inside the GUI.
        int dest_screen_pos_x = (screen_width() - width()) / 2;
        int dest_screen_pos_y = (screen_height() - height()) / 2;
        int window_offset_x = _widget->mapTo(_widget->window(), QPoint(0, 0)).x();
        int window_offset_y = _widget->mapTo(_widget->window(), QPoint(0, 0)).y();
        _widget->window()->setGeometry(
                dest_screen_pos_x - window_offset_x,
                dest_screen_pos_y - window_offset_y,
                _widget->window()->width(), _widget->window()->height());
        _widget->setFocus(Qt::OtherFocusReason);
    }
}

void video_output_qt::enter_fullscreen()
{
    if (!_fullscreen)
    {
        _fullscreen = true;
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

bool video_output_qt::toggle_fullscreen()
{
    if (_fullscreen)
    {
        exit_fullscreen();
    }
    else
    {
        enter_fullscreen();
    }
    return _fullscreen;
}

void video_output_qt::exit_fullscreen()
{
    if (_fullscreen)
    {
        _fullscreen = false;
        if (_container_is_external)
        {
            _container_widget->setWindowFlags(Qt::Widget);
        }
        _container_widget->setWindowState(_widget->windowState() & ~Qt::WindowFullScreen);
        _container_widget->setCursor(Qt::ArrowCursor);
        _container_widget->show();
        _widget->setFocus(Qt::OtherFocusReason);
    }
}

bool video_output_qt::has_events()
{
    return QApplication::hasPendingEvents();
}

void video_output_qt::process_events()
{
    QApplication::sendPostedEvents();
    QApplication::processEvents();
}

void video_output_qt::receive_notification(const notification &note)
{
    if (note.type == notification::play)
    {
        std::istringstream current(note.current);
        s11n::load(current, _playing);
    }
    /* More is currently not implemented.
     * In the future, an on-screen display might show hints about what happened. */
}
