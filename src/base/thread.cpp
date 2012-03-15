/*
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

#include <cerrno>
#include <cstring>
#include <pthread.h>
#include <sched.h>

#include "gettext.h"
#define _(string) gettext(string)

#include "thread.h"


const pthread_mutex_t mutex::_mutex_initializer = PTHREAD_MUTEX_INITIALIZER;

mutex::mutex() : _mutex(_mutex_initializer)
{
    int e = pthread_mutex_init(&_mutex, NULL);
    if (e != 0)
        throw exc(std::string(_("System function failed: "))
                + "pthread_mutex_init(): " + std::strerror(e), e);
}

mutex::mutex(const mutex&) : _mutex(_mutex_initializer)
{
    // You cannot have multiple copies of the same mutex.
    // Instead, we create a new one. This allows easier use of mutexes in STL containers.
    int e = pthread_mutex_init(&_mutex, NULL);
    if (e != 0)
        throw exc(std::string(_("System function failed: "))
                + "pthread_mutex_init(): " + std::strerror(e), e);
}

mutex::~mutex()
{
    (void)pthread_mutex_destroy(&_mutex);
}

void mutex::lock()
{
    int e = pthread_mutex_lock(&_mutex);
    if (e != 0)
        throw exc(std::string(_("System function failed: "))
                + "pthread_mutex_lock(): " + std::strerror(e), e);
}

bool mutex::trylock()
{
    return (pthread_mutex_trylock(&_mutex) == 0);
}

void mutex::unlock()
{
    int e = pthread_mutex_unlock(&_mutex);
    if (e != 0)
        throw exc(std::string(_("System function failed: "))
                + "pthread_mutex_unlock(): " + std::strerror(e), e);
}


const pthread_cond_t condition::_cond_initializer = PTHREAD_COND_INITIALIZER;

condition::condition() : _cond(_cond_initializer)
{
    int e = pthread_cond_init(&_cond, NULL);
    if (e != 0)
        throw exc(std::string(_("System function failed: "))
                + "pthread_cond_init(): " + std::strerror(e), e);
}

condition::condition(const condition&) : _cond(_cond_initializer)
{
    // You cannot have multiple copies of the same condition.
    // Instead, we create a new one. This allows easier use of conditions in STL containers.
    int e = pthread_cond_init(&_cond, NULL);
    if (e != 0)
        throw exc(std::string(_("System function failed: "))
                + "pthread_cond_init(): " + std::strerror(e), e);
}

condition::~condition()
{
    (void)pthread_cond_destroy(&_cond);
}

void condition::wait(mutex& m)
{
    int e = pthread_cond_wait(&_cond, &m._mutex);
    if (e != 0)
        throw exc(std::string(_("System function failed: "))
                + "pthread_cond_wait(): " + std::strerror(e), e);
}

void condition::wake_one()
{
    int e = pthread_cond_signal(&_cond);
    if (e != 0)
        throw exc(std::string(_("System function failed: "))
                + "pthread_cond_signal(): " + std::strerror(e), e);
}

void condition::wake_all()
{
    int e = pthread_cond_broadcast(&_cond);
    if (e != 0)
        throw exc(std::string(_("System function failed: "))
                + "pthread_cond_broadcast(): " + std::strerror(e), e);
}


const int thread::priority_default;
const int thread::priority_min;

thread::thread() :
    __thread_id(pthread_self()),
    __joinable(false),
    __running(false),
    __wait_mutex(),
    __exception()
{
}

thread::thread(const thread&) :
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
        (void)pthread_detach(__thread_id);
}

void* thread::__run(void* p)
{
    thread* t = static_cast<thread*>(p);
    try {
        t->run();
    }
    catch (exc& e) {
        t->__exception = e;
    }
    catch (std::exception& e) {
        t->__exception = e;
    }
    catch (...) {
        // We must assume this means thread cancellation. In this
        // case, we *must* rethrow the exception.
        t->__running = false;
        throw;
    }
    t->__running = false;
    return NULL;
}

void thread::start(int priority)
{
    if (atomic::bool_compare_and_swap(&__running, false, true)) {
        wait();
        int e;
        pthread_attr_t priority_thread_attr;
        pthread_attr_t* thread_attr = NULL;
        if (priority != priority_default)
        {
            // Currently this means priority_min
            int policy, min_priority;
            struct sched_param param;
            e = pthread_attr_init(&priority_thread_attr);
            e = e || pthread_attr_getschedpolicy(&priority_thread_attr, &policy);
            if (e == 0) {
                min_priority = sched_get_priority_min(policy);
                if (min_priority == -1)
                    e = errno;
            }
            e = e || pthread_attr_getschedparam(&priority_thread_attr, &param);
            if (e == 0) {
                param.sched_priority = min_priority;
            }
            e = e || pthread_attr_setschedparam(&priority_thread_attr, &param);
            if (e != 0) {
                throw exc(std::string(_("System function failed: "))
                        + "pthread_attr_*(): " + std::strerror(e), e);
            }
            thread_attr = &priority_thread_attr;
        }
        e = pthread_create(&__thread_id, thread_attr, __run, this);
        if (e != 0) {
            throw exc(std::string(_("System function failed: "))
                    + "pthread_create(): " + std::strerror(e), e);
        }
        __joinable = true;
    }
}

void thread::wait()
{
    __wait_mutex.lock();
    if (atomic::bool_compare_and_swap(&__joinable, true, false)) {
        int e = pthread_join(__thread_id, NULL);
        if (e != 0) {
            __wait_mutex.unlock();
            throw exc(std::string(_("System function failed: "))
                    + "pthread_join(): " + std::strerror(e), e);
        }
    }
    __wait_mutex.unlock();
}

void thread::finish()
{
    wait();
    if (!exception().empty())
        throw exception();
}

void thread::cancel()
{
    __wait_mutex.lock();
    int e = pthread_cancel(__thread_id);
    if (e != 0) {
        __wait_mutex.unlock();
        throw exc(std::string(_("System function failed: "))
                + "pthread_cancel(): " + std::strerror(e), e);
    }
    __wait_mutex.unlock();
}


thread_group::thread_group(unsigned char size) : __max_size(size)
{
    __active_threads.reserve(__max_size);
    __finished_threads.reserve(__max_size);
}

thread_group::~thread_group()
{
    try {
        for (size_t i = 0; i < __active_threads.size(); i++) {
            __active_threads[i]->cancel();
        }
    }
    catch (...) {
    }
}

bool thread_group::start(thread* t, int priority)
{
    if (__active_threads.size() < __max_size) {
        t->start(priority);
        __active_threads.push_back(t);
        return true;
    } else {
        return false;
    }
}

thread* thread_group::get_next_finished_thread()
{
    if (__finished_threads.size() == 0) {
        std::vector<thread*>::iterator it = __active_threads.begin();
        while (it != __active_threads.end()) {
            if (!(*it)->is_running()) {
                __finished_threads.push_back(*it);
                it = __active_threads.erase(it);
            } else {
                it++;
            }
        }
    }
    if (__finished_threads.size() > 0) {
        thread* ret = __finished_threads.back();
        __finished_threads.pop_back();
        return ret;
    } else {
        return NULL;
    }
}
