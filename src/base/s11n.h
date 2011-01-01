/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2008-2011
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

#ifndef S11N_H
#define S11N_H

/*
 * This module provides functions s11n::save() and s11n::load() that serialize
 * types and objects.
 * This works for all basic data types, STL types such as std::string, and
 * classes that implement the s11n interface.
 * Additionally, STL containers of serializable types can also be serialized,
 * e.g. std::vector<std::string>.
 */

#include <iostream>
#include <string>
#include <vector>


class s11n
{
public:

    /*
     * Interface for serializable classes
     */

    virtual void save(std::ostream &os) const = 0;
    virtual void load(std::istream &is) = 0;


    /*
     * Save a value to a stream
     */

    // Fundamental arithmetic data types

    static void save(std::ostream &os, const bool x);
    static void save(std::ostream &os, const char x);
    static void save(std::ostream &os, const signed char x);
    static void save(std::ostream &os, const unsigned char x);
    static void save(std::ostream &os, const short x);
    static void save(std::ostream &os, const unsigned short x);
    static void save(std::ostream &os, const int x);
    static void save(std::ostream &os, const unsigned int x);
    static void save(std::ostream &os, const long x);
    static void save(std::ostream &os, const unsigned long x);
    static void save(std::ostream &os, const long long x);
    static void save(std::ostream &os, const unsigned long long x);
    static void save(std::ostream &os, const float x);
    static void save(std::ostream &os, const double x);
    static void save(std::ostream &os, const long double x);

    // Binary blobs

    static void save(std::ostream &os, const void *x, const size_t n);

    // Serializable classes

    static void save(std::ostream &os, const s11n &x);

    // Basic STL types

    static void save(std::ostream &os, const std::string &x);

    // STL containters

    template<typename T>
    static void save(std::ostream &os, const std::vector<T> &x)
    {
        size_t s = x.size();
        save(os, s);
        for (size_t i = 0; i < s; i++)
        {
            save(os, x[i]);
        }
    }

    // TODO: add more STL containers as needed


    /*
     * Load a value from a stream
     */

    // Fundamental arithmetic data types

    static void load(std::istream &is, bool &x);
    static void load(std::istream &is, char &x);
    static void load(std::istream &is, signed char &x);
    static void load(std::istream &is, unsigned char &x);
    static void load(std::istream &is, short &x);
    static void load(std::istream &is, unsigned short &x);
    static void load(std::istream &is, int &x);
    static void load(std::istream &is, unsigned int &x);
    static void load(std::istream &is, long &x);
    static void load(std::istream &is, unsigned long &x);
    static void load(std::istream &is, long long &x);
    static void load(std::istream &is, unsigned long long &x);
    static void load(std::istream &is, float &x);
    static void load(std::istream &is, double &x);
    static void load(std::istream &is, long double &x);

    // Binary blobs

    static void load(std::istream &is, void *x, const size_t n);

    // Serializable classes

    static void load(std::istream &is, s11n &x);

    // Basic STL types

    static void load(std::istream &is, std::string &x);

    // STL containers

    template<typename T>
    static void load(std::istream &is, std::vector<T> &x)
    {
        x.clear();
        size_t s;
        load(is, s);
        for (size_t i = 0; i < s; i++)
        {
            T v;
            load(is, v);
            x.push_back(v);
        }
    }

    // TODO: add more STL containers as needed
};

#endif
