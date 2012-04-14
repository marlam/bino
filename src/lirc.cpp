/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2011, 2012
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
#include <cstdio>
#include <unistd.h>
#include <fcntl.h>

#include "gettext.h"
#define _(string) gettext(string)

#include "exc.h"
#include "str.h"
#include "msg.h"

#include "lirc.h"


lircclient::lircclient(
        const std::string &client_name,
        const std::vector<std::string> &conf_files) :
    controller(),
    _client_name(client_name),
    _conf_files(conf_files),
    _initialized(false)
{
}

lircclient::~lircclient()
{
    deinit();
}

void lircclient::init()
{
    if (!_initialized)
    {
        int socket_flags;
        _socket = lirc_init(const_cast<char *>(_client_name.c_str()), msg::level() == msg::DBG ? 1 : 0);
        if (_socket == -1)
        {
            throw exc(_("Cannot initialize LIRC."));
        }
        if ((socket_flags = fcntl(_socket, F_GETFL)) < 0
                || fcntl(_socket, F_SETFL, socket_flags | O_NONBLOCK) < 0)
        {
            lirc_deinit();
            throw exc(_("Cannot set LIRC socket properties."));
        }
        if (_conf_files.size() == 0)
        {
            if (lirc_readconfig(NULL, &_config, NULL) != 0)
            {
                lirc_deinit();
                throw exc(_("Cannot read LIRC default configuration."));
            }
        }
        else
        {
            _config = NULL;
            for (size_t i = 0; i < _conf_files.size(); i++)
            {
                if (lirc_readconfig(const_cast<char *>(_conf_files[i].c_str()), &_config, NULL) != 0)
                {
                    if (_config)
                    {
                        lirc_freeconfig(_config);
                    }
                    lirc_deinit();
                    throw exc(str::asprintf(_("Cannot read LIRC configuration file %s."), _conf_files[i].c_str()));
                }
            }
        }
        _initialized = true;
    }
}

void lircclient::deinit()
{
    if (_initialized)
    {
        lirc_freeconfig(_config);
        lirc_deinit();
        _initialized = false;
    }
}

void lircclient::process_events()
{
    if (_initialized)
    {
        char *code;
        char *cmd;
        int e;

        if ((e = lirc_nextcode(&code)) != 0)
        {
            msg::err(_("Cannot get LIRC event; disabling LIRC support."));
            deinit();
            return;
        }
        if (!code)
        {
            /* No event currently available */
            return;
        }
        while ((e = lirc_code2char(_config, code, &cmd)) == 0 && cmd)
        {
            command c;
            if (!dispatch::parse_command(cmd, &c))
            {
                msg::err(str::asprintf(_("Received invalid command '%s' from LIRC."), str::sanitize(cmd).c_str()));
            }
            else
            {
                send_cmd(c);
            }
        }
        std::free(code);
        if (e != 0)
        {
            msg::wrn(_("Cannot get command for LIRC code."));
        }
    }
}
