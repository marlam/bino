/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2015
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

#include "base/msg.h"
#include "base/exc.h"

#include "base/gettext.h"
#define _(string) gettext(string)


const char *exc::_fallback_str = strerror(ENOMEM);

exc::exc() throw ()
    : _fallback(false), _str(), _sys_errno(0)
{
}

exc::exc(const std::string &what, int sys_errno) throw ()
    : _fallback(false), _sys_errno(sys_errno)
{
    try
    {
        _str = what;
    }
    catch (...)
    {
        _fallback = true;
        _sys_errno = ENOMEM;
    }
    if (!empty())
    {
        msg::dbg_txt(_("Exception: %s"), _str.c_str());
    }
}

exc::exc(int sys_errno) throw ()
    : _fallback(false), _sys_errno(sys_errno)
{
    try
    {
        _str = strerror(_sys_errno);
    }
    catch (...)
    {
        _fallback = true;
        _sys_errno = ENOMEM;
    }
    if (!empty())
    {
        msg::dbg_txt(_("Exception: %s"), _str.c_str());
    }
}

exc::exc(const exc &e) throw ()
    : _fallback(e._fallback), _sys_errno(e._sys_errno)
{
    try
    {
        _str = e._str;
    }
    catch (...)
    {
        _fallback = true;
        _sys_errno = ENOMEM;
    }
}

exc::exc(const std::exception &e) throw ()
    : _fallback(false), _sys_errno(0)
{
    // TODO: Avoid the crappy what() strings; ideally translate them to errnos.
    // E.g. std::bad_alloc -> ENOMEM.
    try
    {
        _str = e.what();
    }
    catch (...)
    {
        _fallback = true;
        _sys_errno = ENOMEM;
    }
    if (!empty())
    {
        msg::dbg_txt(_("Exception: %s"), _str.c_str());
    }
}

exc::~exc() throw ()
{
}

bool exc::empty() const throw ()
{
    return (_str.length() == 0 && _sys_errno == 0 && !_fallback);
}

int exc::sys_errno() const throw ()
{
    return _sys_errno;
}

const char *exc::what() const throw ()
{
    return (_fallback ? _fallback_str : _str.c_str());
}
