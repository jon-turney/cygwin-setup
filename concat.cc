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
concat (const char *s, ...)
{
  va_list v;

  va_start (v, s);

  return vconcat (s, v);
}

char *
vconcat (const char *s, va_list v)
{
  int len;
  char *rv, *arg;
  va_list save_v = v;
  int unc;

  if (!s)
    return 0;

  len = strlen (s);

  unc = SLASH_P (*s) && SLASH_P (s[1]);

  while (1)
    {
      arg = va_arg (v, char *);
      if (arg == 0)
	break;
      len += strlen (arg);
    }
  va_end (v);

  rv = (char *) malloc (len + 1);
  strcpy (rv, s);
  v = save_v;
  while (1)
    {
      arg = va_arg (v, char *);
      if (arg == 0)
	break;
      strcat (rv, arg);
    }
  va_end (v);

  char *d, *p;
  for (p = rv; *p; p++)
    if (*p == '\\')
      *p = '/';

  /* concat is only used for urls and files, so we can safely
     canonicalize the results */
  for (p = d = rv; *p; p++)
    {
      *d++ = *p;
      /* special case for URLs */
      if (*p == ':' && p[1] == '/' && p[2] == '/' && p > rv + 1)
	{
	  *d++ = *++p;
	  *d++ = *++p;
	}
      else if (*p == '/' || *p == '\\')
	{
	  if (p == rv && unc)
	    p++;
	  while (p[1] == '/')
	    p++;
	}
    }
  *d = 0;

  return rv;
}

char *
backslash (char *s)
{
  for (char *t = s; *t; t++)
    if (*t == '/')
      *t = '\\';
  return s;
}
