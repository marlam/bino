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

#include <vector>

#include "exc.h"
#include "dbg.h"
#include "msg.h"
#include "timer.h"
#include "thread.h"

#include "controller.h"
#include "player.h"


// The single player instance
static player *global_player;

// The list of registered controllers
static std::vector<controller *> global_controllers;
static unsigned int global_controllers_version;
static mutex global_controllers_mutex;

// Note about thread safety:
// Only the constructor and destructor are thread safe (by using a lock on the
// global_controllers vector).  The visitation of controllers and the actions
// performed as a result are supposed to happen inside a single thread.

controller::controller() throw ()
{
    global_controllers_mutex.lock();
    global_controllers.push_back(this);
    global_controllers_version++;
    global_controllers_mutex.unlock();
}

controller::~controller()
{
    global_controllers_mutex.lock();
    for (size_t i = 0; i < global_controllers.size(); i++)
    {
        if (global_controllers[i] == this)
        {
            global_controllers.erase(global_controllers.begin() + i);
            break;
        }
    }
    global_controllers_version++;
    global_controllers_mutex.unlock();
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

void controller::process_events()
{
    /* default: do nothing */
}

void *controller::get_global_player()
{
    return global_player;
}

void controller::set_global_player(void *p)
{
    assert(!p || !global_player);       // there can be at most one global player
    global_player = static_cast<player *>(p);
}

void controller::visit_all_controllers(int action, const notification &note)
{
    std::vector<controller *> visited_controllers;
    visited_controllers.reserve(global_controllers.size());

    // First, try to visit all controllers in one row without extra checks. This
    // only works as long as controllers do not vanish or appear as a result of
    // the function we call. This is the common case.
    unsigned int controllers_version = global_controllers_version;
    for (size_t i = 0; i < global_controllers.size(); i++)
    {
        controller *c = global_controllers[i];
        if (action == 0)
        {
            c->process_events();
        }
        else
        {
            c->receive_notification(note);
        }
        visited_controllers.push_back(c);
        if (controllers_version != global_controllers_version)
        {
            break;
        }
    }
    // If some controllers vanished or appeared, we need to redo the loop and
    // check for each controller if it was visited before. This is costly, but
    // it happens seldomly.
    while (controllers_version != global_controllers_version)
    {
        controllers_version = global_controllers_version;
        for (size_t i = 0; i < global_controllers.size(); i++)
        {
            controller *c = global_controllers[i];
            bool visited = false;
            for (size_t j = 0; j < visited_controllers.size(); j++)
            {
                if (visited_controllers[j] == c)
                {
                    visited = true;
                    break;
                }
            }
            if (!visited)
            {
                if (action == 0)
                {
                    c->process_events();
                }
                else
                {
                    c->receive_notification(note);
                }
                visited_controllers.push_back(c);
                if (controllers_version != global_controllers_version)
                {
                    break;
                }
            }
        }
    }
}

void controller::process_all_events()
{
    visit_all_controllers(0, notification(notification::play));
}

void controller::notify_all(const notification &note)
{
    visit_all_controllers(1, note);
}
