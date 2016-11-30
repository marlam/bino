/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010, 2011, 2012, 2013, 2014, 2015
 * Martin Lambers <marlam@marlam.de>
 * Frédéric Devernay <frederic.devernay@inrialpes.fr>
 * Joe <cuchac@email.cz>
 * Binocle <http://binocle.com> (author: Olivier Letz <oletz@binocle.com>)
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
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <unistd.h>

#include <QCoreApplication>
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
#if HAVE_X11
# include <GL/glxew.h>
#endif
#ifdef Q_OS_MAC
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <AvailabilityMacros.h>
#endif

#include "base/exc.h"
#include "base/msg.h"
#include "base/str.h"
#include "base/dbg.h"
#include "base/tmr.h"

#include "base/gettext.h"
#define _(string) gettext(string)

#include "video_output_qt.h"
#include "lib_versions.h"


/* The GL thread */

gl_thread::gl_thread(video_output_qt* vo_qt, video_output_qt_widget* vo_qt_widget) :
    _vo_qt(vo_qt), _vo_qt_widget(vo_qt_widget),
    _render(false),
    _action_activate(false),
    _action_prepare(false),
    _action_finished(false),
    _failure(false),
    _display_frameno(0)
{
}

#if HAVE_X11
GLXEWContext* gl_thread::glxewGetContext() const
{
    return _vo_qt->glxewGetContext();
}
#endif

void gl_thread::set_render(bool r)
{
    _redisplay = r;
    _render = r;
}

void gl_thread::resize(int w, int h)
{
    _w = w;
    _h = h;
}

void gl_thread::activate_next_frame()
{
    if (_failure)
        return;
    _wait_mutex.lock();
    _action_finished = false;
    _action_activate = true;
    while (_action_activate)
        _wait_cond.wait(_wait_mutex);
    _action_finished = true;
    _wait_mutex.unlock();
}

void gl_thread::prepare_next_frame(const video_frame &frame, const subtitle_box &subtitle)
{
    if (_failure)
        return;
    _wait_mutex.lock();
    _next_subtitle = subtitle;
    _next_frame = frame;
    _action_finished = false;
    _action_prepare = true;
    while (_action_prepare)
        _wait_cond.wait(_wait_mutex);
    _action_finished = true;
    _wait_mutex.unlock();
}

void gl_thread::redisplay()
{
    _redisplay = true;
}

void gl_thread::run()
{
    try {
        assert(_vo_qt_widget->context()->isValid());
        _vo_qt_widget->makeCurrent();
        assert(QGLContext::currentContext() == _vo_qt_widget->context());
        while (_render) {
#if HAVE_X11
            GLuint counter;
            if (GLXEW_SGI_video_sync && glXGetVideoSyncSGI(&counter) == 0)
                _display_frameno = counter;
            else
                _display_frameno++;
#else
            _display_frameno++;
#endif
            // In alternating mode, we should always present both left and right view of
            // a stereo video frame before advancing to the next video frame. This means we
            // can only switch video frames every other output frame.
            if (dispatch::parameters().stereo_mode() != parameters::mode_alternating
                    || _display_frameno % 2 == 0) {
                _wait_mutex.lock();
                if (_action_activate) {
                    try {
                        _vo_qt->video_output::activate_next_frame();
                    }
                    catch (std::exception& e) {
                        _e = e;
                        _render = false;
                        _failure = true;
                    }
                    _action_activate = false;
                    _wait_cond.wake_one();
                    _redisplay = true;
                }
                _wait_mutex.unlock();
            }
            if (_failure)
                break;
            _wait_mutex.lock();
            if (_action_prepare) {
                try {
                    _vo_qt->video_output::prepare_next_frame(_next_frame, _next_subtitle);
                }
                catch (std::exception& e) {
                    _e = e;
                    _render = false;
                    _failure = true;
                }
                _action_prepare = false;
                _wait_cond.wake_one();
            }
            _wait_mutex.unlock();
            if (_failure)
                break;
            if (_w > 0 && _h > 0
                    && (_vo_qt->full_display_width() != _w
                        || _vo_qt->full_display_height() != _h)) {
                _vo_qt->reshape(_w, _h);
                _redisplay = true;
            }
            // In alternating mode, we always need to redisplay
            if (dispatch::parameters().stereo_mode() == parameters::mode_alternating)
                _redisplay = true;
            // If DLP 3-D Ready Sync is active, we always need to redisplay
            if (dispatch::parameters().fullscreen() && dispatch::parameters().fullscreen_3d_ready_sync()
                    && (dispatch::parameters().stereo_mode() == parameters::mode_left_right
                        || dispatch::parameters().stereo_mode() == parameters::mode_left_right_half
                        || dispatch::parameters().stereo_mode() == parameters::mode_top_bottom
                        || dispatch::parameters().stereo_mode() == parameters::mode_top_bottom_half
                        || dispatch::parameters().stereo_mode() == parameters::mode_alternating))
                _redisplay = true;
            // Redisplay if necessary
            if (_redisplay) {
                _redisplay = false;
#if HAVE_LIBXNVCTRL
                _vo_qt->sdi_output(_display_frameno);
#endif // HAVE_LIBXNVCTRL
                _vo_qt->display_current_frame(_display_frameno);
                _vo_qt_widget->swapBuffers();
            } else if (!dispatch::parameters().benchmark()) {
                // do not busy loop
                usleep(1000);
            }
        }
    }
    catch (std::exception& e) {
        _e = e;
        _render = false;
        _failure = true;
    }
    _wait_mutex.lock();
    if (_action_activate || _action_prepare) {
        while (!_action_finished) {
            _wait_cond.wake_one();
            _wait_mutex.unlock();
            _wait_mutex.lock();
        }
    }
    _wait_mutex.unlock();
    _vo_qt_widget->doneCurrent();
#if QT_VERSION >= 0x050000
    _vo_qt_widget->context()->moveToThread(QCoreApplication::instance()->thread());
#endif
}

int64_t gl_thread::time_to_next_frame_presentation()
{
    // TODO: return a good estimate of the number of microseconds that will
    // pass until the next buffer swap completes. For now, just assume that
    // the next frame will display immediately.
    return 0;
}

/* The GL widget */

video_output_qt_widget::video_output_qt_widget(
        video_output_qt *vo, const QGLFormat &format, QWidget *parent) :
    QGLWidget(format, parent), _vo(vo), _gl_thread(vo, this),
    _width(0), _height(0), _pos_x(0), _pos_y(0)
{
    setAutoBufferSwap(false);
    setFocusPolicy(Qt::StrongFocus);
    connect(&_timer, SIGNAL(timeout()), this, SLOT(check_gl_thread()));
}

int video_output_qt_widget::vo_width() const
{
    return _width;
}

int video_output_qt_widget::vo_height() const
{
    return _height;
}

int video_output_qt_widget::vo_pos_x() const
{
    return _pos_x;
}

int video_output_qt_widget::vo_pos_y() const
{
    return _pos_y;
}

void video_output_qt_widget::check_gl_thread()
{
    // XXX We need to know our current position on the screen (global pixel coordinates).
    // Querying our position in vo_pos_*() does not work since that function is called
    // from the GL thread and mapToGlocal() seems to block for some strange reason.
    // Therefore we record our current position here.
    _pos_x = mapToGlobal(QPoint(0, 0)).x();
    _pos_y = mapToGlobal(QPoint(0, 0)).y();
    if (_gl_thread.failure()) {
        stop_rendering();
        QMessageBox::critical(this, _("Error"), _gl_thread.exception().what());
        _vo->send_cmd(command::toggle_play);
    }
}

void video_output_qt_widget::start_rendering()
{
#if QT_VERSION >= 0x050000
    context()->moveToThread(&_gl_thread);
#endif
    _gl_thread.set_render(true);
    _gl_thread.start();
    _timer.start(0);
}

void video_output_qt_widget::stop_rendering()
{
    _gl_thread.set_render(false);
    _gl_thread.wait();
    _timer.stop();
}

void video_output_qt_widget::redisplay()
{
    _gl_thread.redisplay();
}

void video_output_qt_widget::paintEvent(QPaintEvent*)
{
    _gl_thread.redisplay();
}

void video_output_qt_widget::resizeEvent(QResizeEvent* event)
{
    _width = event->size().width();
    _height = event->size().height();
    _gl_thread.resize(_width, _height);
}

void video_output_qt_widget::keyPressEvent(QKeyEvent *event)
{
    switch (event->key())
    {
    case Qt::Key_Escape:
        // ESC should stop playback, unless we're in fullscreen mode.
        // In that case, it should simply leave fullscreen mode, since
        // this is what most users expect.
        if (dispatch::parameters().fullscreen())
        {
            _vo->send_cmd(command::toggle_fullscreen);
        }
        else
        {
            _vo->send_cmd(command::toggle_play);
        }
        break;
    case Qt::Key_Q:
#if QT_VERSION >= 0x040700
    case Qt::Key_MediaStop:
#endif
        _vo->send_cmd(command::toggle_play);
        break;
    case Qt::Key_E:
    case Qt::Key_F7:
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
#if QT_VERSION >= 0x040700
    case Qt::Key_MediaTogglePlayPause:
#endif
        _vo->send_cmd(command::toggle_pause);
        break;
#if QT_VERSION >= 0x040700
    case Qt::Key_MediaPlay:
        if (dispatch::pausing())
        {
            _vo->send_cmd(command::toggle_pause);
        }
        break;
    case Qt::Key_MediaPause:
        if (!dispatch::pausing())
        {
            _vo->send_cmd(command::toggle_pause);
        }
        break;
#endif
    case Qt::Key_Period:
        _vo->send_cmd(command::step);
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
    case Qt::Key_BracketLeft:
        _vo->send_cmd(command::adjust_parallax, -0.01f);
        break;
    case Qt::Key_BracketRight:
        _vo->send_cmd(command::adjust_parallax, +0.01f);
        break;
    case Qt::Key_Less:
    case Qt::Key_ZoomOut:
        _vo->send_cmd(command::adjust_zoom, -0.1f);
        break;
    case Qt::Key_Greater:
    case Qt::Key_ZoomIn:
        _vo->send_cmd(command::adjust_zoom, +0.1f);
        break;
    case Qt::Key_Slash:
#if QT_VERSION >= 0x040700
    case Qt::Key_VolumeDown:
#endif
        _vo->send_cmd(command::adjust_audio_volume, -0.05f);
        break;
    case Qt::Key_Asterisk:
#if QT_VERSION >= 0x040700
    case Qt::Key_VolumeUp:
#endif
        _vo->send_cmd(command::adjust_audio_volume, +0.05f);
        break;
    case Qt::Key_M:
#if QT_VERSION >= 0x040700
    case Qt::Key_VolumeMute:
#endif
        _vo->send_cmd(command::toggle_audio_mute);
        break;
    case Qt::Key_Left:
        _vo->send_cmd(command::seek, -10.0f);
        break;
    case Qt::Key_Right:
        _vo->send_cmd(command::seek, +10.0f);
        break;
    case Qt::Key_Down:
        _vo->send_cmd(command::seek, -60.0f);
        break;
    case Qt::Key_Up:
        _vo->send_cmd(command::seek, +60.0f);
        break;
    case Qt::Key_PageDown:
        _vo->send_cmd(command::seek, -600.0f);
        break;
    case Qt::Key_PageUp:
        _vo->send_cmd(command::seek, +600.0f);
        break;
    default:
        QGLWidget::keyPressEvent(event);
        break;
    }
}

void video_output_qt_widget::mouseReleaseEvent(QMouseEvent *event)
{
#if QT_VERSION < 0x050000
    _vo->mouse_set_pos(std::max(std::min(static_cast<float>(event->posF().x()) / this->width(), 1.0f), 0.0f));
#else
    _vo->mouse_set_pos(std::max(std::min(static_cast<float>(event->localPos().x()) / this->width(), 1.0f), 0.0f));
#endif
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

void video_container_widget::start_timer()
{
    _timer = new QTimer(this);
    connect(_timer, SIGNAL(timeout()), this, SLOT(playloop_step()));
    _timer->start(0);
}

void video_container_widget::playloop_step()
{
    try {
        dispatch::step();
        dispatch::process_all_events();
    }
    catch (std::exception& e) {
        send_cmd(command::close);
        QMessageBox::critical(this, _("Error"), e.what());
    }
}

void video_container_widget::receive_notification(const notification& note)
{
    if (note.type == notification::play && dispatch::playing()) {
        grab_focus();
    } else if (note.type == notification::quit && _timer) {
        QApplication::quit();
    }
}

void video_container_widget::set_recommended_size(int w, int h)
{
    _w = w;
    _h = h;
}

void video_container_widget::grab_focus()
{
    childAt(0, 0)->setFocus(Qt::OtherFocusReason);
}

QSize video_container_widget::sizeHint() const
{
    return QSize(_w, _h);
}

void video_container_widget::moveEvent(QMoveEvent*)
{
    send_cmd(command::update_display_pos);
}

void video_container_widget::closeEvent(QCloseEvent *)
{
    send_cmd(command::toggle_play);
}


/* The video_output_qt class */

video_output_qt::video_output_qt(video_container_widget *container_widget) :
    video_output(),
    _container_widget(container_widget),
    _container_is_external(container_widget != NULL),
    _widget(NULL),
    _fullscreen(false),
    _screensaver_inhibited(false),
    _recreate_context(false),
    _recreate_context_stereo(false)
{
    if (!_container_widget)
    {
        _container_widget = new video_container_widget(NULL);
        _container_widget->start_timer();
    }
    _format.setDoubleBuffer(true);
    _format.setSwapInterval(dispatch::parameters().swap_interval());
    _format.setStereo(false);
    // Save some constant values into member variables
    // so that we don't have to call Qt functions when the GL thread
    // calls our screen_*() functions.
    _screen_width = QApplication::desktop()->screenGeometry().width();
    _screen_height = QApplication::desktop()->screenGeometry().height();
    _screen_pixel_aspect_ratio =
        static_cast<float>(QApplication::desktop()->logicalDpiY())
        / static_cast<float>(QApplication::desktop()->logicalDpiX());
    if (std::fabs(_screen_pixel_aspect_ratio - 1.0f) < 0.03f) {
        // This screen most probably has square pixels, and the difference to 1.0
        // is only due to slightly inaccurate measurements and rounding. Force
        // 1.0 so that the user gets expected results.
        _screen_pixel_aspect_ratio = 1.0f;
    }
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
        glewExperimental = GL_TRUE;
        GLenum err = glewInit();
#if HAVE_X11
        if (err == GLEW_OK)
        {
            err = glxewInit();
        }
#endif
        if (err != GLEW_OK)
        {
            throw exc(str::asprintf(_("Cannot initialize GLEW: %s"),
                        reinterpret_cast<const char *>(glewGetErrorString(err))));
        }
        /* We essentially use OpenGL 2.1 + FBOs. Instead of checking for these two,
         * we check for GL 1.3 + a larger number of extensions, so that GL implementations
         * that do not fully support 2.1 still work. */
        if (!glewIsSupported("GL_VERSION_1_3 " // at least OpenGL 1.3 is reasonable
                    // GL_VERSION_2_0:
                    "GL_ARB_shader_objects GL_ARB_fragment_shader "
                    "GL_ARB_texture_non_power_of_two "
                    // GL_VERSION_2_1:
                    "GL_ARB_pixel_buffer_object "
                    // FBOs. Some drivers (e.g. GeForce 6600 on Mac) only provide the EXT
                    // version, though an ARB version exists
                    "GL_EXT_framebuffer_object"
                    // SRGB extensions.
                    // GL_EXT_texture_sRGB is optional (see video_output.cpp for the test)
                    // GL_ARB_framebuffer_sRGB is not needed (see video_output.cpp)
                    ))
        {
            throw exc(std::string(_("This OpenGL implementation does not support required features.")));
        }
        video_output::init();
        video_output::clear();
        // Initialize GL things
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        // Start rendering
        _widget->doneCurrent();
        _widget->start_rendering();
    }
}

int64_t video_output_qt::wait_for_subtitle_renderer()
{
    if (_subtitle_renderer.is_initialized())
    {
        return 0;
    }
    int64_t wait_start = timer::get(timer::monotonic);
    exc init_exception;
    QDialog *mbox = NULL;
    // Show a dialog only in GUI mode
    if (_container_is_external && !dispatch::parameters().fullscreen())
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
    int64_t wait_stop = timer::get(timer::monotonic);
    return (wait_stop - wait_start);
}

void video_output_qt::deinit()
{
    exit_fullscreen();
    if (_widget) {
        try {
            _widget->stop_rendering();
            _widget->makeCurrent();
            video_output::deinit();
        }
        catch (std::exception& e) {
            QMessageBox::critical(_widget, _("Error"), e.what());
        }
        delete _widget;
        _widget = NULL;
    }
}

void video_output_qt::create_widget()
{
    _widget = new video_output_qt_widget(this, _format, _container_widget);
    if (!_widget->context()->isValid())
    {
        QMessageBox::critical(_widget, _("Error"), _("Cannot get valid OpenGL context."));
        std::exit(1);
    }
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
    msg::dbg("OpenGL framebuffer: %d:%d:%d bits for R:G:B",
            _widget->format().redBufferSize(),
            _widget->format().greenBufferSize(),
            _widget->format().blueBufferSize());
    QGridLayout *container_layout = new QGridLayout();
    container_layout->addWidget(_widget, 0, 0);
    container_layout->setContentsMargins(0, 0, 0, 0);
    container_layout->setRowStretch(0, 1);
    container_layout->setColumnStretch(0, 1);
    delete _container_widget->layout();
    _container_widget->setLayout(container_layout);
    if (!_container_is_external)
        _container_widget->show();
    process_events();
}

#if HAVE_X11
GLXEWContext* video_output_qt::glxewGetContext() const
{
    return const_cast<GLXEWContext*>(&_glxew_context);
}
#endif

GLEWContext* video_output_qt::glewGetContext() const
{
    return const_cast<GLEWContext*>(&_glew_context);
}

bool video_output_qt::context_is_stereo() const
{
    return (_format.stereo());
}

void video_output_qt::recreate_context(bool stereo)
{
    // This was called from the GL thread (from inside video_output).
    // The request will be handled the next time process_events() is run,
    // because it involves destroying the current GL context and thread.
    _recreate_context = true;
    _recreate_context_stereo = stereo;
}

void video_output_qt::trigger_resize(int w, int h)
{
    _container_widget->set_recommended_size(w, h);
    _container_widget->updateGeometry();
    // Process events to propagate the information that the geometry needs updating.
    process_events();
    if (!_container_is_external)
        _container_widget->adjustSize();
}

void video_output_qt::mouse_set_pos(float dest)
{
    if (dispatch::parameters().fullscreen() || _container_is_external)
    {
        // Disabled in fullscreen and GUI mode
        return;
    }
    if (dispatch::playing())
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
    if (dispatch::playing())
    {
        send_cmd(command::toggle_fullscreen);
    }
}

void video_output_qt::suspend_screensaver()
{
#if defined(Q_OS_WIN)
    /* TODO */
#elif defined(Q_OS_MAC)
# if defined (MAC_OS_X_VERSION_10_6) && MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6
    // API introduced in 10.6
    IOPMAssertionCreateWithName(kIOPMAssertionTypeNoDisplaySleep, kIOPMAssertionLevelOn, CFSTR ("Bino"), &_disableDisplaySleepAssertion);
# else
    // API introduced in OSX 10.5, deprecated in 10.6
    IOPMAssertionCreate(kIOPMAssertionTypeNoDisplaySleep, kIOPMAssertionLevelOn, &_disableDisplaySleepAssertion);
# endif
#elif defined(Q_OS_UNIX)
    _widget->stop_rendering();
    if (QProcess::execute(QString("xdg-screensaver suspend ")
                + str::from(_container_widget->winId()).c_str()) != 0)
        msg::wrn(_("Cannot suspend screensaver."));
    _widget->start_rendering();
#endif
}

void video_output_qt::resume_screensaver()
{
#if defined(Q_OS_WIN)
    /* TODO */
#elif defined(Q_OS_MAC)
    IOPMAssertionRelease(_disableDisplaySleepAssertion);
#elif defined(Q_OS_UNIX)
    _widget->stop_rendering();
    if (QProcess::execute(QString("xdg-screensaver resume ")
                + str::from(_container_widget->winId()).c_str()) != 0)
        msg::wrn(_("Cannot resume screensaver."));
    _widget->start_rendering();
#endif
}

bool video_output_qt::supports_stereo() const
{
#if QT_VERSION >= 0x050700
    /* the test below does not seem to work with Qt 5.7; for now
     * disable stereo support for that version */
    return false;
#else
    QGLFormat fmt = _format;
    fmt.setStereo(true);
    QGLWidget *tmpwidget = new QGLWidget(fmt);
    bool ret = tmpwidget->format().stereo();
    delete tmpwidget;
    return ret;
#endif
}

int video_output_qt::screen_width() const
{
    return _screen_width;
}

int video_output_qt::screen_height() const
{
    return _screen_height;
}

float video_output_qt::screen_pixel_aspect_ratio() const
{
    return _screen_pixel_aspect_ratio;
}

int video_output_qt::width() const
{
    return _widget->vo_width();
}

int video_output_qt::height() const
{
    return _widget->vo_height();
}

int video_output_qt::pos_x() const
{
    return _widget->vo_pos_x();
}

int video_output_qt::pos_y() const
{
    return _widget->vo_pos_y();
}

void video_output_qt::center()
{
    if (!dispatch::parameters().fullscreen())
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

void video_output_qt::enter_fullscreen()
{
    if (!_fullscreen) {
#ifdef Q_OS_MAC
        _widget->stop_rendering();
#endif
        // If the container is a window, we save its geometry here so that
        // we can restore it later.
        if (!_container_is_external)
            _geom = _container_widget->geometry();
        // If the container is not yet a window, but embedded in the main window,
        // we need to make it a window now.
        if (_container_is_external)
            _container_widget->setWindowFlags(Qt::Window);
        // Determine combined geometry of the chosen screens.
        int screens = dispatch::parameters().fullscreen_screens();
        int screen_count = 0;
        QRect geom;
        for (int i = 0; i < std::min(QApplication::desktop()->screenCount(), 16); i++) {
            if (screens & (1 << i)) {
                if (geom.isNull())
                    geom = QApplication::desktop()->screenGeometry(i);
                else
                    geom = geom.united(QApplication::desktop()->screenGeometry(i));
                screen_count++;
            }
        }
        if (geom.isNull()) {
            // Use default screen
            geom = QApplication::desktop()->screenGeometry(-1);
        }
        Qt::WindowFlags new_window_flags =
            _container_widget->windowFlags()
            | Qt::FramelessWindowHint
            | Qt::WindowStaysOnTopHint;
        // In the dual and multi screen cases we need to bypass the window manager
        // on X11 because Qt does not support _NET_WM_FULLSCREEN_MONITORS, and thus
        // the window manager would always restrict the fullscreen window to one screen.
        // Note: it may be better to set _NET_WM_FULLSCREEN_MONITORS ourselves, but that
        // would also require the window manager to support this extension...
        if (screen_count > 1)
            new_window_flags |= Qt::X11BypassWindowManagerHint;
        _container_widget->setWindowFlags(new_window_flags);
        _container_widget->setWindowState(_container_widget->windowState() | Qt::WindowFullScreen);
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
        /* In the hope that this crap is not necessary anymore with Qt5, we continue to test
         * for Q_WS_X11, which is never defined with Qt5. */
        if (new_window_flags & Qt::X11BypassWindowManagerHint) {
            QApplication::syncX();      // just for safety; not sure if it is necessary
            XSetInputFocus(QX11Info::display(), _container_widget->winId(), RevertToParent, CurrentTime);
            XFlush(QX11Info::display());
        }
#endif
        _container_widget->grab_focus();
        // Suspend the screensaver after going fullscreen, so that our window ID
        // represents the fullscreen window. We need to have the same ID for resume.
        if (dispatch::parameters().fullscreen_inhibit_screensaver()) {
            suspend_screensaver();
            _screensaver_inhibited = true;
        }
        _fullscreen = true;
#ifdef Q_OS_MAC
        _widget->start_rendering();
#endif
    }
}

void video_output_qt::exit_fullscreen()
{
    if (_fullscreen) {
#ifdef Q_OS_MAC
        _widget->stop_rendering();
#endif
        // Resume the screensaver before disabling fullscreen, so that our window ID
        // still represents the fullscreen window and was the same when suspending the screensaver.
        if (_screensaver_inhibited) {
            resume_screensaver();
            _screensaver_inhibited = false;
        }
        // Re-embed the container widget into the main window if necessary
        if (_container_is_external)
            _container_widget->setWindowFlags(Qt::Widget);
        _container_widget->setWindowFlags(_container_widget->windowFlags()
                & ~Qt::X11BypassWindowManagerHint
                & ~Qt::FramelessWindowHint
                & ~Qt::WindowStaysOnTopHint);
        _container_widget->setWindowState(_container_widget->windowState() & ~Qt::WindowFullScreen);
        _container_widget->setCursor(Qt::ArrowCursor);
        if (!_container_is_external)
            _container_widget->setGeometry(_geom);
        _container_widget->show();
        _container_widget->raise();
        _container_widget->grab_focus();
        _fullscreen = false;
#ifdef Q_OS_MAC
        _widget->start_rendering();
#endif
    }
}

void video_output_qt::prepare_next_frame(const video_frame &frame, const subtitle_box &subtitle)
{
    if (_widget)
        _widget->gl_thread()->prepare_next_frame(frame, subtitle);
}

void video_output_qt::activate_next_frame()
{
    if (_widget)
        _widget->gl_thread()->activate_next_frame();
}

int64_t video_output_qt::time_to_next_frame_presentation() const
{
    return (_widget ? _widget->gl_thread()->time_to_next_frame_presentation() : 0);
}

void video_output_qt::process_events()
{
    if (_recreate_context) {
        // To prevent recursion, we must unset this flag first: deinit() and init()
        // cause more calls to process_events().
        _widget->stop_rendering();
        _recreate_context = false;
        deinit();
        _format.setStereo(_recreate_context_stereo);
        init();
    }
    QApplication::sendPostedEvents();
    QApplication::processEvents();
}

void video_output_qt::receive_notification(const notification& note)
{
    /* Redisplay if a parameter was changed that affects the video display. */
    if (dispatch::playing()
            && (note.type == notification::quality
                || note.type == notification::stereo_mode
                || note.type == notification::stereo_mode_swap
                || note.type == notification::crosstalk
                || note.type == notification::fullscreen_flip_left
                || note.type == notification::fullscreen_flop_left
                || note.type == notification::fullscreen_flip_right
                || note.type == notification::fullscreen_flop_right
                || note.type == notification::fullscreen_3d_ready_sync
                || note.type == notification::contrast
                || note.type == notification::brightness
                || note.type == notification::hue
                || note.type == notification::saturation
                || note.type == notification::zoom
#if HAVE_LIBXNVCTRL
                || note.type == notification::sdi_output_format
                || note.type == notification::sdi_output_left_stereo_mode
                || note.type == notification::sdi_output_right_stereo_mode
#endif // HAVE_LIBXNVCTRL
                || note.type == notification::crop_aspect_ratio
                || note.type == notification::parallax
                || note.type == notification::ghostbust)) {
        _widget->redisplay();
    }
    /* Redisplay if the widget moved and a masking stereo mode is active. */
    if (dispatch::playing()
            && note.type == notification::display_pos
            && (dispatch::parameters().stereo_mode() == parameters::mode_even_odd_rows
                || dispatch::parameters().stereo_mode() == parameters::mode_even_odd_columns
                || dispatch::parameters().stereo_mode() == parameters::mode_checkerboard)) {
        _widget->redisplay();
    }
    if (note.type == notification::play && !dispatch::playing()) {
        exit_fullscreen();
    }
}
