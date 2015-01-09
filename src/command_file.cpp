/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2012, 2015
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

#include <cstring>
#include <cerrno>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "base/exc.h"
#include "base/str.h"
#include "base/tmr.h"

#include "base/gettext.h"
#define _(string) gettext(string)

#include "command_file.h"

/* Windows portability. */
#ifndef O_NONBLOCK
# define O_NONBLOCK 0
#endif
#ifndef EWOULDBLOCK
# define EWOULDBLOCK EAGAIN
#endif


command_file::command_file(const std::string& filename) :
    _filename(filename), _fd(-1)
{
}

command_file::~command_file()
{
    deinit();
}

void command_file::init()
{
    if (_fd < 0) {
        _fd = ::open(_filename.c_str(), O_RDONLY | O_NONBLOCK);
        if (_fd < 0) {
            throw exc(str::asprintf(_("%s: %s"), _filename.c_str(), ::strerror(errno)), errno);
        }
        _is_fifo = false;
        struct stat statbuf;
        if (fstat(_fd, &statbuf) == 0 && S_ISFIFO(statbuf.st_mode))
            _is_fifo = true;
        _linebuf.clear();
        _wait_until_stop = false;
        _wait_until = -1;
    }
}

void command_file::deinit()
{
    if (_fd >= 0) {
        if (::close(_fd) != 0) {
            msg::wrn(_("%s: %s"), _filename.c_str(), ::strerror(errno));
        }
        _fd = -1;
    }
}

bool command_file::is_active() const
{
    return (_fd >= 0 || _linebuf.size() > 0);
}

bool command_file::allow_early_quit()
{
    return !is_active();
}

void command_file::process_events()
{
    if (!is_active())
        return;

    if (_wait_until_stop && dispatch::playing())
        return;

    if (_wait_until >= 0) {
        int64_t now = timer::get(timer::monotonic);
        if (now < _wait_until)
            return;
    }

    _wait_until_stop = false;
    _wait_until = -1;

    static const size_t readbuf_size = 512;
    static char readbuf[readbuf_size];

    while (_fd >= 0) {
        ssize_t r = ::read(_fd, readbuf, readbuf_size);
        if (r < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // no data to read right now; we try again later
                break;
            } else {
                // an error occured
                int errno_bak = errno;
                deinit();
                throw exc(str::asprintf(_("%s: %s"), _filename.c_str(), ::strerror(errno_bak)), errno_bak);
            }
        } else if (r == 0) {
            // EOF. Keep a FIFO open: a writer may appear later. Close other files.
            if (!_is_fifo)
                deinit();
            break;
        } else {
            // We have some data
            _linebuf.append(readbuf, r);
        }
    }
    std::string cmd;
    if (_linebuf.size() > 0) {
        size_t eol_pos = _linebuf.find_first_of('\n');
        if (eol_pos != std::string::npos) {
            // We have a complete command line. Execute the command,
            // and remember the rest of the buffer (if any) for a subsequent
            // call to this function (we execute at most one command
            // per process_events() call).
            cmd = _linebuf.substr(0, eol_pos);
            _linebuf.erase(0, eol_pos + 1);
        } else if (_fd < 0) {
            // We have a last line without EOL. Process it.
            cmd = _linebuf;
            _linebuf.clear();
        }
    }
    if (cmd.length() > 0) {
        cmd = str::sanitize(str::trim(cmd));
        if (cmd.substr(0, 4) == "wait") {
            // This command is specific to this particular controller!
            std::vector<std::string> tokens = str::tokens(cmd, " \t\r");
            if (tokens.size() == 2 && tokens[0] == "wait") {
                float seconds;
                if (tokens[1] == "stop") {
                    _wait_until_stop = true;
                    return;
                } else if (str::to(tokens[1], &seconds)) {
                    _wait_until = timer::get(timer::monotonic);
                    if (seconds > 0.0f)
                        _wait_until += seconds * 1e6f;
                    return;
                }
            }
        }
        command c;
        bool ok = dispatch::parse_command(cmd, &c);
        if (!ok) {
            msg::err(_("%s: invalid command '%s'"), _filename.c_str(), cmd.c_str());
        } else {
            send_cmd(c);
        }
    }
}
