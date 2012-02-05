/*
 * Copyright (C) 2008, 2009, 2010, 2011, 2012
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

#include "str.h"
#include "intcheck.h"

#include "s11n.h"


static const char* low_char_encodings[] = {
    "\\(NUL)", // 0
    "\\(SOH)", // 1
    "\\(STX)", // 2
    "\\(ETX)", // 3
    "\\(EOT)", // 4
    "\\(ENQ)", // 5
    "\\(ACK)", // 6
    "\\(BEL)", // 7
    "\\(_BS)", // 8
    "\\(_HT)", // 9
    "\\(_LF)", // 10
    "\\(_VT)", // 11
    "\\(_FF)", // 12
    "\\(_CR)", // 13
    "\\(_SO)", // 14
    "\\(_SI)", // 15
    "\\(DLE)", // 16
    "\\(DC1)", // 17
    "\\(DC2)", // 18
    "\\(DC3)", // 19
    "\\(DC4)", // 20
    "\\(NAK)", // 21
    "\\(SYN)", // 22
    "\\(ETB)", // 23
    "\\(CAN)", // 24
    "\\(_EM)", // 25
    "\\(SUB)", // 26
    "\\(ESC)", // 27
    "\\(_FS)", // 28
    "\\(_GS)", // 29
    "\\(_RS)", // 30
    "\\(_US)", // 31
};


void s11n::startgroup(std::ostream& os, const char* name)
{
    os << ' ' << name << "={";
}

void s11n::endgroup(std::ostream& os)
{
    os << " }";
}

void s11n::load(std::istream& is, std::string& name, std::string& value)
{
    char c;
    // skip leading spaces
    while (is.good() && (c = is.get()) == ' ');
    // read name
    name.clear();
    while (is.good() && c != '=') {
        name.push_back(c);
        c = is.get();
    }
    c = is.get();
    // read value
    value.clear();
    bool value_is_valid = false;
    int group_depth = 0;
    char enc_char[] = "\\(...)";
    for (;;) {
        bool c_is_valid = false;
        if (c == '\\') {
            c = is.get();
            if (c == '\\' || c == ' ' || c == '{' || c == '}') {
                c_is_valid = true;
            } else {
                enc_char[1] = c;
                enc_char[2] = is.get();
                enc_char[3] = is.get();
                enc_char[4] = is.get();
                enc_char[5] = is.get();
                if (std::memcmp(enc_char, "\\(DEL)", 6) == 0) {
                    c = 127;
                    c_is_valid = true;
                } else {
                    for (int j = 0; j <= 31; j++) {
                        if (std::memcmp(enc_char, low_char_encodings[j], 6) == 0) {
                            c = j;
                            c_is_valid = true;
                            break;
                        }
                    }
                }
            }
        } else {
            if (c == '{') {
                group_depth++;
            } else if (c == '}') {
                group_depth--;
                if (group_depth < 0)
                    break;
            }
            c_is_valid = true;
        }
        if (!c_is_valid || !is.good())
            break;
        value.append(1, c);
        if (group_depth == 0 && (is.peek() == ' ' || is.eof())) {
            value_is_valid = true;
            break;
        }
        c = is.get();
    }
    if (!value_is_valid)
        value = "";
    else if (value.length() >= 2 && value[0] == '{' && value[value.length() - 1] == '}')
        value = value.substr(1, value.length() - 2);
}

void serializable::save(std::ostream& os, const char* name) const
{
    // Default implementation: use binary save and store binary blob
    std::ostringstream oss;
    this->save(oss);
    s11n::startgroup(os, name);
    s11n::save(os, "size", oss.str().length());
    s11n::save(os, "", oss.str().data(), oss.str().length());
    s11n::endgroup(os);
}

void serializable::load(const std::string& s)
{
    // Default implementation: read and restore binary blob
    std::istringstream iss(s);
    std::string name, value;
    s11n::load(iss, name, value);
    size_t size = 0;
    if (name == "size")
        s11n::load(value, size);
    s11n::load(iss, name, value);
    std::string str;
    char* buf = new char[size];
    s11n::load(value, buf, size);
    str.assign(buf, size);
    delete[] buf;
    std::istringstream iss2(str);
    this->load(iss2);
}

// Return value NULL means the character can be written as is.
static const char* enc_char(char x)
{
    return (x >= 0 && x <= 31 ? low_char_encodings[static_cast<int>(x)]
            : x == 127 ? "\\(DEL)"
            : x == '{' ? "\\{"
            : x == '}' ? "\\}"
            : x == ' ' ? "\\ "
            : x == '\\' ? "\\\\"
            : NULL);
}

// Decode a character. The index i is incremented according to the characters consumed from s.
static char dec_char(const char* s, size_t& i)
{
    if (s[i] == '\\') {
        if (s[i + 1] == '\\' || s[i + 1] == ' ' || s[i + 1] == '{' || s[i + 1] == '}') {
            i++;
            return s[i++];
        } else if (s[i + 1] != '\0' && s[i + 2] != '\0' && s[i + 3] != '\0' && s[i + 4] != '\0' && s[i + 5] != '\0') {
            if (std::memcmp(s, "\\(DEL)", 6) == 0) {
                i += 6;
                return 127;
            } else {
                for (int j = 0; j <= 31; j++) {
                    if (std::memcmp(s, low_char_encodings[j], 6) == 0) {
                        i += 6;
                        return j;
                    }
                }
                // invalid string!
                return '\0';
            }
        } else {
            // invalid string!
            return '\0';
        }
    } else {
        return s[i++];
    }
}

/*
 * Save a value to a stream
 */

// Fundamental arithmetic data types

void s11n::save(std::ostream& os, bool x)
{
    os.write(reinterpret_cast<const char*>(&x), sizeof(x));
}

void s11n::save(std::ostream& os, const char* name, bool x)
{
    os << ' ' << name << '=' << (x ? '1' : '0');
}

void s11n::save(std::ostream& os, char x)
{
    os.write(reinterpret_cast<const char*>(&x), sizeof(x));
}

void s11n::save(std::ostream& os, const char* name, char x)
{
    const char* e = enc_char(x);
    os << ' ' << name << '=';
    if (e)
        os << e;
    else
        os << x;
}

void s11n::save(std::ostream& os, signed char x)
{
    os.write(reinterpret_cast<const char*>(&x), sizeof(x));
}

void s11n::save(std::ostream& os, const char* name, signed char x)
{
    os << ' ' << name << '=' << static_cast<int>(x);
}

void s11n::save(std::ostream& os, unsigned char x)
{
    os.write(reinterpret_cast<const char*>(&x), sizeof(x));
}

void s11n::save(std::ostream& os, const char* name, unsigned char x)
{
    os << ' ' << name << '=' << static_cast<int>(x);
}

void s11n::save(std::ostream& os, short x)
{
    os.write(reinterpret_cast<const char*>(&x), sizeof(x));
}

void s11n::save(std::ostream& os, const char* name, short x)
{
    os << ' ' << name << '=' << x;
}

void s11n::save(std::ostream& os, unsigned short x)
{
    os.write(reinterpret_cast<const char*>(&x), sizeof(x));
}

void s11n::save(std::ostream& os, const char* name, unsigned short x)
{
    os << ' ' << name << '=' << x;
}

void s11n::save(std::ostream& os, int x)
{
    os.write(reinterpret_cast<const char*>(&x), sizeof(x));
}

void s11n::save(std::ostream& os, const char* name, int x)
{
    os << ' ' << name << '=' << x;
}

void s11n::save(std::ostream& os, unsigned int x)
{
    os.write(reinterpret_cast<const char*>(&x), sizeof(x));
}

void s11n::save(std::ostream& os, const char* name, unsigned int x)
{
    os << ' ' << name << '=' << x;
}

void s11n::save(std::ostream& os, long x)
{
    os.write(reinterpret_cast<const char*>(&x), sizeof(x));
}

void s11n::save(std::ostream& os, const char* name, long x)
{
    os << ' ' << name << '=' << x;
}

void s11n::save(std::ostream& os, unsigned long x)
{
    os.write(reinterpret_cast<const char*>(&x), sizeof(x));
}

void s11n::save(std::ostream& os, const char* name, unsigned long x)
{
    os << ' ' << name << '=' << x;
}

void s11n::save(std::ostream& os, long long x)
{
    os.write(reinterpret_cast<const char*>(&x), sizeof(x));
}

void s11n::save(std::ostream& os, const char* name, long long x)
{
    os << ' ' << name << '=' << x;
}

void s11n::save(std::ostream& os, unsigned long long x)
{
    os.write(reinterpret_cast<const char*>(&x), sizeof(x));
}

void s11n::save(std::ostream& os, const char* name, unsigned long long x)
{
    os << ' ' << name << '=' << x;
}

void s11n::save(std::ostream& os, float x)
{
    os.write(reinterpret_cast<const char*>(&x), sizeof(x));
}

void s11n::save(std::ostream& os, const char* name, float x)
{
    os << ' ' << name << '=' << str::from(x).c_str();
}

void s11n::save(std::ostream& os, double x)
{
    os.write(reinterpret_cast<const char*>(&x), sizeof(x));
}

void s11n::save(std::ostream& os, const char* name, double x)
{
    os << ' ' << name << '=' << str::from(x).c_str();
}

void s11n::save(std::ostream& os, long double x)
{
    os.write(reinterpret_cast<const char*>(&x), sizeof(x));
}

void s11n::save(std::ostream& os, const char* name, long double x)
{
    os << ' ' << name << '=' << str::from(x).c_str();
}

// Binary blobs

void s11n::save(std::ostream& os, const void* x, const size_t n)
{
    os.write(reinterpret_cast<const char*>(x), n);
}

void s11n::save(std::ostream& os, const char* name, const void* x, const size_t n)
{
    static const char hex[] = "0123456789abcdef";
    startgroup(os, name);
    for (size_t i = 0; i < n; i++) {
        unsigned char val = static_cast<const unsigned char*>(x)[i];
        os << hex[(val >> 4) & 0xf] << hex[val & 0xf];
        if (i < n - 1)
            os << ' ';
    }
    endgroup(os);
}

// Serializable classes

void s11n::save(std::ostream& os, const serializable& x)
{
    x.save(os);
}

void s11n::save(std::ostream& os, const char* name, const serializable& x)
{
    x.save(os, name);
}

// Basic STL types

void s11n::save(std::ostream& os, const std::string& x)
{
    size_t s = x.length();
    os.write(reinterpret_cast<const char*>(&s), sizeof(s));
    os.write(reinterpret_cast<const char*>(x.data()), s);
}

void s11n::save(std::ostream& os, const char* name, const std::string& x)
{
    os << ' ' << name << '=';
    for (size_t i = 0; i < x.length(); i++) {
        char c = x[i];
        const char* e = enc_char(c);
        if (e)
            os << e;
        else
            os << c;
    }
}


/*
 * Load a value from a stream
 */

// Fundamental arithmetic data types

void s11n::load(std::istream& is, bool& x)
{
    is.read(reinterpret_cast<char*>(&x), sizeof(x));
}

void s11n::load(const std::string& s, bool& x)
{
    x = str::to<bool>(s);
}

void s11n::load(std::istream& is, char& x)
{
    is.read(reinterpret_cast<char*>(&x), sizeof(x));
}

void s11n::load(const std::string& s, char& x)
{
    size_t i = 0;
    x = dec_char(s.c_str(), i);
}

void s11n::load(std::istream& is, signed char& x)
{
    is.read(reinterpret_cast<char*>(&x), sizeof(x));
}

void s11n::load(const std::string& s, signed char& x)
{
    x = str::to<int>(s);
}

void s11n::load(std::istream& is, unsigned char& x)
{
    is.read(reinterpret_cast<char*>(&x), sizeof(x));
}

void s11n::load(const std::string& s, unsigned char& x)
{
    x = str::to<unsigned int>(s);
}

void s11n::load(std::istream& is, short& x)
{
    is.read(reinterpret_cast<char*>(&x), sizeof(x));
}

void s11n::load(const std::string& s, short& x)
{
    x = str::to<short>(s);
}

void s11n::load(std::istream& is, unsigned short& x)
{
    is.read(reinterpret_cast<char*>(&x), sizeof(x));
}

void s11n::load(const std::string& s, unsigned short& x)
{
    x = str::to<unsigned short>(s);
}

void s11n::load(std::istream& is, int& x)
{
    is.read(reinterpret_cast<char*>(&x), sizeof(x));
}

void s11n::load(const std::string& s, int& x)
{
    x = str::to<int>(s);
}

void s11n::load(std::istream& is, unsigned int& x)
{
    is.read(reinterpret_cast<char*>(&x), sizeof(x));
}

void s11n::load(const std::string& s, unsigned int& x)
{
    x = str::to<unsigned int>(s);
}

void s11n::load(std::istream& is, long& x)
{
    is.read(reinterpret_cast<char*>(&x), sizeof(x));
}

void s11n::load(const std::string& s, long& x)
{
    x = str::to<long>(s);
}

void s11n::load(std::istream& is, unsigned long& x)
{
    is.read(reinterpret_cast<char*>(&x), sizeof(x));
}

void s11n::load(const std::string& s, unsigned long& x)
{
    x = str::to<unsigned long>(s);
}

void s11n::load(std::istream& is, long long& x)
{
    is.read(reinterpret_cast<char*>(&x), sizeof(x));
}

void s11n::load(const std::string& s, long long& x)
{
    x = str::to<long long>(s);
}

void s11n::load(std::istream& is, unsigned long long& x)
{
    is.read(reinterpret_cast<char*>(&x), sizeof(x));
}

void s11n::load(const std::string& s, unsigned long long& x)
{
    x = str::to<unsigned long long>(s);
}

void s11n::load(std::istream& is, float& x)
{
    is.read(reinterpret_cast<char*>(&x), sizeof(x));
}

void s11n::load(const std::string& s, float& x)
{
    x = str::to<float>(s);
}

void s11n::load(std::istream& is, double& x)
{
    is.read(reinterpret_cast<char*>(&x), sizeof(x));
}

void s11n::load(const std::string& s, double& x)
{
    x = str::to<double>(s);
}

void s11n::load(std::istream& is, long double& x)
{
    is.read(reinterpret_cast<char*>(&x), sizeof(x));
}

void s11n::load(const std::string& s, long double& x)
{
    x = str::to<long double>(s);
}

// Binary blobs

void s11n::load(std::istream& is, void* x, const size_t n)
{
    is.read(reinterpret_cast<char*>(x), n);
}

void s11n::load(const std::string& s, void* x, const size_t n)
{
    std::memset(x, 0, n);
    size_t i = 0;
    while (i < n && i + 3 < s.length()) {
        unsigned char val = 0;
        if (s[i] == ' ') {
            i++;
            if (s[i] >= '0' && s[i] <= '9')
                val |= (s[i] - '0') << 4;
            else if (s[i] >= 'a' && s[i] <= 'z')
                val |= (s[i] - 'a' + 10) << 4;
            i++;
            if (s[i] >= '0' && s[i] <= '9')
                val |= (s[i] - '0');
            else if (s[i] >= 'a' && s[i] <= 'z')
                val |= (s[i] - 'a' + 10);
        }
        static_cast<unsigned char*>(x)[i] = val;
    }
}

// Serializable classes

void s11n::load(std::istream& is, serializable& x)
{
    x.load(is);
}

void s11n::load(const std::string& s, serializable& x)
{
    x.load(s);
}

// Basic STL types

void s11n::load(std::istream& is, std::string& x)
{
    size_t s;
    is.read(reinterpret_cast<char*>(&s), sizeof(s));
    char* buf = new char[s];
    is.read(buf, s);
    x.assign(buf, s);
    delete[] buf;
}

void s11n::load(const std::string& s, std::string& x)
{
    x.clear();
    if (s.length() == 0)
        return;
    const char *sc = s.c_str();
    size_t i = 0;
    while (i < s.length()) {
        x.append(1, dec_char(sc, i));
    }
}
