/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013
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
 * \file exc.h
 *
 * Error and exception handling.
 *
 * This should be used as follows:
 *
 * 1. Throw only objects derived from std::exception.
 * 2. Catch only objects of type std::exception, and catch by reference.
 *    In almost all cases, the information given by std::exception::what() is
 *    all you need. If you are not interested in any information from the
 *    exception, use 'catch (...)'. Only catch other types if you absolutely
 *    have to, e.g. when dealing with external libraries.
 * 3. Only mark functions with an exception specification if they can never
 *    throw exceptions, i.e. only ever use the exception specification 'throw ()'.
 * 4. Assume that all functions not explicitly marked with 'throw ()' can throw
 *    exceptions of type std::exception (see rule 1). Using the specification
 *    'throw (std::exception)' in this case would not be useful and would only
 *    lead to extra try/catch block insertion by the compiler, so do not do it.
 * 5. Do not throw exceptions from a constructor. This only leads to headaches.
 *    If you have to, add a init() or start() member function instead.
 */

#ifndef EXC_H
#define EXC_H

#include <exception>
#include <string>

/**
 * Error and exception handling.
 */

class exc : public std::exception
{
    private:
        static const char *_fallback_str;
        bool _fallback;
        std::string _str;
        int _sys_errno;

    public:
        exc() throw ();
        exc(const std::string &what, int sys_errno = 0) throw ();
        exc(int sys_errno) throw ();
        exc(const exc &e) throw ();
        exc(const std::exception &e) throw ();
        ~exc() throw ();

        bool empty() const throw ();
        int sys_errno() const throw ();
        virtual const char *what() const throw ();
};

#endif
