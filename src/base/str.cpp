/*
 * Copyright (C) 2009, 2010, 2011, 2012, 2013
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
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <cmath>
#include <cerrno>
#include <limits>
#include <sstream>
#include <locale>
#include <cwchar>

#ifdef HAVE_NL_LANGINFO
# include <locale.h>
# include <langinfo.h>
#else
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
#endif

#ifdef HAVE_ICONV
# include <iconv.h>
#endif

#include "gettext.h"
#define _(string) gettext(string)

#include "dbg.h"
#include "msg.h"

#include "str.h"


#ifndef HAVE_VASPRINTF
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

/* Convert an unsigned integer to a string.
 * We hide this function so that it cannot be called with invalid arguments. */
template<typename T>
static inline std::string uint_to_str(T x)
{
    std::string s;
    do
    {
        // this assumes an ASCII-compatible character set
        s.insert(0, 1, '0' + x % 10);
        x /= 10;
    }
    while (x != 0);
    return s;
}

/* Convert a signed integer to a string.
 * We hide this function so that it cannot be called with invalid arguments. */
template<typename T>
static inline std::string int_to_str(T x)
{
    std::string s;
    bool negative = (x < 0);
    do
    {
        // this assumes an ASCII-compatible character set
        s.insert(0, 1, (negative ? ('0' - x % 10) : ('0' + x % 10)));
        x /= 10;
    }
    while (x != 0);
    if (negative)
    {
        s.insert(0, 1, '-');
    }
    return s;
}

/* Convert a floating point number (float, double, long double) to a string.
 * We hide this function so that it cannot be called with invalid arguments. */
template<typename T>
static inline std::string float_to_str(T x)
{
    std::ostringstream os;
    os.imbue(std::locale::classic());
    os.precision(std::numeric_limits<T>::digits10 + 1);
    os << x;
    return os.str();
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
        return std::string(x ? "0" : "1");
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

    /* Convert a string to one of the basic data types */

    template<typename T>
    static inline T _to(const std::string &s, const char *name)
    {
        std::istringstream is(s);
        T v;
        is >> v;
        if (is.fail() || !is.eof())
        {
            throw exc(str::asprintf(_("Cannot convert string to %s."), name), EINVAL);
        }
        return v;
    }

    template<> bool to<bool>(const std::string &s) { return _to<bool>(s, "bool"); }
    template<> signed char to<signed char>(const std::string &s) { return _to<signed char>(s, "signed char"); }
    template<> unsigned char to<unsigned char>(const std::string &s) { return _to<unsigned char>(s, "unsigned char"); }
    template<> short to<short>(const std::string &s) { return _to<short>(s, "short"); }
    template<> unsigned short to<unsigned short>(const std::string &s) { return _to<unsigned short>(s, "unsigned short"); }
    template<> int to<int>(const std::string &s) { return _to<int>(s, "int"); }
    template<> unsigned int to<unsigned int>(const std::string &s) { return _to<unsigned int>(s, "unsigned int"); }
    template<> long to<long>(const std::string &s) { return _to<long>(s, "long"); }
    template<> unsigned long to<unsigned long>(const std::string &s) { return _to<unsigned long>(s, "unsigned long"); }
    template<> long long to<long long>(const std::string &s) { return _to<long long>(s, "long long"); }
    template<> unsigned long long to<unsigned long long>(const std::string &s) { return _to<unsigned long long>(s, "unsigned long long"); }
    template<> float to<float>(const std::string &s) { return _to<float>(s, "float"); }
    template<> double to<double>(const std::string &s) { return _to<double>(s, "double"); }
    template<> long double to<long double>(const std::string &s) { return _to<long double>(s, "long double"); }

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
    template<> bool to(const std::string& s, float* x) { return _to(s, x); }
    template<> bool to(const std::string& s, double* x) { return _to(s, x); }
    template<> bool to(const std::string& s, long double* x) { return _to(s, x); }

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
    std::string &replace(std::string &str, const std::string &s, const std::string &r)
    {
        size_t s_len = s.length();
        size_t r_len = r.length();
        size_t p = 0;

        while ((p = str.find(s, p)) != std::string::npos)
        {
            str.replace(p, s_len, r);
            p += r_len;
        }
        return str;
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
        const uint8_t *buffer = static_cast<const uint8_t *>(buf);
        for (size_t i = 0; i < n; i++)
        {
            s[2 * i + 0] = hex_chars[buffer[i] >> 4];
            s[2 * i + 1] = hex_chars[buffer[i] & 0x0f];
        }

        return s;
    }

    /* Convert various values to human readable strings */

    std::string human_readable_memsize(const uintmax_t size)
    {
        const double dsize = static_cast<double>(size);
        const uintmax_t u1024 = static_cast<uintmax_t>(1024);

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

    std::string human_readable_time(int64_t microseconds)
    {
        int64_t hours = microseconds / (1000000ll * 60ll * 60ll);
        microseconds -= hours * (1000000ll * 60ll * 60ll);
        int64_t minutes = microseconds / (1000000ll * 60ll);
        microseconds -= minutes * (1000000ll * 60ll);
        int64_t seconds = microseconds / 1000000ll;
        std::string hr;
        if (hours > 0)
        {
            hr = str::from(hours) + (minutes < 10 ? ":0" : ":");
        }
        hr += str::from(minutes) + (seconds < 10 ? ":0" : ":") + str::from(seconds);
        return hr;
    }

    /* Get the name of the user's character set */
    std::string localcharset()
    {
#ifdef HAVE_NL_LANGINFO
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

#ifdef HAVE_ICONV
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
        catch (std::exception &e)
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
#ifdef HAVE_WCSWIDTH
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
