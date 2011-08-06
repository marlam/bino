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

#ifndef THREADS_H
#define THREADS_H

#include <pthread.h>

#include "exc.h"


/*
 * Atomic operations
 */

namespace atomic
{
    // TODO
    //
    // Currently this is just an interface to the GCC atomic builtin functions; see
    // http://gcc.gnu.org/onlinedocs/gcc-4.4.1/gcc/Atomic-Builtins.html#Atomic-Builtins
    //
    // This must be extended when (if) other compilers are used some day.
    //
    // The functions obviously only work for basic data types.
    // The comments were copied from the GCC manual referenced above.

    /* The following functions perform the operation suggested by the name,
     * and return the value that had previously been in memory. */
    template<typename T> T fetch_and_add(T *ptr, T value) { return __sync_fetch_and_add(ptr, value); }
    template<typename T> T fetch_and_sub(T *ptr, T value) { return __sync_fetch_and_sub(ptr, value); }
    template<typename T> T fetch_and_or(T *ptr, T value) { return __sync_fetch_and_or(ptr, value); }
    template<typename T> T fetch_and_and(T *ptr, T value) { return __sync_fetch_and_and(ptr, value); }
    template<typename T> T fetch_and_xor(T *ptr, T value) { return __sync_fetch_and_xor(ptr, value); }
    template<typename T> T fetch_and_nand(T *ptr, T value) { return __sync_fetch_and_nand(ptr, value); }

    /* The following functions perform the operation suggested by the name,
     * and return the new value. */
    template<typename T> T add_and_fetch(T *ptr, T value) { return __sync_add_and_fetch(ptr, value); }
    template<typename T> T sub_and_fetch(T *ptr, T value) { return __sync_sub_and_fetch(ptr, value); }
    template<typename T> T or_and_fetch(T *ptr, T value) { return __sync_or_and_fetch(ptr, value); }
    template<typename T> T and_and_fetch(T *ptr, T value) { return __sync_and_and_fetch(ptr, value); }
    template<typename T> T xor_and_fetch(T *ptr, T value) { return __sync_xor_and_fetch(ptr, value); }
    template<typename T> T nand_and_fetch(T *ptr, T value) { return __sync_nand_and_fetch(ptr, value); }

    /* The following functions perform an atomic compare and swap.
     * That is, if the current value of *ptr is oldval, then write newval into *ptr.
     * The "bool" version returns true if the comparison is successful and newval was written.
     * The "val" version returns the contents of *ptr before the operation. */
    template<typename T> bool bool_compare_and_swap(T *ptr, T oldval, T newval) { return __sync_bool_compare_and_swap(ptr, oldval, newval); }
    template<typename T> T val_compare_and_swap(T *ptr, T oldval, T newval) { return __sync_val_compare_and_swap(ptr, oldval, newval); }

    /* The following are convenience functions implemented on top of the above
     * basic atomic operations. */
    template<typename T> T fetch(T *ptr) { return fetch_and_add(ptr, static_cast<T>(0)); }
    template<typename T> T increment(T *ptr) { return add_and_fetch(ptr, static_cast<T>(1)); }
    template<typename T> T decrement(T *ptr) { return sub_and_fetch(ptr, static_cast<T>(1)); }
}


/*
 * Mutex
 */

class mutex
{
private:
    static const pthread_mutex_t _mutex_initializer;
    pthread_mutex_t _mutex;

public:
    // Constructor / Destructor
    mutex();
    mutex(const mutex &m);
    ~mutex();

    // Lock the mutex.
    void lock();
    // Try to lock the mutex. Return true on success, false otherwise.
    bool trylock();
    // Unlock the mutex
    void unlock();
};


/*
 * Thread
 *
 * Implement the run() function in a subclass.
 */

class thread
{
private:
    pthread_t __thread_id;
    bool __joinable;
    bool __running;
    mutex __wait_mutex;
    exc __exception;

    static void *__run(void *p);

public:
    // Constructor / Destructor
    thread();
    thread(const thread &t);
    virtual ~thread();

    // Implement this in a subclass; it will be executed from the thread via start()
    virtual void run() = 0;

    // Start a new thread that executes the run() function. If the thread is already
    // running, this function does nothing.
    void start();

    // Returns whether this thread is currently running.
    bool is_running()
    {
        return __running;
    }

    // Wait for the thread to finish. If the thread is not running, this function
    // returns immediately.
    void wait();

    // Wait for the thread to finish, like wait(), and rethrow an exception that the
    // run() function might have thrown during its execution.
    void finish();

    // Cancel a thread. This is dangerous and should not be used.
    void cancel();

    // Get an exception that the run() function might have thrown.
    const exc &exception() const
    {
        return __exception;
    }
    // Modify the stored exception
    exc &exception()
    {
        return __exception;
    }
};

#endif
