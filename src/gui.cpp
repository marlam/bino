/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010, 2011, 2012, 2013, 2014, 2018
 * Martin Lambers <marlam@marlam.de>
 * Frédéric Devernay <Frederic.Devernay@inrialpes.fr>
 * Joe <cuchac@email.cz>
 * Daniel Schaal <farbing@web.de>
 * D. Matz <bandregent@yahoo.de>
 * Binocle <http://binocle.com> (author: Olivier Letz <oletz@binocle.com>)
 * Frédéric Bour <frederic.bour@lakaban.net>
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

#include <QCoreApplication>
#include <QFile>
#include <QSettings>

#include "gui.h"

#if HAVE_LIBXNVCTRL
#  include <NVCtrl/NVCtrl.h>
#  include "NvSDIutils.h"
#endif // HAVE_LIBXNVCTRL

// for autoconf < 2.65:
#ifndef PACKAGE_URL
#  define PACKAGE_URL "https://bino3d.org"
#endif

gui::gui()
{
    QCoreApplication::setOrganizationName(PACKAGE_NAME);
    QCoreApplication::setApplicationName(PACKAGE_NAME);
    _settings = new QSettings;
    _main_window = new main_window(_settings);
}

gui::~gui()
{
    delete _main_window;
    delete _settings;
}

void gui::open(const open_input_data& input_data)
{
     if (input_data.urls.size() > 0) {
         QStringList urls;
         for (size_t i = 0; i < input_data.urls.size(); i++)
             urls.push_back(QFile::decodeName(input_data.urls[i].c_str()));
         parameters initial_params = input_data.params;
         if (dispatch::parameters().stereo_mode_is_set())
             initial_params.set_stereo_mode(dispatch::parameters().stereo_mode());
         if (dispatch::parameters().stereo_mode_swap_is_set())
             initial_params.set_stereo_mode_swap(dispatch::parameters().stereo_mode_swap());
         _main_window->open(urls, input_data.dev_request, initial_params);
     }
}
