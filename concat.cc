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

/* See concat.h.  Note that we canonicalize the result, this avoids
   multiple slashes being interpreted as UNCs. */

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "concat.h"

char *
backslash (char *s)
{
  for (char *t = s; *t; t++)
    if (*t == '/')
      *t = '\\';
  return s;
}
