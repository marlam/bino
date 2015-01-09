/*
 * Copyright (C) 2008, 2009, 2010, 2011, 2012, 2013, 2014
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
#include <sstream>
#include <string>
#include <vector>


class serializable
{
public:
    /*
     * Interface for serializable classes
     */

    // Save binary. Efficient. Needs compatible architectures and application versions on the writing and reading end.
    virtual void save(std::ostream& os) const = 0;
    virtual void load(std::istream& is) = 0;

    // Save in human-readable and editable text format. Can be used for machine- and application version independent
    // saving of data. The default implementation falls back on the above binary functions and stores the binary data in
    // strings.
    virtual void save(std::ostream& os, const char* name) const;
    virtual void load(const std::string& s);
};

namespace s11n
{
    // Functions needed for human-readable save/load:
    // - When saving a group of values contained in a serializable class, use startgroup()/endgroup()
    // - When loading back, get the next name/value pair and interpret it
    void startgroup(std::ostream& os, const char* name);
    void endgroup(std::ostream& os);
    void load(std::istream& is, std::string& name, std::string& value);

    /*
     * Save a value to a stream
     */

    // Fundamental arithmetic data types

    void save(std::ostream& os, bool x);
    void save(std::ostream& os, const char* name, bool x);
    void save(std::ostream& os, char x);
    void save(std::ostream& os, const char* name, char x);
    void save(std::ostream& os, signed char x);
    void save(std::ostream& os, const char* name, signed char x);
    void save(std::ostream& os, unsigned char x);
    void save(std::ostream& os, const char* name, unsigned char x);
    void save(std::ostream& os, short x);
    void save(std::ostream& os, const char* name, short x);
    void save(std::ostream& os, unsigned short x);
    void save(std::ostream& os, const char* name, unsigned short x);
    void save(std::ostream& os, int x);
    void save(std::ostream& os, const char* name, int x);
    void save(std::ostream& os, unsigned int x);
    void save(std::ostream& os, const char* name, unsigned int x);
    void save(std::ostream& os, long x);
    void save(std::ostream& os, const char* name, long x);
    void save(std::ostream& os, unsigned long x);
    void save(std::ostream& os, const char* name, unsigned long x);
    void save(std::ostream& os, long long x);
    void save(std::ostream& os, const char* name, long long x);
    void save(std::ostream& os, unsigned long long x);
    void save(std::ostream& os, const char* name, unsigned long long x);
    void save(std::ostream& os, float x);
    void save(std::ostream& os, const char* name, float x);
    void save(std::ostream& os, double x);
    void save(std::ostream& os, const char* name, double x);
    void save(std::ostream& os, long double x);
    void save(std::ostream& os, const char* name, long double x);

    // Binary blobs

    void save(std::ostream& os, const void* x, const size_t n);
    void save(std::ostream& os, const char* name, const void* x, const size_t n);

    // Serializable classes

    void save(std::ostream& os, const serializable& x);
    void save(std::ostream& os, const char* name, const serializable& x);

    // Basic STL types

    void save(std::ostream& os, const std::string& x);
    void save(std::ostream& os, const char* name, const std::string& x);

    // STL containters

    template<typename T>
    void save(std::ostream& os, const std::vector<T>& x)
    {
        size_t s = x.size();
        save(os, s);
        for (size_t i = 0; i < s; i++) {
            save(os, x[i]);
        }
    }

    template<typename T>
    void save(std::ostream& os, const char* name, const std::vector<T>& x)
    {
        size_t s = x.size();
        startgroup(os, name);
        save(os, "size", s);
        for (size_t i = 0; i < s; i++) {
            save(os, "", x[i]);
        }
        endgroup(os);
    }

    // TODO: add more STL containers as needed

    /*
     * Load a value from a stream
     */

    // Fundamental arithmetic data types

    void load(std::istream& is, bool& x);
    void load(const std::string& s, bool& x);
    void load(std::istream& is, char& x);
    void load(const std::string& s, char& x);
    void load(std::istream& is, signed char& x);
    void load(const std::string& s, signed char& x);
    void load(std::istream& is, unsigned char& x);
    void load(const std::string& s, unsigned char& x);
    void load(std::istream& is, short& x);
    void load(const std::string& s, short& x);
    void load(std::istream& is, unsigned short& x);
    void load(const std::string& s, unsigned short& x);
    void load(std::istream& is, int& x);
    void load(const std::string& s, int& x);
    void load(std::istream& is, unsigned int& x);
    void load(const std::string& s, unsigned int& x);
    void load(std::istream& is, long& x);
    void load(const std::string& s, long& x);
    void load(std::istream& is, unsigned long& x);
    void load(const std::string& s, unsigned long& x);
    void load(std::istream& is, long long& x);
    void load(const std::string& s, long long& x);
    void load(std::istream& is, unsigned long long& x);
    void load(const std::string& s, unsigned long long& x);
    void load(std::istream& is, float& x);
    void load(const std::string& s, float& x);
    void load(std::istream& is, double& x);
    void load(const std::string& s, double& x);
    void load(std::istream& is, long double& x);
    void load(const std::string& s, long double& x);

    // Binary blobs

    void load(std::istream& is, void* x, const size_t n);
    void load(const std::string& s, void* x, const size_t n);

    // Serializable classes

    void load(std::istream& is, serializable& x);
    void load(const std::string& s, serializable& x);

    // Basic STL types

    void load(std::istream& is, std::string& x);
    void load(const std::string& s, std::string& x);

    // STL containers

    template<typename T>
    void load(std::istream& is, std::vector<T>& x)
    {
        size_t s;
        load(is, s);
        x.resize(s);
        for (size_t i = 0; i < s; i++) {
            load(is, x[i]);
        }
    }

    template<typename T>
    void load(const std::string& s, std::vector<T>& x)
    {
        std::stringstream ss(s);
        std::string name, value;
        size_t z = 0;
        load(ss, name, value);
        if (name == "size")
            load(value, z);
        x.resize(z);
        size_t i = 0;
        while (ss.good() && i < z) {
            load(ss, name, value);
            load(value, x[i]);
            i++;
        }
    }

    // TODO: add more STL containers as needed

    // Convenience wrappers: directly return a loaded value

    template<typename T>
    T load(std::istream& is)
    {
        T x;
        load(is, x);
        return x;
    }

    template<typename T>
    T load(const std::string& s)
    {
        T x;
        load(s, x);
        return x;
    }
};

#endif
