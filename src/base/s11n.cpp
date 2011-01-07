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

#include "config.h"

#include "s11n.h"


/*
 * Save a value to a stream
 */

// Fundamental arithmetic data types

void s11n::save(std::ostream &os, const bool x)
{
    os.write(reinterpret_cast<const char *>(&x), sizeof(x));
}

void s11n::save(std::ostream &os, const char x)
{
    os.write(reinterpret_cast<const char *>(&x), sizeof(x));
}

void s11n::save(std::ostream &os, const signed char x)
{
    os.write(reinterpret_cast<const char *>(&x), sizeof(x));
}

void s11n::save(std::ostream &os, const unsigned char x)
{
    os.write(reinterpret_cast<const char *>(&x), sizeof(x));
}

void s11n::save(std::ostream &os, const short x)
{
    os.write(reinterpret_cast<const char *>(&x), sizeof(x));
}

void s11n::save(std::ostream &os, const unsigned short x)
{
    os.write(reinterpret_cast<const char *>(&x), sizeof(x));
}

void s11n::save(std::ostream &os, const int x)
{
    os.write(reinterpret_cast<const char *>(&x), sizeof(x));
}

void s11n::save(std::ostream &os, const unsigned int x)
{
    os.write(reinterpret_cast<const char *>(&x), sizeof(x));
}

void s11n::save(std::ostream &os, const long x)
{
    os.write(reinterpret_cast<const char *>(&x), sizeof(x));
}

void s11n::save(std::ostream &os, const unsigned long x)
{
    os.write(reinterpret_cast<const char *>(&x), sizeof(x));
}

void s11n::save(std::ostream &os, const long long x)
{
    os.write(reinterpret_cast<const char *>(&x), sizeof(x));
}

void s11n::save(std::ostream &os, const unsigned long long x)
{
    os.write(reinterpret_cast<const char *>(&x), sizeof(x));
}

void s11n::save(std::ostream &os, const float x)
{
    os.write(reinterpret_cast<const char *>(&x), sizeof(x));
}

void s11n::save(std::ostream &os, const double x)
{
    os.write(reinterpret_cast<const char *>(&x), sizeof(x));
}

void s11n::save(std::ostream &os, const long double x)
{
    os.write(reinterpret_cast<const char *>(&x), sizeof(x));
}

// Binary blobs

void s11n::save(std::ostream &os, const void *x, const size_t n)
{
    os.write(reinterpret_cast<const char *>(x), n);
}

// Serializable classes

void s11n::save(std::ostream &os, const s11n &x)
{
    x.save(os);
}

// Basic STL types

void s11n::save(std::ostream &os, const std::string &x)
{
    const size_t s = x.length();
    os.write(reinterpret_cast<const char *>(&s), sizeof(s));
    os.write(reinterpret_cast<const char *>(x.data()), s);
}


/*
 * Load a value from a stream
 */

// Fundamental arithmetic data types

void s11n::load(std::istream &is, bool &x)
{
    is.read(reinterpret_cast<char *>(&x), sizeof(x));
}

void s11n::load(std::istream &is, char &x)
{
    is.read(reinterpret_cast<char *>(&x), sizeof(x));
}

void s11n::load(std::istream &is, signed char &x)
{
    is.read(reinterpret_cast<char *>(&x), sizeof(x));
}

void s11n::load(std::istream &is, unsigned char &x)
{
    is.read(reinterpret_cast<char *>(&x), sizeof(x));
}

void s11n::load(std::istream &is, short &x)
{
    is.read(reinterpret_cast<char *>(&x), sizeof(x));
}

void s11n::load(std::istream &is, unsigned short &x)
{
    is.read(reinterpret_cast<char *>(&x), sizeof(x));
}

void s11n::load(std::istream &is, int &x)
{
    is.read(reinterpret_cast<char *>(&x), sizeof(x));
}

void s11n::load(std::istream &is, unsigned int &x)
{
    is.read(reinterpret_cast<char *>(&x), sizeof(x));
}

void s11n::load(std::istream &is, long &x)
{
    is.read(reinterpret_cast<char *>(&x), sizeof(x));
}

void s11n::load(std::istream &is, unsigned long &x)
{
    is.read(reinterpret_cast<char *>(&x), sizeof(x));
}

void s11n::load(std::istream &is, long long &x)
{
    is.read(reinterpret_cast<char *>(&x), sizeof(x));
}

void s11n::load(std::istream &is, unsigned long long &x)
{
    is.read(reinterpret_cast<char *>(&x), sizeof(x));
}

void s11n::load(std::istream &is, float &x)
{
    is.read(reinterpret_cast<char *>(&x), sizeof(x));
}

void s11n::load(std::istream &is, double &x)
{
    is.read(reinterpret_cast<char *>(&x), sizeof(x));
}

void s11n::load(std::istream &is, long double &x)
{
    is.read(reinterpret_cast<char *>(&x), sizeof(x));
}

// Binary blobs

void s11n::load(std::istream &is, void *x, const size_t n)
{
    is.read(reinterpret_cast<char *>(x), n);
}

// Serializable classes

void s11n::load(std::istream &is, s11n &x)
{
    x.load(is);
}

// Basic STL types

void s11n::load(std::istream &is, std::string &x)
{
    size_t s;
    is.read(reinterpret_cast<char *>(&s), sizeof(s));
    char *buf = new char[s];
    is.read(buf, s);
    x.assign(buf, s);
    delete[] buf;
}
