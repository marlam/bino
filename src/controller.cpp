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

#include <vector>

#include "exc.h"
#include "msg.h"
#include "timer.h"

#include "controller.h"
#include "player.h"


// The single player instance
extern player *global_player;
extern std::vector<controller *> global_controllers;

controller::controller(bool receive_notifications) throw ()
{
    if (receive_notifications)
    {
        global_controllers.push_back(this);
    }
}

controller::~controller()
{
    for (size_t i = 0; i < global_controllers.size(); i++)
    {
        if (global_controllers[i] == this)
        {
            global_controllers.erase(global_controllers.begin() + i);
            break;
        }
    }
}

void controller::send_cmd(const command &cmd)
{
    if (global_player)
    {
        global_player->receive_cmd(cmd);
    }
}

void controller::receive_notification(const notification &)
{
    /* default: ignore the notification */
}
