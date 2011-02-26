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

#include "config.h"

#include <cstdlib>

#include <QCoreApplication>
#include <QApplication>
#include <QtGlobal>

#include "qt_app.h"

#include "msg.h"


static int qt_argc = 0;
static char **qt_argv = NULL;
static QApplication *qt_app = NULL;

void set_qt_argv(int argc, char *argv[])
{
    qt_argc = argc;
    qt_argv = argv;
}

static void qt_msg_handler(QtMsgType type, const char *msg)
{
    switch (type)
    {
    case QtDebugMsg:
        msg::dbg("%s", msg);
        break;
    case QtWarningMsg:
        msg::wrn("%s", msg);
        break;
    case QtCriticalMsg:
        msg::err("%s", msg);
        break;
    case QtFatalMsg:
        msg::err("%s", msg);
        std::abort();
    }
}

bool init_qt()
{
    if (!qt_app)
    {
        qInstallMsgHandler(qt_msg_handler);
        qt_app = new QApplication(qt_argc, const_cast<char **>(qt_argv));
        QCoreApplication::setOrganizationName("Bino");
        QCoreApplication::setOrganizationDomain("bino.nongnu.org");
        QCoreApplication::setApplicationName(PACKAGE_NAME);
        return true;
    }
    else
    {
        return false;
    }
}

void exit_qt()
{
    delete qt_app;
    qt_app = NULL;
}

int exec_qt()
{
    return qt_app->exec();
}
