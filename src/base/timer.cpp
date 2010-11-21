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

#include <cerrno>
#ifndef HAVE_CLOCK_GETTIME
# include <sys/time.h>
#endif
#ifdef HAVE_QUERYPERFORMANCECOUNTER
# include <windows.h>
#endif

#include "exc.h"
#include "timer.h"


int64_t timer::get_microseconds(timer::type t)
{
#ifdef HAVE_CLOCK_GETTIME

    struct timespec time;
    int r = clock_gettime(
              t == realtime ? CLOCK_REALTIME
            : t == monotonic ? 
# ifdef CLOCK_MONOTONIC_RAW
                               CLOCK_MONOTONIC_RAW
# else
                               CLOCK_MONOTONIC
# endif
            : t == process_cpu ? CLOCK_PROCESS_CPUTIME_ID
            : CLOCK_THREAD_CPUTIME_ID, &time);
    if (r != 0)
    {
        throw exc(std::string("cannot get ")
                + std::string(
                      t == realtime ? "real"
                    : t == monotonic ? "monotonic"
                    : t == process_cpu ? "process CPU"
                    : "thread CPU")
                + std::string(" time"), errno);
    }
    return static_cast<int64_t>(time.tv_sec) * 1000000 + time.tv_nsec / 1000;

#else

    if (t == realtime
# ifndef HAVE_QUERYPERFORMANCECOUNTER
            || t == monotonic)
# endif
    {
        struct timeval tv;
        int r = gettimeofday(&tv, NULL);
        if (r != 0)
        {
            throw exc(std::string("cannot get ")
                    + std::string(t == realtime ? "real" : "monotonic")
                    + std::string(" time"), errno);
        }
        return static_cast<int64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
    }
# ifdef HAVE_QUERYPERFORMANCECOUNTER
    else if (t == monotonic)
    {
        LARGE_INTEGER now, frequency;
        QueryPerformanceCounter(&now);
        QueryPerformanceFrequency(&frequency);
        return (now.QuadPart * 1000000) / frequency.QuadPart;
    }
# endif
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
        throw exc(std::string("cannot get ")
                + std::string("thread CPU")
                + std::string(" time"), ENOSYS);
    }

#endif
}
