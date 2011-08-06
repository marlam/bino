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

#include "config.h"

#include <cstring>
#include <pthread.h>

#include "gettext.h"
#define _(string) gettext(string)

#include "dbg.h"
#include "thread.h"


const pthread_mutex_t mutex::_mutex_initializer = PTHREAD_MUTEX_INITIALIZER;

mutex::mutex() : _mutex(_mutex_initializer)
{
    int e = pthread_mutex_init(&_mutex, NULL);
    if (e != 0)
    {
        throw exc(str::asprintf(_("Cannot initialize mutex: %s"), std::strerror(e)), e);
    }
}

mutex::mutex(const mutex &) : _mutex(_mutex_initializer)
{
    // You cannot have multiple copies of the same mutex.
    // Instead, we create a new one. This allows easier use of mutexes in STL containers.
    int e = pthread_mutex_init(&_mutex, NULL);
    if (e != 0)
    {
        throw exc(str::asprintf(_("Cannot initialize mutex: %s"), std::strerror(e)), e);
    }
}

mutex::~mutex()
{
    (void)pthread_mutex_destroy(&_mutex);
}

void mutex::lock()
{
    int e = pthread_mutex_lock(&_mutex);
    if (e != 0)
    {
        throw exc(str::asprintf(_("Cannot lock mutex: %s"), std::strerror(e)), e);
    }
}

bool mutex::trylock()
{
    return (pthread_mutex_trylock(&_mutex) == 0);
}

void mutex::unlock()
{
    int e = pthread_mutex_unlock(&_mutex);
    if (e != 0)
    {
        throw exc(str::asprintf(_("Cannot unlock mutex: %s"), std::strerror(e)), e);
    }
}


thread::thread() :
    __thread_id(pthread_self()),
    __joinable(false),
    __running(false),
    __wait_mutex(),
    __exception()
{
}

thread::thread(const thread &) :
    __thread_id(pthread_self()),
    __joinable(false),
    __running(false),
    __wait_mutex(),
    __exception()
{
    // The thread state cannot be copied; a new state is created instead.
}

thread::~thread()
{
    if (__joinable)
    {
        (void)pthread_detach(__thread_id);
    }
}

void *thread::__run(void *p)
{
    thread *t = static_cast<thread *>(p);
    try
    {
        t->run();
    }
    catch (exc &e)
    {
        t->__exception = e;
    }
    catch (std::exception &e)
    {
        t->__exception = e;
    }
    catch (...)
    {
        // We must assume this means thread cancellation. In this
        // case, we *must* rethrow the exception.
        t->__running = false;
        throw;
    }
    t->__running = false;
    return NULL;
}

void thread::start()
{
    if (atomic::bool_compare_and_swap(&__running, false, true))
    {
        wait();
        int e = pthread_create(&__thread_id, NULL, __run, this);
        if (e != 0)
        {
            throw exc(str::asprintf(_("Cannot create thread: %s"), std::strerror(e)), e);
        }
        __joinable = true;
    }
}

void thread::wait()
{
    __wait_mutex.lock();
    if (atomic::bool_compare_and_swap(&__joinable, true, false))
    {
        int e = pthread_join(__thread_id, NULL);
        if (e != 0)
        {
            __wait_mutex.unlock();
            throw exc(str::asprintf(_("Cannot join with thread: %s"), std::strerror(e)), e);
        }
    }
    __wait_mutex.unlock();
}

void thread::finish()
{
    wait();
    if (!exception().empty())
    {
        throw exception();
    }
}

void thread::cancel()
{
    __wait_mutex.lock();
    int e = pthread_cancel(__thread_id);
    if (e != 0)
    {
        __wait_mutex.unlock();
        throw exc(str::asprintf(_("Cannot cancel thread: %s"), std::strerror(e)), e);
    }
    __wait_mutex.unlock();
}
