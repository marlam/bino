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

#ifndef QT_APP_H
#define QT_APP_H

#include <vector>
#include <string>

/* Initialize Qt. If init_qt() returns true, then the caller is responsible for
 * calling exit_qt() later. If it returns false, then Qt was already
 * initialized elsewhere and the caller must not call exit_qt(). */
bool init_qt();
void exit_qt();
/* Run the Qt application */
int exec_qt();

std::vector<std::string> qt_versions();

#endif
