/*
 * Copyright (C) 2010, 2011, 2012
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
 * \file blob.h
 *
 * This class provides an opaque memory block of a given size which can
 * store any kind of data. Such memory blocks are a pain to manage with
 * new/delete or new[]/delete[] or malloc()/free() because of the necessary
 * type casting. This class provides easy access pointers and a destructor.
 */

#ifndef BLOB_H
#define BLOB_H

#include <cstdlib>
#include <cstring>
#include <cerrno>

#include "exc.h"
#include "intcheck.h"


class blob
{
private:

    size_t _size;
    void* _ptr;

    static void* alloc(size_t s)
    {
        void* ptr = std::malloc(s);
        if (s != 0 && !ptr) {
            throw exc(ENOMEM);
        }
        return ptr;
    }

    static void* realloc(void* p, size_t s)
    {
        void* ptr = std::realloc(p, s);
        if (s != 0 && !ptr) {
            throw exc(ENOMEM);
        }
        return ptr;
    }

public:

    blob() throw () :
        _size(0), _ptr(NULL)
    {
    }

    blob(size_t s) :
        _size(s), _ptr(alloc(_size))
    {
    }

    blob(size_t s, size_t n) :
        _size(checked_mul(s, n)), _ptr(alloc(_size))
    {
    }

    blob(size_t s, size_t n0, size_t n1) :
        _size(checked_mul(checked_mul(s, n0), n1)), _ptr(alloc(_size))
    {
    }

    blob(size_t s, size_t n0, size_t n1, size_t n2) :
        _size(checked_mul(checked_mul(s, n0), checked_mul(n1, n2))), _ptr(alloc(_size))
    {
    }

    blob(const blob& b) :
        _size(b._size), _ptr(alloc(_size))
    {
        if (_size > 0) {
            std::memcpy(_ptr, b._ptr, _size);
        }
    }

    const blob& operator=(const blob& b)
    {
        if (b.size() == 0) {
            std::free(_ptr);
            _size = 0;
            _ptr = NULL;
        } else {
            void *ptr = alloc(b.size());
            std::memcpy(ptr, b.ptr(), b.size());
            std::free(_ptr);
            _ptr = ptr;
            _size = b.size();
        }
        return *this;
    }

    ~blob() throw ()
    {
        std::free(_ptr);
    }

    void free()
    {
        std::free(_ptr);
        _ptr = NULL;
        _size = 0;
    }

    void resize(size_t s)
    {
        _ptr = realloc(_ptr, s);
        _size = s;
    }

    void resize(size_t s, size_t n)
    {
        _ptr = realloc(_ptr, checked_mul(s, n));
        _size = s;
    }

    void resize(size_t s, size_t n0, size_t n1)
    {
        _ptr = realloc(_ptr, checked_mul(checked_mul(s, n0), n1));
        _size = s;
    }

    void resize(size_t s, size_t n0, size_t n1, size_t n2)
    {
        _ptr = realloc(_ptr, checked_mul(checked_mul(s, n0), checked_mul(n1, n2)));
        _size = s;
    }

    size_t size() const throw ()
    {
        return _size;
    }

    const void* ptr(size_t offset = 0) const throw ()
    {
        return static_cast<const void*>(static_cast<const char*>(_ptr) + offset);
    }

    void* ptr(size_t offset = 0) throw ()
    {
        return static_cast<void*>(static_cast<char*>(_ptr) + offset);
    }

    template<typename T>
    const T* ptr(size_t offset = 0) const throw ()
    {
        return static_cast<const T*>(ptr(offset * sizeof(T)));
    }

    template<typename T>
    T* ptr(size_t offset = 0) throw ()
    {
        return static_cast<T*>(ptr(offset * sizeof(T)));
    }
};

#endif
