/*
 * Copyright (c) 2002 Robert Collins.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Robert Collins <robertc@hotmail.com>
 *
 */

#include <getopt++/StringOption.h>

StringOption::StringOption(string const defaultvalue, char shortopt, 
		       char const *longopt, string const &shorthelp, 
		       bool const optional, OptionSet &owner) : 
		       _value (defaultvalue) , _shortopt(shortopt),
		       _longopt (longopt), _shorthelp (shorthelp)
{
  if (!optional)
    _optional = required_argument;
  else
    _optional = optional_argument;
  owner.Register (this);
};

StringOption::~ StringOption () {};

string const 
StringOption::shortOption () const
{
  return string() + _shortopt + ":";
}

struct option 
StringOption::longOption () const
{
  struct option foo = {_longopt, _optional, NULL, _shortopt};
  return foo;
}

string const 
StringOption::shortHelp () const
{
  return _shorthelp;
}

bool 
StringOption::Process (char const *optarg)
{
  if (optarg)
    _value = optarg;
}

StringOption::operator string () const
{
  return _value;
}
