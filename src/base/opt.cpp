/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2006-2007, 2009-2011
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

#include <typeinfo>
#include <limits>
#include <cstring>

#include "gettext.h"
#define _(string) gettext(string)

#include <getopt.h>
#undef no_argument
#undef required_argument
#undef optional_argument

#include "msg.h"
#include "opt.h"


namespace opt
{
    bool parse(int argc, char *argv[],
            std::vector<option *> &options,
            int min_arguments, int max_arguments,
            std::vector<std::string> &arguments)
    {
        const int optval_base = static_cast<int>(std::numeric_limits<unsigned char>::max()) + 1;
        int longopt_count;          /* Number of long options == number of options */
        int longopt_ind;            /* Index in options and longoptions */
        int shortopt_count;         /* Number of short options <= number of options */
        struct ::option *longopts;  /* long option description for getopt_long() */
        char *shortopts;            /* short option description for getopt_long() */
        int shortopts_ind;          /* index in short options */
        int *shortopt_opt_ind;      /* Mapping from index of short options to index of options */
        bool option_shortname;      /* Whether the short name of an option was used */
        bool *option_was_seen;      /* Whether option i was used */
        bool info_option_was_seen;  /* Whether an option of type opt_info was seen */
        bool error;

        info_option_was_seen = false;
        longopt_count = options.size();
        shortopt_count = 0;
        for (int i = 0; i < longopt_count; i++)
        {
            if (options[i]->shortname() != '\0')
            {
                shortopt_count++;
            }
        }
        /* Construct an array of type 'struct option', 'longopts', and a short
         * option description string 'shortopts' for use with getopt_long().
         * The indices of options in the given array 'options' and the constructed
         * array 'longopts' are the same. This is not true for the 'shortopts'
         * string. We use an index array 'shortopt_longopt_ind' in the following
         * way: If 'shortopts' is ":i::", then option i has index 1 in 'shortopts',
         * and 'options[shortopt_longopt_ind[1]]' is the corresponding option
         * description in 'options'.
         * The maximum length of 'shortopts' is 3*number_of_options+1: This is the
         * case when all options described in 'options' have corresponding short
         * options, and all have optional arguments. A leading colon in shortopts is
         * always needed. */
        longopts = new struct ::option[longopt_count + 1];
        shortopts = new char[1 + 3 * shortopt_count + 1];
        shortopts[0] = ':';
        shortopt_opt_ind = new int[1 + 3 * shortopt_count];
        shortopts_ind = 1;
        for (longopt_ind = 0; longopt_ind < longopt_count; longopt_ind++)
        {
            longopts[longopt_ind].name = options[longopt_ind]->longname().c_str();
            if (options[longopt_ind]->argument_policy() == option::no_argument)
            {
                longopts[longopt_ind].has_arg = 0;      // no_argument
            }
            else if (options[longopt_ind]->argument_policy() == option::optional_argument)
            {
                longopts[longopt_ind].has_arg = 2;      // optional_argument
            }
            else
            {
                longopts[longopt_ind].has_arg = 1;      // required_argument
            }
            longopts[longopt_ind].flag = NULL;
            longopts[longopt_ind].val = optval_base + longopt_ind;
            if (options[longopt_ind]->shortname() != '\0')
            {
                shortopts[shortopts_ind] = options[longopt_ind]->shortname();
                shortopt_opt_ind[shortopts_ind] = longopt_ind;
                shortopts_ind++;
                if (options[longopt_ind]->argument_policy() == option::no_argument)
                {
                }
                else if (options[longopt_ind]->argument_policy() == option::optional_argument)
                {
                    shortopts[shortopts_ind++] = ':';
                    shortopts[shortopts_ind++] = ':';
                }
                else
                {
                    shortopts[shortopts_ind++] = ':';
                }
            }
        }
        shortopts[shortopts_ind] = '\0';
        longopts[longopt_count].name = NULL;
        longopts[longopt_count].has_arg = 0;
        longopts[longopt_count].flag = 0;
        longopts[longopt_count].val = 0;

        /* The getopt_long() loop */
        option_was_seen = new bool[longopt_count];
        for (int i = 0; i < longopt_count; i++)
        {
            option_was_seen[i] = false;
        }
        error = false;
        opterr = 0;
        optind = 1;
#if defined HAVE_DECL_OPTRESET && HAVE_DECL_OPTRESET
        optreset = 1;
#endif
        while (!error)
        {
            int optval = getopt_long(argc, argv, shortopts, longopts, NULL);
            if (optval == -1)
            {
                break;
            }
            else if (optval == '?')
            {
                if (optopt == 0)
                {
                    msg::err(_("Invalid option %s."), argv[optind - 1]);
                }
                else if (optopt >= optval_base)
                {
                    msg::err(_("Option --%s does not take an argument."),
                            options[optopt - optval_base]->longname().c_str());
                }
                else
                {
                    msg::err(_("Invalid option -%c."), optopt);
                }
                error = true;
                break;
            }
            else if (optval == ':')
            {
                if (optopt >= optval_base)
                {
                    msg::err(_("Option --%s requires an argument."),
                            options[optopt - optval_base]->longname().c_str());
                }
                else
                {
                    msg::err(_("Option -%c requires an argument."), optopt);
                }
                error = true;
                break;
            }
            else if (optval < optval_base)
            {
                option_shortname = true;
                optval = shortopt_opt_ind[(strchr(shortopts, optval) - shortopts)];
            }
            else
            {
                option_shortname = false;
                optval -= optval_base;
            }
            /* options[optval] is the original description of the current option. */
            option_was_seen[optval] = true;
            if (!options[optval]->parse_argument(optarg ? optarg : std::string("")))
            {
                if (option_shortname)
                {
                    msg::err(_("Invalid argument for -%c."), options[optval]->shortname());
                }
                else
                {
                    msg::err(_("Invalid argument for --%s."), options[optval]->longname().c_str());
                }
                error = true;
            }
            if (typeid(*(options[optval])) == typeid(info))
            {
                info_option_was_seen = true;
            }
        }

        /* Test if all mandatory options were seen */
        if (!error && !info_option_was_seen)
        {
            for (int i = 0; i < longopt_count; i++)
            {
                if (options[i]->policy() == opt::required && !option_was_seen[i])
                {
                    if (options[i]->shortname() != '\0')
                    {
                        msg::err(_("Option --%s (-%c) is mandatory."), options[i]->longname().c_str(), options[i]->shortname());
                    }
                    else
                    {
                        msg::err(_("Option --%s is mandatory."), options[i]->longname().c_str());
                    }
                    error = true;
                    /* Do not break the loop. Print an error message for every missing option. */
                }
            }
        }

        /* Test if number of non-options arguments is ok */
        int args = argc - optind;
        if (!error && !info_option_was_seen)
        {
            if (args < min_arguments)
            {
                msg::err(_("Too few arguments."));
                error = true;
            }
            else if (max_arguments >= 0 && args > max_arguments)
            {
                msg::err(_("Too many arguments."));
                error = true;
            }
        }

        /* Return the arguments */
        if (!error)
        {
            arguments.clear();
            for (int i = 0; i < args; i++)
            {
                arguments.push_back(argv[optind + i]);
            }
        }

        delete[] longopts;
        delete[] shortopts;
        delete[] shortopt_opt_ind;
        delete[] option_was_seen;
        return !error;
    }
}
