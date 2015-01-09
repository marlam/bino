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

#include <limits>
#include <cstdlib>
#include <cstdio>

#include "base/dbg.h"
#include "base/str.h"
#include "base/msg.h"


namespace msg
{
    static FILE *_file = stderr;
    static level_t _level = WRN;
    static int _columns = 80;
    static std::string _program_name("");
    static std::string _category_name("");

    /* Get / Set configuration */

    FILE *file()
    {
        return _file;
    }

    void set_file(FILE *f)
    {
        _file = f;
        std::setvbuf(f, NULL, _IOLBF, 0);
    }

    level_t level()
    {
        return _level;
    }

    void set_level(level_t l)
    {
#ifdef NDEBUG
        _level = (l == msg::DBG ? msg::INF : l);
#else
        _level = l;
#endif
    }

    int columns()
    {
        return _columns;
    }

    void set_columns(int l)
    {
        _columns = l;
    }

    void set_columns_from_env()
    {
        char *s;
        if ((s = ::getenv("COLUMNS")))
        {
            int c = ::atoi(s);
            if (c > 0)
            {
                set_columns(c);
            }
        }
    }

    std::string program_name()
    {
        return _program_name;
    }

    void set_program_name(const std::string &n)
    {
        _program_name = n;
    }

    std::string category_name()
    {
        return _category_name;
    }

    void set_category_name(const std::string &n)
    {
        _category_name = n;
    }

    /* Print messages */

    static std::string prefix(level_t level)
    {
        static const char *_level_prefixes[] = { "[dbg] ", "[inf] ", "[wrn] ", "[err] ", "" };

        if (!_program_name.empty() && !_category_name.empty())
        {
            return _program_name + ": " + _level_prefixes[level] + _category_name + ": ";
        }
        else if (!_program_name.empty() && _category_name.empty())
        {
            return _program_name + ": " + _level_prefixes[level];
        }
        else if (_program_name.empty() && !_category_name.empty())
        {
            return _level_prefixes[level] + _category_name + ": ";
        }
        else
        {
            return _level_prefixes[level];
        }
    }

    void msg(int indent, level_t level, const std::string &s)
    {
        if (level < _level)
        {
            return;
        }

        std::string out = prefix(level) + std::string(indent, ' ') + s.c_str() + '\n';
        std::fputs(out.c_str(), _file);
    }

    void msg(int indent, level_t level, const char *format, ...)
    {
        if (level < _level)
        {
            return;
        }

        std::string s;
        va_list args;
        va_start(args, format);
        s = str::vasprintf(format, args);
        va_end(args);
        msg(indent, level, s);
    }

    void msg_txt(int indent, level_t level, const std::string &s)
    {
        if (level < _level)
        {
            return;
        }

        std::wstring pfx = str::to_wstr(prefix(level) + std::string(indent, ' '));
        size_t pfx_dw = str::display_width(pfx);
        std::wstring text = str::to_wstr(s);

        std::wstring out;
        size_t line_dw = 0;
        int first_unprinted = 0;
        int last_blank = -1;
        bool end_of_text = false;
        for (int text_index = 0; !end_of_text; text_index++)
        {
            if (text[text_index] == L'\0')
            {
                text[text_index] = L'\n';
                end_of_text = true;
            }
            if (text[text_index] == L'\n')
            {
                // output from first_unprinted to text_index
                out += pfx;
                out += std::wstring(text.c_str() + first_unprinted, text_index - first_unprinted + 1);
                first_unprinted = text_index + 1;
                last_blank = -1;
                line_dw = 0;
            }
            else
            {
                if (text[text_index] == L' ' || text[text_index] == L'\t')
                {
                    last_blank = text_index;
                }
                if (line_dw >= _columns - pfx_dw)
                {
                    // output from first_unprinted to last_blank (which is replaced
                    // by '\n'), then update first_unprinted.
                    if (last_blank == -1)
                    {
                        // word is too long for line; search next blank and use that
                        do
                        {
                            text_index++;
                        }
                        while (text[text_index] != L' '
                                && text[text_index] != L'\t'
                                && text[text_index] != L'\n'
                                && text[text_index] != L'\0');
                        if (text[text_index] == L'\0')
                        {
                            end_of_text = true;
                        }
                        last_blank = text_index;
                    }
                    text[last_blank] = L'\n';
                    out += pfx;
                    out += std::wstring(text.c_str() + first_unprinted, last_blank - first_unprinted + 1);
                    first_unprinted = last_blank + 1;
                    last_blank = -1;
                    line_dw = str::display_width(text.substr(first_unprinted, text_index - first_unprinted + 1));
                }
                else
                {
                    line_dw += str::display_width(text.substr(text_index, 1));
                }
            }
        }
        std::fprintf(_file, "%ls", &(out[0]));
    }

    void msg_txt(int indent, level_t level, const char *format, ...)
    {
        if (level < _level)
        {
            return;
        }

        std::string s;
        va_list args;
        va_start(args, format);
        s = str::vasprintf(format, args);
        va_end(args);
        msg_txt(indent, level, s);
    }

    void msg(level_t level, const std::string &s)
    {
        if (level < _level)
        {
            return;
        }

        msg(0, level, s);
    }

    void msg(level_t level, const char *format, ...)
    {
        if (level < _level)
        {
            return;
        }

        std::string s;
        va_list args;
        va_start(args, format);
        s = str::vasprintf(format, args);
        va_end(args);
        msg(0, level, s);
    }

    void msg_txt(level_t level, const std::string &s)
    {
        if (level < _level)
        {
            return;
        }

        msg_txt(0, level, s);
    }

    void msg_txt(level_t level, const char *format, ...)
    {
        if (level < _level)
        {
            return;
        }

        std::string s;
        va_list args;
        va_start(args, format);
        s = str::vasprintf(format, args);
        va_end(args);
        msg_txt(0, level, s);
    }

#ifndef NDEBUG
    void dbg(int indent, const std::string &s)
    {
        if (DBG < _level)
        {
            return;
        }

        msg(indent, DBG, s);
    }

    void dbg(int indent, const char *format, ...)
    {
        if (DBG < _level)
        {
            return;
        }

        std::string s;
        va_list args;
        va_start(args, format);
        s = str::vasprintf(format, args);
        va_end(args);
        msg(indent, DBG, s);
    }

    void dbg_txt(int indent, const std::string &s)
    {
        if (DBG < _level)
        {
            return;
        }

        msg_txt(indent, DBG, s);
    }

    void dbg_txt(int indent, const char *format, ...)
    {
        if (DBG < _level)
        {
            return;
        }

        std::string s;
        va_list args;
        va_start(args, format);
        s = str::vasprintf(format, args);
        va_end(args);
        msg_txt(indent, DBG, s);
    }

    void dbg(const std::string &s)
    {
        if (DBG < _level)
        {
            return;
        }

        msg(0, DBG, s);
    }

    void dbg(const char *format, ...)
    {
        if (DBG < _level)
        {
            return;
        }

        std::string s;
        va_list args;
        va_start(args, format);
        s = str::vasprintf(format, args);
        va_end(args);
        msg(0, DBG, s);
    }

    void dbg_txt(const std::string &s)
    {
        if (DBG < _level)
        {
            return;
        }

        msg_txt(0, DBG, s);
    }

    void dbg_txt(const char *format, ...)
    {
        if (DBG < _level)
        {
            return;
        }

        std::string s;
        va_list args;
        va_start(args, format);
        s = str::vasprintf(format, args);
        va_end(args);
        msg_txt(0, DBG, s);
    }
#endif

    void inf(int indent, const std::string &s)
    {
        if (INF < _level)
        {
            return;
        }

        msg(indent, INF, s);
    }

    void inf(int indent, const char *format, ...)
    {
        if (INF < _level)
        {
            return;
        }

        std::string s;
        va_list args;
        va_start(args, format);
        s = str::vasprintf(format, args);
        va_end(args);
        msg(indent, INF, s);
    }

    void inf_txt(int indent, const std::string &s)
    {
        if (INF < _level)
        {
            return;
        }

        msg_txt(indent, INF, s);
    }

    void inf_txt(int indent, const char *format, ...)
    {
        if (INF < _level)
        {
            return;
        }

        std::string s;
        va_list args;
        va_start(args, format);
        s = str::vasprintf(format, args);
        va_end(args);
        msg_txt(indent, INF, s);
    }

    void inf(const std::string &s)
    {
        if (INF < _level)
        {
            return;
        }

        msg(0, INF, s);
    }

    void inf(const char *format, ...)
    {
        if (INF < _level)
        {
            return;
        }

        std::string s;
        va_list args;
        va_start(args, format);
        s = str::vasprintf(format, args);
        va_end(args);
        msg(0, INF, s);
    }

    void inf_txt(const std::string &s)
    {
        if (INF < _level)
        {
            return;
        }

        msg_txt(0, INF, s);
    }

    void inf_txt(const char *format, ...)
    {
        if (INF < _level)
        {
            return;
        }

        std::string s;
        va_list args;
        va_start(args, format);
        s = str::vasprintf(format, args);
        va_end(args);
        msg_txt(0, INF, s);
    }

    void wrn(int indent, const std::string &s)
    {
        if (WRN < _level)
        {
            return;
        }

        msg(indent, WRN, s);
    }

    void wrn(int indent, const char *format, ...)
    {
        if (WRN < _level)
        {
            return;
        }

        std::string s;
        va_list args;
        va_start(args, format);
        s = str::vasprintf(format, args);
        va_end(args);
        msg(indent, WRN, s);
    }

    void wrn_txt(int indent, const std::string &s)
    {
        if (WRN < _level)
        {
            return;
        }

        msg_txt(indent, WRN, s);
    }

    void wrn_txt(int indent, const char *format, ...)
    {
        if (WRN < _level)
        {
            return;
        }

        std::string s;
        va_list args;
        va_start(args, format);
        s = str::vasprintf(format, args);
        va_end(args);
        msg_txt(indent, WRN, s);
    }

    void wrn(const std::string &s)
    {
        if (WRN < _level)
        {
            return;
        }

        msg(0, WRN, s);
    }

    void wrn(const char *format, ...)
    {
        if (WRN < _level)
        {
            return;
        }

        std::string s;
        va_list args;
        va_start(args, format);
        s = str::vasprintf(format, args);
        va_end(args);
        msg(0, WRN, s);
    }

    void wrn_txt(const std::string &s)
    {
        if (WRN < _level)
        {
            return;
        }

        msg_txt(0, WRN, s);
    }

    void wrn_txt(const char *format, ...)
    {
        if (WRN < _level)
        {
            return;
        }

        std::string s;
        va_list args;
        va_start(args, format);
        s = str::vasprintf(format, args);
        va_end(args);
        msg_txt(0, WRN, s);
    }

    void err(int indent, const std::string &s)
    {
        if (ERR < _level)
        {
            return;
        }

        msg(indent, ERR, s);
    }

    void err(int indent, const char *format, ...)
    {
        if (ERR < _level)
        {
            return;
        }

        std::string s;
        va_list args;
        va_start(args, format);
        s = str::vasprintf(format, args);
        va_end(args);
        msg(indent, ERR, s);
    }

    void err_txt(int indent, const std::string &s)
    {
        if (ERR < _level)
        {
            return;
        }

        msg_txt(indent, ERR, s);
    }

    void err_txt(int indent, const char *format, ...)
    {
        if (ERR < _level)
        {
            return;
        }

        std::string s;
        va_list args;
        va_start(args, format);
        s = str::vasprintf(format, args);
        va_end(args);
        msg_txt(indent, ERR, s);
    }

    void err(const std::string &s)
    {
        if (ERR < _level)
        {
            return;
        }

        msg(0, ERR, s);
    }

    void err(const char *format, ...)
    {
        if (ERR < _level)
        {
            return;
        }

        std::string s;
        va_list args;
        va_start(args, format);
        s = str::vasprintf(format, args);
        va_end(args);
        msg(0, ERR, s);
    }

    void err_txt(const std::string &s)
    {
        if (ERR < _level)
        {
            return;
        }

        msg_txt(0, ERR, s);
    }

    void err_txt(const char *format, ...)
    {
        if (ERR < _level)
        {
            return;
        }

        std::string s;
        va_list args;
        va_start(args, format);
        s = str::vasprintf(format, args);
        va_end(args);
        msg_txt(0, ERR, s);
    }

    void req(int indent, const std::string &s)
    {
        msg(indent, REQ, s);
    }

    void req(int indent, const char *format, ...)
    {
        std::string s;
        va_list args;
        va_start(args, format);
        s = str::vasprintf(format, args);
        va_end(args);
        msg(indent, REQ, s);
    }

    void req_txt(int indent, const std::string &s)
    {
        msg_txt(indent, REQ, s);
    }

    void req_txt(int indent, const char *format, ...)
    {
        std::string s;
        va_list args;
        va_start(args, format);
        s = str::vasprintf(format, args);
        va_end(args);
        msg_txt(indent, REQ, s);
    }

    void req(const std::string &s)
    {
        msg(0, REQ, s);
    }

    void req(const char *format, ...)
    {
        std::string s;
        va_list args;
        va_start(args, format);
        s = str::vasprintf(format, args);
        va_end(args);
        msg(0, REQ, s);
    }

    void req_txt(const std::string &s)
    {
        msg_txt(0, REQ, s);
    }

    void req_txt(const char *format, ...)
    {
        std::string s;
        va_list args;
        va_start(args, format);
        s = str::vasprintf(format, args);
        va_end(args);
        msg_txt(0, REQ, s);
    }
}
