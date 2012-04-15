/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2012
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

#ifndef COMMAND_FILE_H
#define COMMAND_FILE_H

#include <string>

#include "dispatch.h"


class command_file : public controller
{
private:
    const std::string _filename;
    int _fd;
    bool _is_fifo;
    std::string _linebuf;
    bool _wait_until_stop;
    int64_t _wait_until;

    // Helper function to read a command from a string
    bool get_command(const std::string &s, command &c);

public:
    command_file(const std::string& filename);
    ~command_file();

    /* Initialize. Throw an exception if this fails. */
    void init();
    /* Deinitialize. */
    void deinit();

    bool is_active() const;

    /* Process events (see if there's any input on the file). */
    virtual void process_events();
    /* Prevent the application from quitting if there might be
     * an additional 'open' command in the future. */
    virtual bool allow_early_quit();
};

#endif
