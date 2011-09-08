/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010-2011
 * Martin Lambers <marlam@marlam.de>
 * Frédéric Devernay <frederic.devernay@inrialpes.fr>
 * Joe <cuchac@email.cz>
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
#include <unistd.h>

#include <QApplication>
#include <QDesktopWidget>
#include <QGridLayout>
#include <QKeyEvent>
#include <QIcon>
#include <QMessageBox>
#include <QPalette>
#include <QProcess>
#include <QDialog>
#include <QLabel>
#ifdef Q_WS_X11
# include <QX11Info>
# include <X11/Xlib.h>
#endif

#include "gettext.h"
#define _(string) gettext(string)

#include "exc.h"
#include "msg.h"
#include "str.h"
#include "dbg.h"
#include "timer.h"

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
        QMessageBox::critical(this, _("Error"), e.what());
        // Disable further output and stop the player
        _vo->prepare_next_frame(video_frame(), subtitle_box());
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
        // ESC should stop playback, unless we're in fullscreen mode.
        // In that case, it should simply leave fullscreen mode, since
        // this is what most users expect.
        if (_vo->_fullscreen)
        {
            _vo->send_cmd(command::toggle_fullscreen);
        }
        else
        {
            _vo->send_cmd(command::toggle_play);
        }
        break;
    case Qt::Key_Q:
        _vo->send_cmd(command::toggle_play);
        break;
    case Qt::Key_E:
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
    case Qt::Key_S:
        _vo->send_cmd(command::cycle_subtitle_stream);
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

void video_output_qt_widget::mouseDoubleClickEvent(QMouseEvent *)
{
    _vo->mouse_toggle_fullscreen();
}

void video_output_qt_widget::focusOutEvent(QFocusEvent *)
{
    setFocus(Qt::OtherFocusReason);
}


/* Our own video container widget, used in case that the video_output_qt
 * constructor is called without an external container widget. */

video_container_widget::video_container_widget(QWidget *parent) : QWidget(parent), _w(64), _h(64)
{
    setWindowIcon(QIcon(":logo/bino/64x64/bino.png"));
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
    video_output(),
    _container_widget(container_widget),
    _container_is_external(container_widget != NULL),
    _widget(NULL),
    _fullscreen(false),
    _playing(false)
{
    if (!_container_widget)
    {
        _container_widget = new video_container_widget(NULL);
    }
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
}

void video_output_qt::init()
{
    if (!_widget)
    {
        create_widget();
        _widget->makeCurrent();
        set_opengl_versions();
        GLenum err = glewInit();
        if (err != GLEW_OK)
        {
            throw exc(str::asprintf(_("Cannot initialize GLEW: %s"),
                        reinterpret_cast<const char *>(glewGetErrorString(err))));
        }
        if (!glewIsSupported("GL_VERSION_2_1 GL_EXT_framebuffer_object"))
        {
            throw exc(std::string(_("This OpenGL implementation does not support "
                            "OpenGL 2.1 and framebuffer objects.")));
        }
        video_output::init();
        video_output::clear();
        // Initialize GL things
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
    }
}

int64_t video_output_qt::wait_for_subtitle_renderer()
{
    if (_subtitle_renderer.is_initialized())
    {
        return 0;
    }
    int64_t wait_start = timer::get_microseconds(timer::monotonic);
    exc init_exception;
    QDialog *mbox = NULL;
    // Show a dialog only in GUI mode
    if (_container_is_external && !_fullscreen)
    {
        mbox = new QDialog(_container_widget);
        mbox->setModal(true);
        mbox->setWindowTitle(_("Please wait"));
        QGridLayout *mbox_layout = new QGridLayout;
        QLabel *mbox_label = new QLabel(_("Waiting for subtitle renderer initialization..."));
        mbox_layout->addWidget(mbox_label, 0, 0);
        mbox->setLayout(mbox_layout);
        mbox->show();
    }
    else
    {
        msg::wrn(_("Waiting for subtitle renderer initialization..."));
    }
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    try
    {
        while (!_subtitle_renderer.is_initialized())
        {
            process_events();
            usleep(10000);
        }
    }
    catch (std::exception &e)
    {
        init_exception = e;
    }
    QApplication::restoreOverrideCursor();
    if (mbox)
    {
        mbox->hide();
        delete mbox;
    }
    if (!init_exception.empty())
    {
        throw init_exception;
    }
    int64_t wait_stop = timer::get_microseconds(timer::monotonic);
    return (wait_stop - wait_start);
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
    if (!_widget->context()->isValid())
    {
        // TODO: replace the error message with a better one once the freeze
        // of translatable strings is over.
        QMessageBox::critical(_widget, _("Error"), _("Cannot set OpenGL context format."));
        std::exit(1);
    }
    QObject::connect(_container_widget, SIGNAL(move_event()), _widget, SLOT(move_event()));
    if ((_format.doubleBuffer() && !_widget->format().doubleBuffer())
            || (_format.stereo() && !_widget->format().stereo()))
    {
        try
        {
            if (_format.stereo())
            {
                // Common failure: display does not support quad buffered stereo
                throw exc(_("The display does not support OpenGL stereo mode."));
            }
            else
            {
                // Should never happen
                throw exc(_("Cannot set OpenGL context format."));
            }
        }
        catch (std::exception &e)
        {
            QMessageBox::critical(_widget, _("Error"), e.what());
            std::exit(1);
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
    if (!_container_is_external)
    {
        _container_widget->adjustSize();
    }
}

void video_output_qt::mouse_set_pos(float dest)
{
    if (_fullscreen || _container_is_external)
    {
        // Disabled in fullscreen and GUI mode
        return;
    }
    if (_playing)
    {
        send_cmd(command::set_pos, dest);
    }
}

void video_output_qt::mouse_toggle_fullscreen()
{
    if (!_container_is_external)
    {
        // Disabled in non-GUI mode
        return;
    }
    if (_playing)
    {
        send_cmd(command::toggle_fullscreen);
    }
}

void video_output_qt::suspend_screensaver()
{
#if defined(Q_WS_X11)
    if (QProcess::execute(QString("xdg-screensaver suspend ")
                + str::from(_container_widget->winId()).c_str()) != 0)
    {
        msg::wrn(_("Cannot suspend screensaver."));
    }
#elif defined(Q_WS_WIN)
    /* TODO */
#elif defined(Q_WS_MAC)
    /* TODO */
#endif
}

void video_output_qt::resume_screensaver()
{
#if defined(Q_WS_X11)
    if (QProcess::execute(QString("xdg-screensaver resume ")
                + str::from(_container_widget->winId()).c_str()) != 0)
    {
        msg::wrn(_("Cannot resume screensaver."));
    }
#elif defined(Q_WS_WIN)
    /* TODO */
#elif defined(Q_WS_MAC)
    /* TODO */
#endif
}

void video_output_qt::move_event()
{
    if (_widget)
    {
        _widget->move_event();
    }
}

void video_output_qt::grab_focus()
{
    if (_widget)
    {
        _widget->setFocus(Qt::OtherFocusReason);
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

float video_output_qt::screen_pixel_aspect_ratio()
{
    float screen_pixel_ar =
        static_cast<float>(QApplication::desktop()->logicalDpiY())
        / static_cast<float>(QApplication::desktop()->logicalDpiX());
    if (std::fabs(screen_pixel_ar - 1.0f) < 0.03f)
    {
        // This screen most probably has square pixels, and the difference to 1.0
        // is only due to slightly inaccurate measurements and rounding. Force
        // 1.0 so that the user gets expected results.
        screen_pixel_ar = 1.0f;
    }
    return screen_pixel_ar;
}

int video_output_qt::width()
{
    return _widget->width();
}

int video_output_qt::height()
{
    return _widget->height();
}

int video_output_qt::pos_x()
{
    return _widget->mapToGlobal(QPoint(0, 0)).x();
}

int video_output_qt::pos_y()
{
    return _widget->mapToGlobal(QPoint(0, 0)).y();
}

bool video_output_qt::fullscreen()
{
    return _fullscreen;
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
    }
}

void video_output_qt::enter_fullscreen(int screens)
{
    if (!_fullscreen)
    {
        _fullscreen = true;
        if (_container_is_external)
        {
            _container_widget->setWindowFlags(Qt::Window);
        }
        // Determine combined geometry of the chosen screens.
        QRect geom(0, 0, 0, 0);
        int screen_count = 0;
        for (int i = 0; i < std::min(QApplication::desktop()->screenCount(), 16); i++)
        {
            if (screens & (1 << i))
            {
                if (screen_count == 0)
                {
                    geom = QApplication::desktop()->screenGeometry(i);
                }
                else
                {
                    geom = geom.united(QApplication::desktop()->screenGeometry(i));
                }
                screen_count++;
            }
        }
        if (screen_count <= 1)
        {
            // The single screen case is supported by Qt.
            _container_widget->setWindowState(_widget->windowState() | Qt::WindowFullScreen);
            if (screen_count == 1)
            {
                // Set the chosen screen
                _container_widget->setGeometry(geom);
            }
            _container_widget->setCursor(Qt::BlankCursor);
            _container_widget->show();
        }
        else
        {
            // In the dual and multi screen cases we need to bypass the window manager
            // on X11 because Qt does not support _NET_WM_FULLSCREEN_MONITORS, and thus
            // the window manager would always restrict the fullscreen window to one screen.
            // Note: it may be better to set _NET_WM_FULLSCREEN_MONITORS ourselves, but that
            // would also require the window manager to support this extension...
            _container_widget->setWindowFlags(_container_widget->windowFlags()
                    | Qt::X11BypassWindowManagerHint
                    | Qt::FramelessWindowHint
                    | Qt::WindowStaysOnTopHint);
            _container_widget->setWindowState(_widget->windowState() | Qt::WindowFullScreen);
            _container_widget->setGeometry(geom);
            _container_widget->setCursor(Qt::BlankCursor);
            _container_widget->show();
            _container_widget->raise();
            _container_widget->activateWindow();
#ifdef Q_WS_X11
            /* According to the Qt documentation, it should be sufficient to call activateWindow()
             * to make a X11 window active when using Qt::X11BypassWindowManagerHint, but this
             * does not work for me (Ubuntu 11.04 Gnome-2D desktop). This is a workaround. */
            /* Note that using X11 functions directly means that we have to link with libX11
             * explicitly; see configure.ac. */
            QApplication::syncX();      // just for safety; not sure if it is necessary
            XSetInputFocus(QX11Info::display(), _container_widget->winId(), RevertToParent, CurrentTime);
            XFlush(QX11Info::display());
#endif
        }
        grab_focus();
        // Suspend the screensaver after going fullscreen, so that our window ID
        // represents the fullscreen window. We need to have the same ID for resume.
        suspend_screensaver();
    }
}

bool video_output_qt::toggle_fullscreen(int screens)
{
    if (_fullscreen)
    {
        exit_fullscreen();
    }
    else
    {
        enter_fullscreen(screens);
    }
    return _fullscreen;
}

void video_output_qt::exit_fullscreen()
{
    if (_fullscreen)
    {
        // Resume the screensaver before disabling fullscreen, so that our window ID
        // still represents the fullscreen window and was the same when suspending the screensaver.
        resume_screensaver();
        _fullscreen = false;
        if (_container_is_external)
        {
            _container_widget->setWindowFlags(Qt::Widget);
        }
        _container_widget->setWindowFlags(_container_widget->windowFlags()
                & ~Qt::X11BypassWindowManagerHint
                & ~Qt::FramelessWindowHint
                & ~Qt::WindowStaysOnTopHint);
        _container_widget->setWindowState(_widget->windowState() & ~Qt::WindowFullScreen);
        _container_widget->setCursor(Qt::ArrowCursor);
        _container_widget->show();
        grab_focus();
    }
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
