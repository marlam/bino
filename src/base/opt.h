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

#ifndef OPT_H
#define OPT_H

#include <sstream>
#include <vector>
#include <string>
#include <cctype>
#include <cstdio>
#include <stdint.h>


/**
 * \brief Command line parsing
 *
 * This module allows quick and easy command line parsing.
 *
 * A set of useful default option types is provided, and you can implement your own.
 *
 * The idea is to build a vector of the options you want to support,
 * and the call the opt::parse() function to parse your command line.
 *
 * An example:
 *
 * opt::info version("version", '\0', opt::optional);
 * opt::flag verbose("verbose", 'v', opt::optional);
 * opt::val<int> width("width", 'w', opt::required, 1, 100, 50);
 *
 * std::vector<opt::option *> options;
 * options.push_back(&version);
 * options.push_back(&verbose);
 * options.push_back(&width);
 *
 * std::vector<std::string> arguments;
 * opt::parse(argc, argv, options, 1, -1, arguments);
 */

namespace opt
{
    /**
     * \brief Option policy
     *
     * Policy of an option.
     */
    enum option_policy
    {
        optional = 0,       /** The option is optional. */
        required = 1        /** The option is mandatory. */
    };

    /**
     * \brief Option interface
     *
     * The interface for options.\n
     * You can implement your own options by implementing this interface.\n
     * If the option takes arguments, you should also implement the two
     * functions value() and values().\n
     * The function value() should return the parsed argument of the last occurence
     * of the option, whereas the function values() should return the parsed arguments
     * of all occurences of the option in a vector. An empty vector indicates that
     * the option was not given on the command line.\n
     */
    class option
    {
    private:
        std::string _longname;
        char _shortname;
        option_policy _policy;

    public:
        /**
         * \brief Constructor
         * \param longname      Long name of the option, or "".
         * \param shortname     Short name of the option, or '\0'.
         * \param mandatory     Whether this option is mandatory or optional.
         */
        option(const std::string &longname, char shortname, option_policy policy)
            : _longname(longname), _shortname(shortname), _policy(policy)
        {
        }

        const std::string &longname() const
        {
            return _longname;
        }

        char shortname() const
        {
            return _shortname;
        }

        opt::option_policy policy() const
        {
            return _policy;
        }

        /**
         * \brief Argument policy
         *
         * Argument policy of an option.
         */
        enum argument_policy
        {
            no_argument = 0,            /** The option takes no argument. */
            required_argument = 1,      /** The option requires an argument. */
            optional_argument = 2       /** The option accepts an optional argument. */
        };

        /**
         * \brief Return the argument policy of this option.
         * \return The argument policy.
         */
        virtual enum argument_policy argument_policy() const = 0;

        /**
         * \brief Parse an argument to this option (if applicable).
         * \param argument      The argument. Empty if there is no argument.
         * \return              Success (true) or failure (false).
         */
        virtual bool parse_argument(const std::string &argument) = 0;
    };

    /**
     * \brief Parse the command line.
     * \param argc              The standard argc parameter.
     * \param argv              The standard argv parameter.
     * \param options           A vector with pointers to options.
     * \param min_arguments     Minimum number of arguments, or -1 if there is no limit.
     * \param max_arguments     Maximum number of arguments, or -1 if there is no limit.
     * \param arguments         A vector in which the arguments will be stored.
     * \return                  Success (true) or failure (false).
     *
     * This function parses the command line. It uses GNU's getopt_long()
     * internally and thus has the same features:\n
     * - allows abbreviation of long options to their shortest unambiguous prefix
     * - allows mixing of arguments and options
     * - space between short option and argument is optional
     * - arguments to long options can be given with an optional '=' character
     * - the special argument '--' separates options from arguments
     * - ...and so on
     */
    bool parse(int argc, char *argv[],
            std::vector<option *> &options,
            int min_arguments, int max_arguments,
            std::vector<std::string> &arguments);

    /**
     * \brief An option type for informational options.
     *
     * This option type is for options such as --help and --version. When such an
     * option is found, the parser will not test for the presence of mandatory
     * options or arguments, so that 'prg --help' and 'prg --version' always work.
     *
     * The value() function will return whether this option was seen.
     */
    class info : public option
    {
    private:
        bool _seen;

    public:
        info(const std::string &longname, char shortname, enum option_policy policy)
            : option(longname, shortname, policy), _seen(false)
        {
        }

        enum argument_policy argument_policy() const
        {
            return no_argument;
        }

        bool parse_argument(const std::string &)
        {
            _seen = true;
            return true;
        }

        bool value() const
        {
            return _seen;
        }
    };

    /**
     * \brief An option type for flags.
     *
     * This option type provides flags. A flag may have no argument, in which
     * case a default argument is assumed, or it may have one of the arguments "true",
     * "on", "yes" to set the flag, or "false", "off", "no" to unset the flag.
     */
    class flag : public option
    {
    private:
        const bool _default_value;
        const bool _default_argument;
        std::vector<bool> _values;

    public:
        flag(const std::string &longname, char shortname, enum option_policy policy,
                bool default_value = false, bool default_argument = true)
            : option(longname, shortname, policy),
            _default_value(default_value), _default_argument(default_argument)
        {
        }

        enum argument_policy argument_policy() const
        {
            return optional_argument;
        }

        bool parse_argument(const std::string &argument)
        {
            if (argument.empty())
            {
                _values.push_back(_default_argument);
                return true;
            }
            else if (argument.compare("on") == 0 || argument.compare("true") == 0 || argument.compare("yes") == 0)
            {
                _values.push_back(true);
                return true;
            }
            else if (argument.compare("off") == 0 || argument.compare("false") == 0 || argument.compare("no") == 0)
            {
                _values.push_back(false);
                return true;
            }
            else
            {
                return false;
            }
        }

        bool value() const
        {
            return (_values.size() > 0 ? _values.back() : _default_value);
        }

        const std::vector<bool> &values() const
        {
            return _values;
        }
    };

    /**
     * \brief An option type for values.
     *
     * This option type allows to parse a value of arbitrary type.\n
     * The number of allowed values can optionally be restricted:\n
     * - If a lower and higher bound are given, then only values within this range
     *   are allowed. Other values will be rejected. The bounds
     *   are inclusive by default, but can be made exclusive (useful
     *   for floating point data).
     * - If a vector of allowed values is given, only these values
     *   will be allowed, and all other values will be rejected.
     *
     * Note that if this option type is used with type std::string,
     * then the string must not contain spaces. If you need to parse strings with spaces,
     * use option_string.
     */
    template<typename T>
    class val : public option
    {
    private:
        enum restriction_policy
        {
            none,
            bounds,
            allowed_values
        };
        const restriction_policy _restriction_policy;
        const T _lower_bound;
        const bool _lower_bound_inclusive;
        const T _higher_bound;
        const bool _higher_bound_inclusive;
        const std::vector<T> _allowed_values;
        const T _default_value;
        std::vector<T> _values;

    public:
        val(const std::string &longname, char shortname, enum option_policy policy,
                const T &lower_bound, const T &higher_bound, const T &default_value = T())
            : option(longname, shortname, policy),
            _restriction_policy(bounds),
            _lower_bound(lower_bound), _lower_bound_inclusive(true),
            _higher_bound(higher_bound), _higher_bound_inclusive(true),
            _allowed_values(),
            _default_value(default_value)
        {
        }
        val(const std::string &longname, char shortname, enum option_policy policy,
                const T &lower_bound, bool lower_bound_inclusive,
                const T &higher_bound, bool higher_bound_inclusive,
                const T &default_value = T())
            : option(longname, shortname, policy),
            _restriction_policy(bounds),
            _lower_bound(lower_bound), _lower_bound_inclusive(lower_bound_inclusive),
            _higher_bound(higher_bound), _higher_bound_inclusive(higher_bound_inclusive),
            _allowed_values(),
            _default_value(default_value)
        {
        }
        val(const std::string &longname, char shortname, enum option_policy policy,
                const std::vector<T> &allowed_values, const T &default_value = T())
            : option(longname, shortname, policy),
            _restriction_policy(val::allowed_values),
            _lower_bound(), _lower_bound_inclusive(),
            _higher_bound(), _higher_bound_inclusive(),
            _allowed_values(allowed_values),
            _default_value(default_value)
        {
        }
        val(const std::string &longname, char shortname, enum option_policy policy,
                const T &default_value = T())
            : option(longname, shortname, policy),
            _restriction_policy(none),
            _lower_bound(), _lower_bound_inclusive(),
            _higher_bound(), _higher_bound_inclusive(),
            _allowed_values(),
            _default_value(default_value)
        {
        }

        enum argument_policy argument_policy() const
        {
            return required_argument;
        }

        bool parse_argument(const std::string &argument)
        {
            T v;
            std::istringstream is(argument);
            bool ok = false;

            is >> v;
            if (!is.fail() && is.eof())
            {
                if (_restriction_policy == allowed_values)
                {
                    for (size_t i = 0; i < _allowed_values.size(); i++)
                    {
                        if (_allowed_values[i] <= v && _allowed_values[i] >= v)
                        {
                            ok = true;
                            break;
                        }
                    }
                }
                else if (_restriction_policy == bounds)
                {
                    ok = (((_lower_bound_inclusive && v >= _lower_bound)
                                || (!_lower_bound_inclusive && v > _lower_bound))
                            && ((_higher_bound_inclusive && v <= _higher_bound)
                                || (!_higher_bound_inclusive && v < _higher_bound)));
                }
                else // _restriction_policy == none
                {
                    ok = true;
                }
                if (ok)
                {
                    _values.push_back(v);
                }
            }
            return ok;
        }

        const T &value() const
        {
            return (_values.size() > 0 ? _values.back() : _default_value);
        }

        const std::vector<T> &values() const
        {
            return _values;
        }
    };

    template<typename T>
    class tuple : public option
    {
    private:
        enum restriction_policy
        {
            none,
            bounds,
            allowed_values
        };
        const std::string _separator;
        const bool _size_is_fixed;
        const size_t _fixed_size;
        const restriction_policy _restriction_policy;
        const T _lower_bound;
        const bool _lower_bound_inclusive;
        const T _higher_bound;
        const bool _higher_bound_inclusive;
        const std::vector<T> _allowed_values;
        const std::vector<T> _default_value;
        std::vector<std::vector<T> > _values;

    public:
        tuple(const std::string &longname, char shortname, enum option_policy policy,
                const T &lower_bound, const T &higher_bound,
                const std::vector<T> &default_value = std::vector<T>(),
                int fixed_size = -1, const std::string &separator = ",")
            : option(longname, shortname, policy),
            _separator(separator), _size_is_fixed(fixed_size >= 0), _fixed_size(fixed_size),
            _restriction_policy(bounds),
            _lower_bound(lower_bound), _lower_bound_inclusive(true),
            _higher_bound(higher_bound), _higher_bound_inclusive(true),
            _allowed_values(),
            _default_value(default_value)
        {
        }
        tuple(const std::string &longname, char shortname, enum option_policy policy,
                const T &lower_bound, bool lower_bound_inclusive,
                const T &higher_bound, bool higher_bound_inclusive,
                const std::vector<T> &default_value = std::vector<T>(),
                int fixed_size = -1, const std::string &separator = ",")
            : option(longname, shortname, policy),
            _separator(separator), _size_is_fixed(fixed_size >= 0), _fixed_size(fixed_size),
            _restriction_policy(bounds),
            _lower_bound(lower_bound), _lower_bound_inclusive(lower_bound_inclusive),
            _higher_bound(higher_bound), _higher_bound_inclusive(higher_bound_inclusive),
            _allowed_values(),
            _default_value(default_value)
        {
        }
        tuple(const std::string &longname, char shortname, enum option_policy policy,
                const std::vector<T> &allowed_values,
                const std::vector<T> &default_value,
                int fixed_size = -1, const std::string &separator = ",")
            : option(longname, shortname, policy),
            _separator(separator), _size_is_fixed(fixed_size >= 0), _fixed_size(fixed_size),
            _restriction_policy(tuple::allowed_values),
            _lower_bound(), _lower_bound_inclusive(),
            _higher_bound(), _higher_bound_inclusive(),
            _allowed_values(allowed_values),
            _default_value(default_value)
        {
        }
        tuple(const std::string &longname, char shortname, enum option_policy policy,
                const std::vector<T> &default_value = std::vector<T>(),
                int fixed_size = -1, const std::string &separator = ",")
            : option(longname, shortname, policy),
            _separator(separator), _size_is_fixed(fixed_size >= 0), _fixed_size(fixed_size),
            _restriction_policy(none),
            _lower_bound(), _lower_bound_inclusive(),
            _higher_bound(), _higher_bound_inclusive(),
            _allowed_values(),
            _default_value(default_value)
        {
        }

        enum argument_policy argument_policy() const
        {
            return required_argument;
        }

        bool parse_argument(const std::string &argument)
        {
            std::istringstream is(argument);
            std::vector<T> vv;
            if (argument.length() > 0)
            {
                do
                {
                    T v;
                    is >> v;
                    if (is.fail())
                    {
                        return false;
                    }
                    vv.push_back(v);
                    if (!is.eof())
                    {
                        for (size_t i = 0; i < _separator.length(); i++)
                        {
                            char c;
                            is.get(c);
                            if (is.fail() || c != _separator[i])
                            {
                                return false;
                            }
                        }
                    }
                }
                while (!is.eof());
            }
            if (_size_is_fixed && vv.size() != _fixed_size)
            {
                return false;
            }
            if (_restriction_policy == allowed_values)
            {
                bool ok = false;
                for (size_t i = 0; i < _allowed_values.size(); i++)
                {
                    for (size_t j = 0; j < vv.size(); j++)
                    {
                        if (_allowed_values[i] <= vv[j] && _allowed_values[i] >= vv[j])
                        {
                            ok = true;
                            break;
                        }
                    }
                    if (ok)
                    {
                        break;
                    }
                }
                if (!ok)
                {
                    return false;
                }
            }
            else if (_restriction_policy == bounds)
            {
                for (size_t i = 0; i < vv.size(); i++)
                {
                    if ((_lower_bound_inclusive && vv[i] < _lower_bound)
                            || (!_lower_bound_inclusive && vv[i] <= _lower_bound)
                            || (_higher_bound_inclusive && vv[i] > _higher_bound)
                            || (!_higher_bound_inclusive && vv[i] >= _higher_bound))
                    {
                        return false;
                    }
                }
            }
            _values.push_back(vv);
            return true;
        }

        const std::vector<T> &value() const
        {
            return (_values.size() > 0 ? _values.back() : _default_value);
        }

        const std::vector<std::vector<T> > &values() const
        {
            return _values;
        }
    };

    /**
     * \brief An option type for strings.
     *
     * This option type allows to parse arbitrary strings, including spaces.
     * Optionally, a list of allowed strings can be given, and also a list
     * of allowed control characters. By default, the string must not contain
     * control characters.
     */
    class string : public option
    {
    private:
        const std::string _allowed_control_chars;
        const std::vector<std::string> _allowed_values;
        const std::string _default_value;
        std::vector<std::string> _values;

    public:
        string(const std::string &longname, char shortname, enum option_policy policy,
                const std::vector<std::string> allowed_values,
                const std::string &default_value = "")
            : option(longname, shortname, policy),
            _allowed_control_chars(""),
            _allowed_values(allowed_values),
            _default_value(default_value)
        {
        }
        string(const std::string &longname, char shortname, enum option_policy policy,
                const std::string &allowed_control_chars,
                const std::string &default_value)
            : option(longname, shortname, policy),
            _allowed_control_chars(allowed_control_chars),
            _allowed_values(),
            _default_value(default_value)
        {
        }
        string(const std::string &longname, char shortname, enum option_policy policy,
                const std::string &default_value = "")
            : option(longname, shortname, policy),
            _allowed_control_chars(""),
            _allowed_values(),
            _default_value(default_value)
        {
        }

        enum argument_policy argument_policy() const
        {
            return required_argument;
        }

        bool parse_argument(const std::string &argument)
        {
            bool ok;
            if (_allowed_values.size() > 0)
            {
                ok = false;
                for (size_t i = 0; i < _allowed_values.size(); i++)
                {
                    if (argument.compare(_allowed_values[i]) == 0)
                    {
                        ok = true;
                        break;
                    }
                }
            }
            else
            {
                ok = true;
                for (size_t i = 0; i < argument.length(); i++)
                {
                    char c = argument[i];
                    if (std::iscntrl(c) && _allowed_control_chars.find(c) == std::string::npos)
                    {
                        ok = false;
                        break;
                    }
                }
            }
            if (ok)
            {
                _values.push_back(argument);
            }
            return ok;
        }

        const std::string &value() const
        {
            return (_values.size() > 0 ? _values.back() : _default_value);
        }

        const std::vector<std::string> &values() const
        {
            return _values;
        }
    };

    /**
     * \brief An option type for colors.
     *
     * This option type accepts colors in the format [AA]RRGGBB. If the alpha
     * part is omitted, it is set to 255.
     * Color values are returned in uint32_t BGRA format.
     */
    class color : public option
    {
    private:
        uint32_t _default_value;
        std::vector<uint32_t> _values;

    public:
        color(const std::string &longname, char shortname, option_policy policy,
                uint32_t default_value = 0u)
            : option(longname, shortname, policy),
            _default_value(default_value)
        {
        }

        virtual enum argument_policy argument_policy() const
        {
            return required_argument;
        }

        virtual bool parse_argument(const std::string &argument)
        {
            unsigned int a = 255u, r, g, b;
            if (argument.length() != 8 && argument.length() != 6)
            {
                return false;
            }
            else if (argument.length() == 8 && std::sscanf(argument.c_str(), "%2x%2x%2x%2x", &a, &r, &g, &b) != 4)
            {
                return false;
            }
            else if (argument.length() == 6 && std::sscanf(argument.c_str(), "%2x%2x%2x", &r, &g, &b) != 3)
            {
                return false;
            }
            _values.push_back((a << 24u) | (r << 16u) | (g << 8u) | b);
            return true;
        }

        uint32_t value() const
        {
            return (_values.size() > 0 ? _values.back() : _default_value);
        }

        const std::vector<uint32_t> &values() const
        {
            return _values;
        }
    };
}

#endif
