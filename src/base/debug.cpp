/*
 * This file is part of bino, a program to play stereoscopic videos.
 *
 * Copyright (C) 2006, 2007, 2009, 2010
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

#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <new>
#if HAVE_BACKTRACE
# include <execinfo.h>
#endif
#if __GNUG__
# include <cxxabi.h>
# include <cstring>
#endif
#include <signal.h>

#include "msg.h"
#include "debug.h"


namespace debug
{
    static void signal_crash(int signum)
    {
        msg::err("Caught signal %d (%s). Aborting.", signum,
                (signum == SIGILL ? "SIGILL" : (signum == SIGFPE ? "SIGFPE" : "SIGSEGV")));
        crash();
    }

    static void exception_crash()
    {
        msg::err("Unexpected exception.");
        crash();
    }

    void oom_abort()
    {
        msg::err("%s", strerror(ENOMEM));
        abort();
    }

    void init_crashhandler()
    {
        struct sigaction signal_handler;
        signal_handler.sa_handler = signal_crash;
        sigemptyset(&signal_handler.sa_mask);
        signal_handler.sa_flags = 0;
        (void)sigaction(SIGILL, &signal_handler, NULL);
        (void)sigaction(SIGFPE, &signal_handler, NULL);
        (void)sigaction(SIGSEGV, &signal_handler, NULL);
        std::set_unexpected(exception_crash);
        std::set_terminate(exception_crash);
        std::set_new_handler(oom_abort);
    }

    void backtrace()
    {
#if HAVE_BACKTRACE
        // Adapted from the example in the glibc manual
        void *array[64];
        int size;
        char **strings;

        size = ::backtrace(array, 64);

        if (size == 0)
        {
            msg::err("No backtrace available.");
        }
        else
        {
            msg::err("Backtrace:");
            strings = backtrace_symbols(array, size);
            for (int i = 0; i < size; i++)
            {
# if __GNUG__
                int status;
                char *p, *q;
                char *realname = NULL;

                p = strchr(strings[i], '(');
                if (p)
                {
                    p++;
                    q = strchr(p, '+');
                    if (q)
                    {
                        *q = '\0';
                        realname = abi::__cxa_demangle(p, NULL, NULL, &status);
                        *q = '+';
                    }
                }
                if (realname && status == 0)
                {
                    *p = '\0';
                    msg::err("    %s%s%s", strings[i], realname, q);
                }
                else
                {
                    msg::err("    %s", strings[i]);
                }
                free(realname);
# else
                msg::err("    %s", strings[i]);
# endif
            }
            free(strings);
        }
#endif
    }

    void crash()
    {
        backtrace();
        msg::err("Please report this bug to <%s>.", PACKAGE_BUGREPORT);
        abort();
    }
}
