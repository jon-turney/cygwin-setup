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

#ifndef _OPTION_H_
#define _OPTION_H_

#if HAVE_CONFIG_H
#include "autoconf.h"
#endif
#if HAVE_STRING___H
#include "String++.h"
// this is bogus, because autoconf doesn't understand
// <string> vs <string.h>
#elif HAVE_STRING_H
#include <string>
typedef string String;
#else 
#error ""String++.h" or <string> required"
#endif

// Each registered option must implement this class.
class Option
{
public:
  virtual ~ Option ();
  virtual char const shortOption () const = 0;
  virtual struct option longOption () const = 0;
  virtual String const shortHelp () const = 0;
  virtual bool Process (char const *) = 0;

protected:
    Option ();
};

#endif // _OPTION_H_
