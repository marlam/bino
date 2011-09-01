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

#include <cerrno>
#ifndef HAVE_CLOCK_GETTIME
# include <sys/time.h>
#endif

#include "gettext.h"
#define _(string) gettext(string)

#include "exc.h"
#include "timer.h"


int64_t timer::get_microseconds(timer::type t)
{
#ifdef HAVE_CLOCK_GETTIME

    struct timespec time;
    int r;
    if (t == realtime)
    {
        r = clock_gettime(CLOCK_REALTIME, &time);
    }
    else if (t == monotonic)
    {
        r = clock_gettime(CLOCK_MONOTONIC, &time);
    }
    else if (t == process_cpu)
    {
#ifdef CLOCK_PROCESS_CPUTIME_ID
        r = clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time);
#else
        r = -1;
        errno = ENOSYS;
#endif
    }
    else // t == thread_cpu
    {
#ifdef CLOCK_THREAD_CPUTIME_ID
        r = clock_gettime(CLOCK_THREAD_CPUTIME_ID, &time);
#else
        r = -1;
        errno = ENOSYS;
#endif
    }
    if (r != 0)
    {
        throw exc(_("Cannot get time."), errno);
    }
    return static_cast<int64_t>(time.tv_sec) * 1000000 + time.tv_nsec / 1000;

#else

    if (t == realtime || t == monotonic)
    {
        struct timeval tv;
        int r = gettimeofday(&tv, NULL);
        if (r != 0)
        {
            throw exc(_("Cannot get time."), errno);
        }
        return static_cast<int64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
    }
    else if (t == process_cpu)
    {
        // In W32, clock() starts with zero on program start, so we do not need
        // to subtract a start value.
        // XXX: Is this also true on Mac OS X?
        clock_t c = clock();
        return (c * static_cast<int64_t>(1000000) / CLOCKS_PER_SEC);
    }
    else
    {
        throw exc(_("Cannot get time."), ENOSYS);
    }

#endif
}
