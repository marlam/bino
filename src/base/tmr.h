/*
 * Copyright (C) 2010, 2011, 2012, 2013
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

/**
 * \file timer.h
 *
 * Timer.
 */

#ifndef TMR_H
#define TMR_H

class timer
{
public:

    /* All timers are always in microseconds. */

    enum type {
        realtime,
        monotonic,
        process_cpu,
        thread_cpu
    };

    static long long get(type type);

    static float to_seconds(long long microseconds)
    {
        return microseconds / 1e6f;
    }
};

#endif
