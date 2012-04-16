/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010, 2011, 2012
 * Martin Lambers <marlam@marlam.de>
 * Stefan Eilemann <eile@eyescale.ch>
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

#ifndef PLAYER_EQUALIZER_H
#define PLAYER_EQUALIZER_H

#include <vector>
#include <string>

#include "player.h"

class eq_node_factory;
class eq_config;

class player_equalizer : public player
{
private:
    eq_node_factory* _node_factory;
    eq_config* _config;
    bool _flat_screen;

public:
    player_equalizer(int* argc, char* argv[], bool flat_screen);
    virtual ~player_equalizer();

    virtual void open();

    static void mainloop();
};

#endif
