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

#include <QApplication>


static int qt_argc = 1;
static const char *qt_argv[2] = { PACKAGE_NAME, NULL };
static QApplication *qt_app = NULL;

bool init_qt()
{
    if (!qt_app)
    {
        qt_app = new QApplication(qt_argc, const_cast<char **>(qt_argv));
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
