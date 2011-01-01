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

/**
 * \file intcheck.h
 *
 * This file provides functions that operate on integers and check
 * for over- and underflow.
 */

#ifndef INTCHECK_H
#define INTCHECK_H

#include <cerrno>
#include <limits>

#include "exc.h"


/**
 * \param x     Integer value.
 * \return      Casted integer value.
 *
 * Cast the integer value \a x to the given type.
 * If over- or underflow would occur, this function throws exc(ERANGE).
 * Example: size_t a = checked_cast<size_t>(b);
 */
template<typename TO, typename FROM>
TO checked_cast(FROM x)
{
    // The goal of this case differentiation is to
    // a) help the compiler to optimize unnecessary checks away
    // b) avoid compiler warnings like 'comparison of signed and unsigned'
    if (std::numeric_limits<FROM>::is_signed && std::numeric_limits<TO>::is_signed)
    {
        if (sizeof(FROM) > sizeof(TO)
                && (x < static_cast<FROM>(std::numeric_limits<TO>::min())
                    || x > static_cast<FROM>(std::numeric_limits<TO>::max())))
        {
            throw exc(ERANGE);
        }
    }
    else if (!std::numeric_limits<FROM>::is_signed && !std::numeric_limits<TO>::is_signed)
    {
        if (sizeof(FROM) >= sizeof(TO) && x > static_cast<FROM>(std::numeric_limits<TO>::max()))
        {
            throw exc(ERANGE);
        }
    }
    else if (std::numeric_limits<FROM>::is_signed)      // TO is unsigned
    {
        if (x < static_cast<FROM>(std::numeric_limits<TO>::min())
                || (sizeof(FROM) > sizeof(TO) && x > static_cast<FROM>(std::numeric_limits<TO>::max())))
        {
            throw exc(ERANGE);
        }
    }
    else        // FROM is unsigned, TO is signed
    {
        if (sizeof(FROM) >= sizeof(TO) && x > static_cast<FROM>(std::numeric_limits<TO>::max()))
        {
            throw exc(ERANGE);
        }
    }
    return x;
}

/**
 * \param a     Integer value.
 * \param b     Integer value.
 * \return      a + b
 *
 * Return a + b.
 * If over- or underflow would occur, this function throws exc(ERANGE).
 */
template<typename T>
T checked_add(T a, T b)
{
    if (b < static_cast<T>(1))
    {
        if (a < std::numeric_limits<T>::min() - b)
        {
            throw exc(ERANGE);
        }
    }
    else
    {
        if (a > std::numeric_limits<T>::max() - b)
        {
            throw exc(ERANGE);
        }
    }
    return a + b;
}

/**
 * \param a     Integer value.
 * \param b     Integer value.
 * \return      a - b
 *
 * Return a - b.
 * If over- or underflow would occur, this function throws exc(ERANGE).
 */
template<typename T>
T checked_sub(T a, T b)
{
    if (b < static_cast<T>(1))
    {
        if (a > std::numeric_limits<T>::max() + b)
        {
            throw exc(ERANGE);
        }
    }
    else
    {
        if (a < std::numeric_limits<T>::min() + b)
        {
            throw exc(ERANGE);
        }
    }
    return a - b;
}

/**
 * \param a     Integer value.
 * \param b     Integer value.
 * \return      a * b
 *
 * Return a * b.
 * If over- or underflow would occur, this function throws exc(ERANGE).
 */
template<typename T>
T checked_mul(T a, T b)
{
    if (std::numeric_limits<T>::is_signed)
    {
        /* Adapted from the comp.lang.c FAQ, see http://c-faq.com/misc/sd26.html */
        if (a == std::numeric_limits<T>::min())
        {
            if (b != static_cast<T>(0) && b != static_cast<T>(1))
            {
                throw exc(ERANGE);
            }
        }
        else if (b == std::numeric_limits<T>::min())
        {
            if (a != static_cast<T>(0) && a != static_cast<T>(1))
            {
                throw exc(ERANGE);
            }
        }
        else
        {
            if (a < static_cast<T>(1))
            {
                a = -a;
            }
            if (b < static_cast<T>(1))
            {
                b = -b;
            }
        }
    }
    if (!(b == static_cast<T>(0) || !(std::numeric_limits<T>::max() / b < a)))
    {
        throw exc(ERANGE);
    }
    return a * b;
}

/**
 * \param a     Integer value.
 * \param b     Integer value.
 * \return      a / b
 *
 * Return a / b.
 * If over- or underflow would occur, this function throws exc(ERANGE).
 */
template<typename T>
T checked_div(T a, T b)
{
    if (b == 0
            || (std::numeric_limits<T>::is_signed
                && a == std::numeric_limits<T>::min()
                && b == static_cast<T>(-1)))
    {
        throw exc(ERANGE);
    }
    return a / b;
}

#endif
