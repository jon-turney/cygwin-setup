/*
 * Copyright (c) 2000, Red Hat, Inc.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by DJ Delorie <dj@cygnus.com>
 *
 */

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include "win32.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>

#include "version.h"
  
String 
canonicalize_version (String const &aString)
{
  char *v =aString.cstr();
  char *savev = v;
  static char nv[3][100];
  static int idx = 0;
  char *np;
  const char *dp;
  int i;

  idx = (idx + 1) % 3;
  np = nv[idx];

  while (*v)
    {
      if (isdigit (*v))
	{
	  for (dp = v; *dp && isdigit (*dp); dp++);
	  for (i = dp - v; i < 12; i++)
	    *np++ = '0';
	  while (v < dp)
	    *np++ = *v++;
	}
      else
	*np++ = *v++;
    }
  *np++ = 0;
  delete[] savev;
  return nv[idx];
}
