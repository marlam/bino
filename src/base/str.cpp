/*
 * Copyright (C) 2009, 2010, 2011, 2012, 2013, 2014, 2015
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

#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cmath>
#include <cerrno>
#include <cstring>
#include <limits>
#include <locale>
#include <cwchar>
#include <stdexcept>

#if HAVE_NL_LANGINFO
# include <locale.h>
# include <langinfo.h>
#else
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
#endif

#if HAVE_ICONV
# include <iconv.h>
#endif

#ifdef HAVE___INT128
# define INT128_MAX (__int128) (((unsigned __int128) 1 << 127) - 1)
# define INT128_MIN (-INT128_MAX - 1)
#endif
#ifdef HAVE_UNSIGNED___INT128
# define UINT128_MAX ((((unsigned __int128) 1 << 127) << 1) - 1)
#endif
#if !defined(LONG_DOUBLE_IS_IEEE_754_QUAD) && defined(HAVE___FLOAT128)
# include <quadmath.h>
#endif

#include "base/dbg.h"
#include "base/exc.h"
#include "base/msg.h"

#include "base/gettext.h"
#define _(string) gettext(string)

#include "base/str.h"


#if HAVE_VASPRINTF
#else
static int vasprintf(char **strp, const char *format, va_list args)
{
    /* vasprintf() is only missing on Windows nowadays.
     * This replacement function only works when the vsnprintf() function is available
     * and its return value is standards compliant. This is true for the MinGW version
     * of vsnprintf(), but not for Microsofts version (Visual Studio etc.)!
     */
    int length = vsnprintf(NULL, 0, format, args);
    if (length > std::numeric_limits<int>::max() - 1
            || !(*strp = static_cast<char *>(malloc(length + 1))))
    {
        return -1;
    }
    vsnprintf(*strp, length + 1, format, args);
    return length;
}
#endif

#if !defined(HAVE_INT128_T) && defined(HAVE___INT128)
__int128 strtoi128(const char* nptr, char** endptr, int base)
{
    // Adapted for __int128 from the following source:

    /*	$NetBSD: strtol.c,v 1.17 2005/11/29 03:12:00 christos Exp $	*/

    /*-
     * Copyright (c) 1990, 1993
     *	The Regents of the University of California.  All rights reserved.
     *
     * Redistribution and use in source and binary forms, with or without
     * modification, are permitted provided that the following conditions
     * are met:
     * 1. Redistributions of source code must retain the above copyright
     *    notice, this list of conditions and the following disclaimer.
     * 2. Redistributions in binary form must reproduce the above copyright
     *    notice, this list of conditions and the following disclaimer in the
     *    documentation and/or other materials provided with the distribution.
     * 3. Neither the name of the University nor the names of its contributors
     *    may be used to endorse or promote products derived from this software
     *    without specific prior written permission.
     *
     * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
     * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
     * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
     * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
     * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
     * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
     * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
     * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
     * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
     * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
     * SUCH DAMAGE.
     */

    /*
     * Convert a string to a long integer.
     *
     * Ignores `locale' stuff.  Assumes that the upper and lower case
     * alphabets and digits are each contiguous.
     */

    const char *s;
    __int128 acc, cutoff;
    int c;
    int neg, any, cutlim;

    /*
     * Skip white space and pick up leading +/- sign if any.
     * If base is 0, allow 0x for hex and 0 for octal, else
     * assume decimal; if base is already 16, allow 0x.
     */
    s = nptr;
    do {
        c = (unsigned char) *s++;
    } while (c == ' ' || c == '\n' || c == '\t' || c == '\r' || c == '\f' || c == '\v');
    if (c == '-') {
        neg = 1;
        c = *s++;
    } else {
        neg = 0;
        if (c == '+')
            c = *s++;
    }
    if ((base == 0 || base == 16) &&
            c == '0' && (*s == 'x' || *s == 'X')) {
        c = s[1];
        s += 2;
        base = 16;
    }
    if (base == 0)
        base = c == '0' ? 8 : 10;

    /*
     * Compute the cutoff value between legal numbers and illegal
     * numbers.  That is the largest legal value, divided by the
     * base.  An input number that is greater than this value, if
     * followed by a legal input character, is too big.  One that
     * is equal to this value may be valid or not; the limit
     * between valid and invalid numbers is then based on the last
     * digit.  For instance, if the range for longs is
     * [-2147483648..2147483647] and the input base is 10,
     * cutoff will be set to 214748364 and cutlim to either
     * 7 (neg==0) or 8 (neg==1), meaning that if we have accumulated
     * a value > 214748364, or equal but the next digit is > 7 (or 8),
     * the number is too big, and we will return a range error.
     *
     * Set any if any `digits' consumed; make it negative to indicate
     * overflow.
     */
    cutoff = neg ? INT128_MIN : INT128_MAX;
    cutlim = (int)(cutoff % base);
    cutoff /= base;
    if (neg) {
        if (cutlim > 0) {
            cutlim -= base;
            cutoff += 1;
        }
        cutlim = -cutlim;
    }
    for (acc = 0, any = 0;; c = (unsigned char) *s++) {
        if (c >= '0' && c <= '9')
            c -= '0';
        else if (c >= 'a' && c <= 'a')
            c -= 'a' - 10;
        else if (c >= 'A' && c <= 'Z')
            c -= 'A' - 10;
        else
            break;
        if (c >= base)
            break;
        if (any < 0)
            continue;
        if (neg) {
            if (acc < cutoff || (acc == cutoff && c > cutlim)) {
                any = -1;
                acc = INT128_MIN;
                errno = ERANGE;
            } else {
                any = 1;
                acc *= base;
                acc -= c;
            }
        } else {
            if (acc > cutoff || (acc == cutoff && c > cutlim)) {
                any = -1;
                acc = INT128_MAX;
                errno = ERANGE;
            } else {
                any = 1;
                acc *= base;
                acc += c;
            }
        }
    }
    if (endptr != 0)
        *endptr = const_cast<char*>(any ? s - 1 : nptr);
    return (acc);
}
#endif

#if !defined(HAVE_UINT128_T) && defined(HAVE_UNSIGNED___INT128)
unsigned __int128 strtoui128(const char* nptr, char** endptr, int base)
{
    // Adapted for __int128 from the following source:

    /*	$NetBSD: strtoul.c,v 1.17 2005/11/29 03:12:00 christos Exp $	*/

    /*
     * Copyright (c) 1990, 1993
     *	The Regents of the University of California.  All rights reserved.
     *
     * Redistribution and use in source and binary forms, with or without
     * modification, are permitted provided that the following conditions
     * are met:
     * 1. Redistributions of source code must retain the above copyright
     *    notice, this list of conditions and the following disclaimer.
     * 2. Redistributions in binary form must reproduce the above copyright
     *    notice, this list of conditions and the following disclaimer in the
     *    documentation and/or other materials provided with the distribution.
     * 3. Neither the name of the University nor the names of its contributors
     *    may be used to endorse or promote products derived from this software
     *    without specific prior written permission.
     *
     * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
     * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
     * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
     * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
     * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
     * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
     * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
     * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
     * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
     * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
     * SUCH DAMAGE.
     */

    /*
     * Convert a string to an unsigned long integer.
     *
     * Ignores `locale' stuff.  Assumes that the upper and lower case
     * alphabets and digits are each contiguous.
     */

    const char *s;
    unsigned __int128 acc, cutoff;
    int c;
    int neg, any, cutlim;

    /*
     * See strtol for comments as to the logic used.
     */
    s = nptr;
    do {
        c = (unsigned char) *s++;
    } while (c == ' ' || c == '\n' || c == '\t' || c == '\r' || c == '\f' || c == '\v');
    if (c == '-') {
        neg = 1;
        c = *s++;
    } else {
        neg = 0;
        if (c == '+')
            c = *s++;
    }
    if ((base == 0 || base == 16) &&
            c == '0' && (*s == 'x' || *s == 'X')) {
        c = s[1];
        s += 2;
        base = 16;
    }
    if (base == 0)
        base = c == '0' ? 8 : 10;

    cutoff = UINT128_MAX / base;
    cutlim = (int)(UINT128_MAX % base);
    for (acc = 0, any = 0;; c = (unsigned char) *s++) {
        if (c >= base)
            break;
        if (c >= '0' && c <= '9')
            c -= '0';
        else if (c >= 'a' && c <= 'a')
            c -= 'a' - 10;
        else if (c >= 'A' && c <= 'Z')
            c -= 'A' - 10;
        else
            break;
        if (any < 0)
            continue;
        if (acc > cutoff || (acc == cutoff && c > cutlim)) {
            any = -1;
            acc = UINT128_MAX;
            errno = ERANGE;
        } else {
            any = 1;
            acc *= (unsigned long)base;
            acc += c;
        }
    }
    if (neg && any > 0)
        acc = -acc;
    if (endptr != 0)
        *endptr = const_cast<char*>(any ? s - 1 : nptr);
    return (acc);
}
#endif

/* Convert an unsigned integer to a string.
 * We hide this function so that it cannot be called with invalid arguments. */
template<typename T>
static inline std::string uint_to_str(T x)
{
    size_t size = sizeof(T) * 8 / 3 + 1;
    char buf[size];
    int i = size;
    do {
        buf[--i] = '0' + x % 10; // this assumes an ASCII-compatible character set
        x /= 10;
    } while (x != 0);
    return std::string(buf + i, size - i);
}

/* Convert a signed integer to a string.
 * We hide this function so that it cannot be called with invalid arguments. */
template<typename T>
static inline std::string int_to_str(T x)
{
    size_t size = sizeof(T) * 8 / 3 + 1 + 1 /* sign */;
    char buf[size];
    int i = size;
    if (x < 0) {
        do {
            buf[--i] = '0' - x % 10; // this assumes an ASCII-compatible character set
            x /= 10;
        } while (x != 0);
        buf[--i] = '-';
    } else {
        do {
            buf[--i] = '0' + x % 10; // this assumes an ASCII-compatible character set
            x /= 10;
        } while (x != 0);
    }
    return std::string(buf + i, size - i);
}

/* Convert a floating point representation to a string.
 * We hide this function so that it cannot be called with invalid arguments. */
template<typename T> static int float_startbufsize() { return 1 << 30; /* will never be called */ }
template<> int float_startbufsize<float>() { return 32; }
template<> int float_startbufsize<double>() { return 64; }
template<> int float_startbufsize<long double>() { return 128; }
#if !defined(LONG_DOUBLE_IS_IEEE_754_QUAD) && defined(HAVE___FLOAT128)
template<> int float_startbufsize<__float128>() { return 128; }
#endif
template<typename T> static int float_snprintf(char*, size_t, T) { return -1; /* will never be called */ }
template<> int float_snprintf<float>(char* str, size_t size, float x) { return snprintf(str, size, "%.9g", x); }
template<> int float_snprintf<double>(char* str, size_t size, double x) { return snprintf(str, size, "%.17g", x); }
template<> int float_snprintf<long double>(char* str, size_t size, long double x) { return snprintf(str, size, "%.36Lg", x); }
#if !defined(LONG_DOUBLE_IS_IEEE_754_QUAD) && defined(HAVE___FLOAT128)
template<> int float_snprintf<__float128>(char* str, size_t size, __float128 x) { return quadmath_snprintf(str, size, "%.36Qg", x); }
#endif
template<typename T>
static inline std::string float_to_str(T x)
{
    std::string localebak = std::string(setlocale(LC_NUMERIC, NULL));
    setlocale(LC_NUMERIC, "C");

    std::string result;
    const int startsize = float_startbufsize<T>();
    char buf[startsize];
    int length = float_snprintf(buf, startsize, x);
    if (length < startsize) {
        result = std::string(buf, length);
    } else {
        std::vector<char> buf2(length + 1);
        length = float_snprintf<T>(&buf2[0], length + 1, x);
        result = std::string(&buf2[0], length);
    }

    setlocale(LC_NUMERIC, localebak.c_str());
    return result;
}

/* Read a number from a string.
 * We hide this function so that it cannot be called with invalid arguments. */
template<typename T> static T str_to_small_int(const char* nptr, char** endptr, int base)
{
    long x = std::strtol(nptr, endptr, base);
    if (std::numeric_limits<T>::is_signed && x < std::numeric_limits<T>::min()) {
        x = std::numeric_limits<T>::min();
        errno = ERANGE;
    } else if (x > std::numeric_limits<T>::max()) {
        x = std::numeric_limits<T>::max();
        errno = ERANGE;
    }
    return x;
}
template<typename T> static T strtox(const char*, char**, int) { return 0; /* will never be called */ }
template<> signed char strtox<signed char>(const char* nptr, char** endptr, int base) { return str_to_small_int<signed char>(nptr, endptr, base); }
template<> unsigned char strtox<unsigned char>(const char* nptr, char** endptr, int base) { return str_to_small_int<unsigned char>(nptr, endptr, base); }
template<> short strtox<short>(const char* nptr, char** endptr, int base) { return str_to_small_int<short>(nptr, endptr, base); }
template<> unsigned short strtox<unsigned short>(const char* nptr, char** endptr, int base) { return str_to_small_int<unsigned short>(nptr, endptr, base); }
template<> int strtox<int>(const char* nptr, char** endptr, int base) { return str_to_small_int<int>(nptr, endptr, base); }
template<> unsigned int strtox<unsigned int>(const char* nptr, char** endptr, int base) { return str_to_small_int<unsigned int>(nptr, endptr, base); }
template<> long strtox<long>(const char* nptr, char** endptr, int base) { return std::strtol(nptr, endptr, base); }
template<> unsigned long strtox<unsigned long>(const char* nptr, char** endptr, int base) { return std::strtoul(nptr, endptr, base); }
template<> long long strtox<long long>(const char* nptr, char** endptr, int base) { return std::strtoll(nptr, endptr, base); }
template<> unsigned long long strtox<unsigned long long>(const char* nptr, char** endptr, int base) { return std::strtoull(nptr, endptr, base); }
#if !defined(HAVE_INT128_T) && defined(HAVE___INT128)
template<> __int128 strtox<__int128>(const char* nptr, char** endptr, int base) { return strtoi128(nptr, endptr, base); }
#endif
#if !defined(HAVE_UINT128_T) && defined(HAVE_UNSIGNED___INT128)
template<> unsigned __int128 strtox<unsigned __int128>(const char* nptr, char** endptr, int base) { return strtoui128(nptr, endptr, base); }
#endif
template<> float strtox<float>(const char* nptr, char** endptr, int /* base */) { return std::strtof(nptr, endptr); }
template<> double strtox<double>(const char* nptr, char** endptr, int /* base */) { return std::strtod(nptr, endptr); }
template<> long double strtox<long double>(const char* nptr, char** endptr, int /* base */) { return std::strtold(nptr, endptr); }
#if !defined(LONG_DOUBLE_IS_IEEE_754_QUAD) && defined(HAVE___FLOAT128)
template<> __float128 strtox<__float128>(const char* nptr, char** endptr, int /* base */) { return strtoflt128(nptr, endptr); }
#endif
template<typename T>
static inline T str_to(const std::string& s, const char* t)
{
    T r;
    const char* str = s.c_str();
    char* p;
    int errnobak = errno;
    errno = 0;
    std::string localebak = std::string(setlocale(LC_NUMERIC, NULL));
    setlocale(LC_NUMERIC, "C");
    r = strtox<T>(str, &p, 0);
    setlocale(LC_NUMERIC, localebak.c_str());
    if (p == str || errno == ERANGE || *p != '\0') {
        errno = errnobak;
        throw std::invalid_argument(str::asprintf(_("Cannot convert string to %s."), t));
    }
    errno = errnobak;
    return r;
}

namespace str
{
    /* Sanitize a string (replace control characters with '?') */

    std::string sanitize(const std::string &s)
    {
        std::string sane(s);
        for (size_t i = 0; i < sane.length(); i++)
        {
            if (iscntrl(static_cast<unsigned char>(sane[i])))
            {
                sane[i] = '?';
            }
        }
        return sane;
    }

    /* Trim a string (remove whitespace from both ends) */

    std::string trim(const std::string &s)
    {
        const char whitespace[] = " \t\v\f\n\r";
        size_t first = s.find_first_not_of(whitespace);
        if (first != std::string::npos)
        {
            size_t last = s.find_last_not_of(whitespace);
            return s.substr(first, last - first + 1);
        }
        else
        {
            return std::string();
        }
    }

    /* Parse a string into tokens */

    std::vector<std::string> tokens(const std::string &s, const std::string &delimiters)
    {
        std::vector<std::string> t;
        size_t index = 0;
        for (;;)
        {
            size_t start = s.find_first_not_of(delimiters, index);
            if (start != std::string::npos)
            {
                size_t end = s.find_first_of(delimiters, start);
                if (end == std::string::npos)
                {
                    t.push_back(s.substr(start));
                    break;
                }
                else
                {
                    t.push_back(s.substr(start, end - start));
                    index = end;
                }
            }
            else
            {
                break;
            }
        }
        return t;
    }

    /* Create std::strings from all basic data types */

    std::string from(bool x)
    {
        return std::string(1, x ? '1' : '0');
    }

    std::string from(signed char x)
    {
        return int_to_str(x);
    }

    std::string from(unsigned char x)
    {
        return uint_to_str(x);
    }

    std::string from(short x)
    {
        return int_to_str(x);
    }

    std::string from(unsigned short x)
    {
        return uint_to_str(x);
    }

    std::string from(int x)
    {
        return int_to_str(x);
    }

    std::string from(unsigned int x)
    {
        return uint_to_str(x);
    }

    std::string from(long x)
    {
        return int_to_str(x);
    }

    std::string from(unsigned long x)
    {
        return uint_to_str(x);
    }

    std::string from(long long x)
    {
        return int_to_str(x);
    }

    std::string from(unsigned long long x)
    {
        return uint_to_str(x);
    }

#if !defined(HAVE_INT128_T) && defined(HAVE___INT128)
    std::string from(__int128 x)
    {
        return int_to_str(x);
    }
#endif

#if !defined(HAVE_UINT128_T) && defined(HAVE_UNSIGNED___INT128)
    std::string from(unsigned __int128 x)
    {
        return uint_to_str(x);
    }
#endif

    std::string from(float x)
    {
        return float_to_str(x);
    }

    std::string from(double x)
    {
        return float_to_str(x);
    }

    std::string from(long double x)
    {
        return float_to_str(x);
    }

#if !defined(LONG_DOUBLE_IS_IEEE_754_QUAD) && defined(HAVE___FLOAT128)
    std::string from(__float128 x)
    {
        return float_to_str(x);
    }
#endif

    /* Convert a string to one of the basic data types */

    template<> bool to<bool>(const std::string &s)
    {
        long r = str_to<long>(s, "bool");
        return r ? true : false;
    }
    template<> signed char to<signed char>(const std::string &s)
    {
        return str_to<signed char>(s, "signed char");
    }
    template<> unsigned char to<unsigned char>(const std::string &s)
    {
        return str_to<unsigned char>(s, "unsigned char");
    }
    template<> short to<short>(const std::string &s)
    {
        return str_to<short>(s, "short");
    }
    template<> unsigned short to<unsigned short>(const std::string &s)
    {
        return str_to<unsigned short>(s, "unsigned short");
    }
    template<> int to<int>(const std::string &s)
    {
        return str_to<int>(s, "int");
    }
    template<> unsigned int to<unsigned int>(const std::string &s)
    {
        return str_to<unsigned int>(s, "unsigned int");
    }
    template<> long to<long>(const std::string &s)
    {
        return str_to<long>(s, "long");
    }
    template<> unsigned long to<unsigned long>(const std::string &s)
    {
        return str_to<unsigned long>(s, "unsigned long");
    }
    template<> long long to<long long>(const std::string &s)
    {
        return str_to<long long>(s, "long long");
    }
    template<> unsigned long long to<unsigned long long>(const std::string &s)
    {
        return str_to<unsigned long long>(s, "unsigned long long");
    }
#if !defined(HAVE_INT128_T) && defined(HAVE___INT128)
    template<> __int128 to<__int128>(const std::string &s)
    {
        return str_to<__int128>(s, "int128");
    }
#endif
#if !defined(HAVE_UINT128_T) && defined(HAVE_UNSIGNED___INT128)
    template<> unsigned __int128 to<unsigned __int128>(const std::string &s)
    {
        return str_to<unsigned __int128>(s, "unsigned int128");
    }
#endif
    template<> float to<float>(const std::string &s)
    {
        return str_to<float>(s, "float");
    }
    template<> double to<double>(const std::string &s)
    {
        return str_to<double>(s, "double");
    }
    template<> long double to<long double>(const std::string &s)
    {
        return str_to<long double>(s, "long double");
    }
#if !defined(LONG_DOUBLE_IS_IEEE_754_QUAD) && defined(HAVE___FLOAT128)
    template<> __float128 to<__float128>(const std::string &s)
    {
        return str_to<__float128>(s, "float128");
    }
#endif

    template<typename T>
    static inline bool _to(const std::string& s, T* x)
    {
        T y;
        try {
            y = str::to<T>(s);
        }
        catch (...) {
            return false;
        }
        *x = y;
        return true;
    }

    template<> bool to(const std::string& s, bool* x) { return _to(s, x); }
    template<> bool to(const std::string& s, signed char* x) { return _to(s, x); }
    template<> bool to(const std::string& s, unsigned char* x) { return _to(s, x); }
    template<> bool to(const std::string& s, short* x) { return _to(s, x); }
    template<> bool to(const std::string& s, unsigned short* x) { return _to(s, x); }
    template<> bool to(const std::string& s, int* x) { return _to(s, x); }
    template<> bool to(const std::string& s, unsigned int* x) { return _to(s, x); }
    template<> bool to(const std::string& s, long* x) { return _to(s, x); }
    template<> bool to(const std::string& s, unsigned long* x) { return _to(s, x); }
    template<> bool to(const std::string& s, long long* x) { return _to(s, x); }
    template<> bool to(const std::string& s, unsigned long long* x) { return _to(s, x); }
#if !defined(HAVE_INT128_T) && defined(HAVE___INT128)
    template<> bool to(const std::string& s, __int128* x) { return _to(s, x); }
#endif
#if !defined(HAVE_UINT128_T) && defined(HAVE_UNSIGNED___INT128)
    template<> bool to(const std::string& s, unsigned __int128* x) { return _to(s, x); }
#endif
    template<> bool to(const std::string& s, float* x) { return _to(s, x); }
    template<> bool to(const std::string& s, double* x) { return _to(s, x); }
    template<> bool to(const std::string& s, long double* x) { return _to(s, x); }
#if !defined(LONG_DOUBLE_IS_IEEE_754_QUAD) && defined(HAVE___FLOAT128)
    template<> bool to(const std::string& s, __float128* x) { return _to(s, x); }
#endif

    /* Create std::strings printf-like */

    std::string vasprintf(const char *format, va_list args)
    {
        char *cstr;
        if (::vasprintf(&cstr, format, args) < 0)
        {
            /* Should never happen (out of memory or some invalid conversions).
             * We do not want to throw an exception because this function might
             * be called because of an exception. Inform the user instead. */
            msg::err("Failure in str::vasprintf().");
            dbg::crash();
        }
        std::string s(cstr);
        free(cstr);
        return s;
    }

    std::string asprintf(const char *format, ...)
    {
        std::string s;
        va_list args;
        va_start(args, format);
        s = vasprintf(format, args);
        va_end(args);
        return s;
    }

    /* Replace all occurences */

    /**
     * \param str       The string in which the replacements will be made.
     * \param s         The string that will be replaced.
     * \param r         The replacement for the string s.
     * \return          The resulting string.
     *
     * Replaces all occurences of \a s in \a str with \a r.
     * Returns \a str.
     */
    std::string replace(const std::string &str, const std::string &s, const std::string &r)
    {
        std::string ts(str);
        size_t s_len = s.length();
        size_t r_len = r.length();
        size_t p = 0;

        while ((p = ts.find(s, p)) != std::string::npos)
        {
            ts.replace(p, s_len, r);
            p += r_len;
        }
        return ts;
    }

    /* Create a hex string from binary data */

    std::string hex(const std::string &s, bool uppercase)
    {
        return hex(reinterpret_cast<const void *>(s.c_str()), s.length(), uppercase);
    }

    std::string hex(const void *buf, size_t n, bool uppercase)
    {
        static const char hex_chars_lower[] = "0123456789abcdef";
        static const char hex_chars_upper[] = "0123456789ABCDEF";

        std::string s;
        s.resize(2 * n);

        const char *hex_chars = uppercase ? hex_chars_upper : hex_chars_lower;
        const unsigned char *buffer = static_cast<const unsigned char *>(buf);
        for (size_t i = 0; i < n; i++)
        {
            s[2 * i + 0] = hex_chars[buffer[i] >> 4];
            s[2 * i + 1] = hex_chars[buffer[i] & 0x0f];
        }

        return s;
    }

    /* Convert various values to human readable strings */

    std::string human_readable_memsize(const unsigned long long size)
    {
        const double dsize = static_cast<double>(size);
        const unsigned long long u1024 = static_cast<unsigned long long>(1024);

        if (size >= u1024 * u1024 * u1024 * u1024)
        {
            return str::asprintf("%.2f TiB", dsize / static_cast<double>(u1024 * u1024 * u1024 * u1024));
        }
        else if (size >= u1024 * u1024 * u1024)
        {
            return str::asprintf("%.2f GiB", dsize / static_cast<double>(u1024 * u1024 * u1024));
        }
        else if (size >= u1024 * u1024)
        {
            return str::asprintf("%.2f MiB", dsize / static_cast<double>(u1024 * u1024));
        }
        else if (size >= u1024)
        {
            return str::asprintf("%.2f KiB", dsize / static_cast<double>(u1024));
        }
        else if (size > 1 || size == 0)
        {
            return str::asprintf("%d bytes", static_cast<int>(size));
        }
        else
        {
            return std::string("1 byte");
        }
    }

    std::string human_readable_length(const double length)
    {
        const double abslength = std::abs(length);
        if (abslength >= 1000.0)
        {
            return str::asprintf("%.1f km", length / 1000.0);
        }
        else if (abslength >= 1.0)
        {
            return str::asprintf("%.1f m", length);
        }
        else if (abslength >= 0.01)
        {
            return str::asprintf("%.1f cm", length * 100.0);
        }
        else if (abslength <= 0.0)
        {
            return std::string("0 m");
        }
        else
        {
            return str::asprintf("%.1f mm", length * 1000.0);
        }
    }

    std::string human_readable_geodetic(double lat, double lon, double elev)
    {
        std::string s = str::asprintf("lat %.6f lon %.6f",
                180.0 / M_PI * lat, 180.0 / M_PI * lon);
        if (elev < 0.0 || elev > 0.0)
            s += " elev " + human_readable_length(elev);
        return s;
    }

    std::string human_readable_time(long long microseconds)
    {
        long long hours = microseconds / (1000000ll * 60ll * 60ll);
        microseconds -= hours * (1000000ll * 60ll * 60ll);
        long long minutes = microseconds / (1000000ll * 60ll);
        microseconds -= minutes * (1000000ll * 60ll);
        long long seconds = microseconds / 1000000ll;
        std::string hr;
        if (hours > 0)
        {
            hr = str::from(hours) + (minutes < 10 ? ":0" : ":");
        }
        hr += str::from(minutes) + (seconds < 10 ? ":0" : ":") + str::from(seconds);
        return hr;
    }

    /* Create an RFC2822-style time string, like "Fri,  4 Dec 2009 22:29:43 +0100 (CET)" */
    /* Code taken from mpop 1.0.29:
     * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011
     * Martin Lambers <marlam@marlam.de>
     * License: GPLv3+
     */
    std::string rfc2822_time(time_t t)
    {
        struct tm gmt, *lt;
        char tz_offset_sign;
        int tz_offset_hours;
        int tz_offset_minutes;
        const char* weekday[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
        const char* month[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
            "Aug", "Sep", "Oct", "Nov", "Dec" };
        char rfc2822_timestamp[32];

        /* Calculate a RFC 2822 timestamp. strftime() is unreliable for this because
         * it is locale dependant, and because the timezone offset conversion
         * specifier %z is not portable. */
        /* copy the struct tm, because the subsequent call to localtime() will
         * overwrite it */
        gmt = *gmtime(&t);
        lt = localtime(&t);
        tz_offset_minutes = (lt->tm_hour - gmt.tm_hour) * 60
            + lt->tm_min - gmt.tm_min
            + (lt->tm_year - gmt.tm_year) * 24 * 60
            + (lt->tm_yday - gmt.tm_yday) * 24 * 60;
        if (tz_offset_minutes < 0)
        {
            tz_offset_sign = '-';
            tz_offset_minutes = -tz_offset_minutes;
        }
        else
        {
            tz_offset_sign = '+';
        }
        tz_offset_hours = tz_offset_minutes / 60;
        tz_offset_minutes %= 60;
        if (tz_offset_hours > 99)
        {
            /* Values equal to or larger than 24 are not meaningful, but we just
             * make sure that the value fits into two digits. If the system time is
             * broken, we cannot fix it. */
            tz_offset_hours = 99;
        }
        snprintf(rfc2822_timestamp, sizeof(rfc2822_timestamp),
                "%s, %02d %s %04d %02d:%02d:%02d %c%02d%02d",
                weekday[lt->tm_wday], lt->tm_mday, month[lt->tm_mon],
                lt->tm_year + 1900, lt->tm_hour, lt->tm_min, lt->tm_sec,
                tz_offset_sign, tz_offset_hours, tz_offset_minutes);
        return std::string(rfc2822_timestamp);
    }

    /* Get the name of the user's character set */
    std::string localcharset()
    {
#if HAVE_NL_LANGINFO
        std::string bak = setlocale(LC_CTYPE, NULL);
        setlocale(LC_CTYPE, "");
        char *charset = nl_langinfo(CODESET);
        setlocale(LC_CTYPE, bak.c_str());
#else
        char charset[2 + 10 + 1];
        snprintf(charset, 2 + 10 + 1, "CP%u", GetACP());
        /* Another instance of incredibly braindead Windows design.
         * We need to return the active code page to get correct results when
         * output goes into files or pipes. But the console output codepage is
         * not related to the active code page. So we force it to be the same.
         * but this only works if the console window uses a true type font;
         * raster fonts ignore the console output codepage.
         * If you find this too stupid to believe, read Microsofts documentation
         * of the SetConsoleOutputCP function. */
        SetConsoleOutputCP(GetACP());
#endif
        return std::string(charset);
    }

    /* Convert a string from one character set to another */

    std::string convert(const std::string &src, const std::string &from_charset, const std::string &to_charset)
    {
        if (from_charset.compare(to_charset) == 0)
        {
            return src;
        }

#if HAVE_ICONV
        iconv_t cd = iconv_open(to_charset.c_str(), from_charset.c_str());
        if (cd == reinterpret_cast<iconv_t>(static_cast<size_t>(-1)))
        {
            throw exc(str::asprintf(_("Cannot convert %s to %s: %s"),
                        from_charset.c_str(), to_charset.c_str(), std::strerror(errno)), errno);
        }

        size_t inbytesleft = src.length() + 1;
        const char *inbuf = src.c_str();
        size_t outbytesleft = inbytesleft;
        if (outbytesleft >= std::numeric_limits<size_t>::max() / 4)
        {
            outbytesleft = std::numeric_limits<size_t>::max();
        }
        else
        {
            outbytesleft = outbytesleft * 4;
        }
        char *orig_outbuf = static_cast<char *>(malloc(outbytesleft));
        if (!orig_outbuf)
        {
            iconv_close(cd);
            throw exc(str::asprintf(_("Cannot convert %s to %s: %s"),
                        from_charset.c_str(), to_charset.c_str(), std::strerror(ENOMEM)), ENOMEM);
        }
        char *outbuf = orig_outbuf;

        size_t s = iconv(cd, const_cast<ICONV_CONST char **>(&inbuf), &inbytesleft, &outbuf, &outbytesleft);
        int saved_errno = errno;
        iconv_close(cd);
        if (s == static_cast<size_t>(-1))
        {
            free(orig_outbuf);
            throw exc(str::asprintf(_("Cannot convert %s to %s: %s"),
                        from_charset.c_str(), to_charset.c_str(), std::strerror(saved_errno)), saved_errno);
        }

        std::string dst;
        try
        {
            dst.assign(orig_outbuf);
        }
        catch (...)
        {
            free(orig_outbuf);
            throw exc(str::asprintf(_("Cannot convert %s to %s: %s"),
                        from_charset.c_str(), to_charset.c_str(), std::strerror(ENOMEM)), ENOMEM);
        }
        free(orig_outbuf);
        return dst;
#else
        throw exc(str::asprintf(_("Cannot convert %s to %s."),
                    from_charset.c_str(), to_charset.c_str(), std::strerror(ENOSYS)), ENOSYS);
#endif
    }

    std::wstring to_wstr(const std::string &in)
    {
        std::wstring out;
        size_t l = std::mbstowcs(NULL, in.c_str(), 0);
        if (l == static_cast<size_t>(-1) || l > static_cast<size_t>(std::numeric_limits<int>::max() - 1)) {
            // This should never happen. We don't want to handle this case, and we don't want to throw
            // an exception from a str function, so inform the user and abort.
            msg::err("Failure in str::to_wstr().");
            dbg::crash();
        }
        out.resize(l);
        std::mbstowcs(&(out[0]), in.c_str(), l);
        return out;
    }

    size_t display_width(const std::wstring& ws)
    {
#if HAVE_WCSWIDTH
        return std::max(0, ::wcswidth(ws.c_str(), ws.length()));
#else
        return ws.length();
#endif
    }

    size_t display_width(const std::string& s)
    {
        return display_width(to_wstr(s));
    }
}
