/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010, 2011, 2012
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

#include "gettext.h"
#define _(string) gettext(string)

#include "exc.h"
#include "msg.h"
#include "str.h"
#include "dbg.h"
#include "timer.h"

#include "video_output_qt.h"
#include "lib_versions.h"


/* The GL thread */

gl_thread::gl_thread(video_output_qt* vo_qt, video_output_qt_widget* vo_qt_widget) :
    _vo_qt(vo_qt), _vo_qt_widget(vo_qt_widget),
    _render(false),
    _activate_next_frame(false),
    _resize(false),
    _prepare_next_frame(false),
    _have_prepared_frame(false),
    _failure(false)
{
    for (int i = 0; i < 2; i++) {
        for (int p = 0; p < 3; p++) {
            _next_data[i][p] = NULL;
            _next_line_size[i][p] = 0;
        }
    }
}

gl_thread::~gl_thread()
{
    for (int i = 0; i < 2; i++) {
        for (int p = 0; p < 3; p++) {
            std::free(_next_data[i][p]);
        }
    }
}

void gl_thread::set_render(bool r)
{
    _render = r;
    _pti = 0;
    _ptc = 0;
}

void gl_thread::activate_next_frame()
{
    _activate_next_frame = true;
}

void gl_thread::resize(int w, int h)
{
    _w = w;
    _h = h;
    _resize = true;
}

void gl_thread::prepare_next_frame(const video_frame &frame, const subtitle_box &subtitle)
{
    _prepare_next_mutex.lock();
    _next_subtitle = subtitle;
    _next_frame = frame;
    // Copy the data because the decoder can overwrite the buffers at any time
    try {
        if (_next_frame.is_valid()) {
            for (int i = 0; i < 2; i++) {
                for (int p = 0; p < 3; p++) {
                    size_t height = _next_frame.raw_height;
                    if (_next_frame.layout == video_frame::yuv420p && p > 0)
                        height /= 2;
                    size_t data_size = height * _next_frame.line_size[i][p];
                    if (_next_line_size[i][p] != _next_frame.line_size[i][p]) {
                        _next_data[i][p] = std::realloc(_next_data[i][p], data_size);
                        if (data_size > 0 && !_next_data[i][p]) {
                            throw exc(ENOMEM);
                        }
                        _next_line_size[i][p] = _next_frame.line_size[i][p];
                    }
                    if (data_size > 0) {
                        std::memcpy(_next_data[i][p], _next_frame.data[i][p], data_size);
                    }
                    _next_frame.data[i][p] = _next_data[i][p];
                }
            }
        }
    }
    catch (std::exception& e) {
        _e = e;
        _render = false;
        _failure = true;
    }
    _prepare_next_mutex.unlock();
    _prepare_next_frame = true;
}

void gl_thread::run()
{
    try {
        while (_render) {
            assert(_vo_qt_widget->context()->isValid());
            _vo_qt_widget->makeCurrent();
            if (QGLContext::currentContext() == _vo_qt_widget->context()) {
                if (_activate_next_frame && _have_prepared_frame) {
                    _vo_qt->video_output::activate_next_frame();
                    _have_prepared_frame = false;
                    _activate_next_frame = false;
                }
                if (_resize) {
                    _vo_qt->reshape(_w, _h);
                    _resize = false;
                }
                _vo_qt->display_current_frame();
                if (_prepare_next_frame) {
                    _prepare_next_mutex.lock();
                    _vo_qt->video_output::prepare_next_frame(_next_frame, _next_subtitle);
                    _prepare_next_mutex.unlock();
                    _prepare_next_frame = false;
                    _have_prepared_frame = true;
                }
            }
            _vo_qt_widget->swapBuffers();
            // When the buffer swap returns, the current frame is just now presented on screen.
            _pt_mutex.lock();
            _pt[_pti] = timer::get_microseconds(timer::monotonic);
            _pti++;
            if (_pti >= _pts)
                _pti = 0;
            if (_ptc < _pts)
                _ptc++;
            _pt_mutex.unlock();
        }
    }
    catch (std::exception& e) {
        _e = e;
        _render = false;
        _failure = true;
    }
}

int64_t gl_thread::time_to_next_frame_presentation()
{
    if (_ptc < _pts)    // no reliable data yet; assume immediate display
        return 0;
    _pt_mutex.lock();
    int last_pti = _pti == 0 ? _pts - 1 : _pti - 1;
    int64_t last_pt = _pt[last_pti];
    int64_t presentation_duration = 0;
    for (int i = 0; i < _pts - 1; i++) {
        int cpti = _pti - i - 1;
        if (cpti < 0)
            cpti += _pts;
        int ppti = cpti - 1;
        if (ppti < 0)
            ppti = _pts - 1;
        assert(_pt[cpti] - _pt[ppti] >= 0);
        presentation_duration += _pt[cpti] - _pt[ppti];
    }
    presentation_duration /= _pts - 1;
    _pt_mutex.unlock();
    int64_t now = timer::get_microseconds(timer::monotonic);
    return last_pt + presentation_duration - now;
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
    QApplication::syncX();
    _gl_thread.set_render(true);
    _gl_thread.start();
    _timer.start(0);
}

void video_output_qt_widget::stop_rendering()
{
    _gl_thread.set_render(false);
    _gl_thread.wait();
    _timer.stop();
    QApplication::syncX();
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
        QMessageBox::critical(this, "Error", e.what());
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
    int64_t wait_start = timer::get_microseconds(timer::monotonic);
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
    int64_t wait_stop = timer::get_microseconds(timer::monotonic);
    return (wait_stop - wait_start);
}

void video_output_qt::deinit()
{
    if (_widget)
    {
        _widget->stop_rendering();
        _widget->makeCurrent();
        video_output::deinit();
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
    _container_widget->updateGeometry();
    _widget->show();
    _container_widget->show();
    _container_widget->raise();
    process_events();
}

GLEWContext* video_output_qt::glewGetContext() const
{
    return const_cast<GLEWContext*>(&_glew_context);
}

void video_output_qt::make_context_current()
{
    _widget->makeCurrent();
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

bool video_output_qt::supports_stereo() const
{
    QGLFormat fmt = _format;
    fmt.setStereo(true);
    QGLWidget *tmpwidget = new QGLWidget(fmt);
    bool ret = tmpwidget->format().stereo();
    delete tmpwidget;
    return ret;
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
    if (!_fullscreen)
    {
        _widget->stop_rendering();
        int screens = dispatch::parameters().fullscreen_screens();
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
        _container_widget->grab_focus();
        // Suspend the screensaver after going fullscreen, so that our window ID
        // represents the fullscreen window. We need to have the same ID for resume.
        if (dispatch::parameters().fullscreen_inhibit_screensaver()) {
            suspend_screensaver();
            _screensaver_inhibited = true;
        }
        _fullscreen = true;
        _widget->start_rendering();
    }
}

void video_output_qt::exit_fullscreen()
{
    if (_fullscreen)
    {
        _widget->stop_rendering();
        // Resume the screensaver before disabling fullscreen, so that our window ID
        // still represents the fullscreen window and was the same when suspending the screensaver.
        if (_screensaver_inhibited) {
            resume_screensaver();
            _screensaver_inhibited = false;
        }
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
        _container_widget->raise();
        _container_widget->grab_focus();
        _fullscreen = false;
        _widget->start_rendering();
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
    if (note.type == notification::play && !dispatch::playing()) {
        exit_fullscreen();
    }
}
