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

#include <getopt++/BoolOption.h>

BoolOption::BoolOption(bool const defaultvalue, char shortopt, 
		       char const *longopt, String const &shorthelp, 
		       OptionSet &owner) : _value (defaultvalue) , 
		       _ovalue (defaultvalue), _shortopt(shortopt), 
		       _longopt (longopt), _shorthelp (shorthelp)
{
  owner.Register (this);
};

BoolOption::~ BoolOption () {};

char const 
BoolOption::shortOption () const
{
  return _shortopt;
}

struct option 
BoolOption::longOption () const
{
  struct option foo = {_longopt, no_argument, NULL, _shortopt};
  return foo;
}

String const 
BoolOption::shortHelp () const
{
  return _shorthelp;
}

bool 
BoolOption::Process (char const *)
{
  _value = !_ovalue;
}

BoolOption::operator bool () const
{
  return _value;
}
