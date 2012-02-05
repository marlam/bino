/*
 * Copyright (C) 2009, 2010, 2011, 2012
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
 * \file msg.h
 *
 * Log messages.
 */

#ifndef MSG_H
#define MSG_H

#include <cstdio>
#include <cstdarg>
#include <string>

#ifdef __GNUC__
# define MSG_AFP(a, b) __attribute__ ((format (printf, a, b)))
#else
# define MSG_AFP(a, b) /* empty */
#endif

namespace msg
{
    /* Message levels */

    typedef enum
    {
        DBG = 0,        // Only ever printed if NDEBUG is not set!
        INF = 1,
        WRN = 2,
        ERR = 3,
        REQ = 4
    } level_t;

    /* Get / Set configuration */

    FILE *file();
    void set_file(FILE *f);
    level_t level();
    void set_level(level_t l);
    int columns();
    void set_columns(int l);
    void set_columns_from_env();
    std::string program_name();
    void set_program_name(const std::string &n);
    std::string category_name();
    void set_category_name(const std::string &n);

    /* Print messages */

    void msg(int indent, level_t level, const std::string &s);
    void msg(int indent, level_t level, const char *format, ...) MSG_AFP(3, 4);
    void msg_txt(int indent, level_t level, const std::string &s);
    void msg_txt(int indent, level_t level, const char *format, ...) MSG_AFP(3, 4);

    void msg(level_t level, const std::string &s);
    void msg(level_t level, const char *format, ...) MSG_AFP(2, 3);
    void msg_txt(level_t level, const std::string &s);
    void msg_txt(level_t level, const char *format, ...) MSG_AFP(2, 3);

#ifdef NDEBUG
    inline void dbg(int, const std::string &) { }
    inline void dbg(int, const char *, ...) { }
    inline void dbg_txt(int, const std::string &) { }
    inline void dbg_txt(int, const char *, ...) { }
#else
    void dbg(int indent, const std::string &s);
    void dbg(int indent, const char *format, ...) MSG_AFP(2, 3);
    void dbg_txt(int indent, const std::string &s);
    void dbg_txt(int indent, const char *format, ...) MSG_AFP(2, 3);
#endif

#ifdef NDEBUG
    inline void dbg(const std::string &) { }
    inline void dbg(const char *, ...) { }
    inline void dbg_txt(const std::string &) { }
    inline void dbg_txt(const char *, ...) { }
#else
    void dbg(const std::string &s);
    void dbg(const char *format, ...) MSG_AFP(1, 2);
    void dbg_txt(const std::string &s);
    void dbg_txt(const char *format, ...) MSG_AFP(1, 2);
#endif

    void inf(int indent, const std::string &s);
    void inf(int indent, const char *format, ...) MSG_AFP(2, 3);
    void inf_txt(int indent, const std::string &s);
    void inf_txt(int indent, const char *format, ...) MSG_AFP(2, 3);

    void inf(const std::string &s);
    void inf(const char *format, ...) MSG_AFP(1, 2);
    void inf_txt(const std::string &s);
    void inf_txt(const char *format, ...) MSG_AFP(1, 2);

    void wrn(int indent, const std::string &s);
    void wrn(int indent, const char *format, ...) MSG_AFP(2, 3);
    void wrn_txt(int indent, const std::string &s);
    void wrn_txt(int indent, const char *format, ...) MSG_AFP(2, 3);

    void wrn(const std::string &s);
    void wrn(const char *format, ...) MSG_AFP(1, 2);
    void wrn_txt(const std::string &s);
    void wrn_txt(const char *format, ...) MSG_AFP(1, 2);

    void err(int indent, const std::string &s);
    void err(int indent, const char *format, ...) MSG_AFP(2, 3);
    void err_txt(int indent, const std::string &s);
    void err_txt(int indent, const char *format, ...) MSG_AFP(2, 3);

    void err(const std::string &s);
    void err(const char *format, ...) MSG_AFP(1, 2);
    void err_txt(const std::string &s);
    void err_txt(const char *format, ...) MSG_AFP(1, 2);

    void req(int indent, const std::string &s);
    void req(int indent, const char *format, ...) MSG_AFP(2, 3);
    void req_txt(int indent, const std::string &s);
    void req_txt(int indent, const char *format, ...) MSG_AFP(2, 3);

    void req(const std::string &s);
    void req(const char *format, ...) MSG_AFP(1, 2);
    void req_txt(const std::string &s);
    void req_txt(const char *format, ...) MSG_AFP(1, 2);
}

#endif
