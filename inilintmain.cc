/*
 * Copyright (c) 2002, Robert Collins.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Robert Collins  <rbtcollins@hotmail.com>
 *
 */

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include "getopt++/GetOption.h"
#include "String++.h"
#include <iostream>
#include <strstream>

extern int yylineno;

static ostrstream error_buf;
static int error_count = 0;

extern int
yyerror (String const &s)
{
  ostrstream buf;
  buf << "setup.ini line " << yylineno << ": ";
  buf << s << endl;
  cout << buf;
  error_buf << buf; 
  error_count++;
  /* TODO: is return 0 correct? */
  return 0;
}

int
main (int argc, char **argv)
{
  return 0;
}
