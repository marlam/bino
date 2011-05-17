/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2011
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

#ifndef LIRC_H
#define LIRC_H

#include <string>
#include <vector>

#include <lirc/lirc_client.h>

#include "controller.h"


class lircclient : public controller
{
private:
    const std::string _client_name;
    const std::vector<std::string> _conf_files;
    bool _initialized;
    int _socket;
    struct lirc_config *_config;

    // Current player state; required for some commands
    bool _playing;
    bool _pausing;

    // Helper function to read a command from a string
    bool get_command(const std::string &s, command &c);

public:
    lircclient(const std::string &client_name, const std::vector<std::string> &conf_files);
    ~lircclient();
    
    /* Initialize LIRC. Throw an exception if this fails. */
    void init();
    /* Deinitialize. */
    void deinit();

    /* Receive notifications to keep track of required state. */
    virtual void receive_notification(const notification &note);

    /* Process LIRC events. */
    virtual void process_events();
};

#endif
