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

#include "dbg.h"
#include "thread.h"


thread::thread() : _thread_id(pthread_self()), _joinable(false), _running(false), _exception()
{
}

thread::~thread()
{
    if (_joinable)
    {
        (void)pthread_detach(_thread_id);
    }
}

void *thread::_run(void *p)
{
    thread *t = static_cast<thread *>(p);
    try
    {
        t->run();
    }
    catch (exc &e)
    {
        t->_exception = e;
    }
    catch (std::exception &e)
    {
        t->_exception = e;
    }
    catch (...)
    {
        t->_exception = exc("Unknown exception");
    }
    t->_running = false;
    return NULL;
}

void thread::start()
{
    if (atomic::bool_compare_and_swap(&_running, false, true))
    {
        wait();
        int e = pthread_create(&_thread_id, NULL, _run, this);
        if (e != 0)
        {
            throw exc(std::string("Cannot create thread: ") + std::strerror(e), e);
        }
        _joinable = true;
    }
}

void thread::wait()
{
    _wait_mutex.lock();
    if (atomic::bool_compare_and_swap(&_joinable, true, false))
    {
        int e = pthread_join(_thread_id, NULL);
        if (e != 0)
        {
            _wait_mutex.unlock();
            throw exc(std::string("Cannot join with thread: ") + std::strerror(e), e);
        }
    }
    _wait_mutex.unlock();
}

void thread::finish()
{
    wait();
    if (!exception().empty())
    {
        throw exception();
    }
}

const pthread_mutex_t mutex::_mutex_initializer = PTHREAD_MUTEX_INITIALIZER;

mutex::mutex() : _mutex(_mutex_initializer)
{
    int e = pthread_mutex_init(&_mutex, NULL);
    if (e != 0)
    {
        throw exc(std::string("Cannot initialize mutex: ") + std::strerror(e), e);
    }
}

mutex::mutex(const mutex &) : _mutex(_mutex_initializer)
{
    // You cannot have multiple copies of the same mutex.
    // Instead, we create a new one. This allows easier use of mutexes in STL containers.
    int e = pthread_mutex_init(&_mutex, NULL);
    if (e != 0)
    {
        throw exc(std::string("Cannot initialize mutex: ") + std::strerror(e), e);
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
        throw exc(std::string("Cannot lock mutex: ") + std::strerror(e), e);
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
        throw exc(std::string("Cannot unlock mutex: ") + std::strerror(e), e);
    }
}
