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

/* The purpose of this file is to doa recursive find on a given
   directory, calling a given function for each file found. */

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include "win32.h"
#include <stdio.h>
#include <stdlib.h>

#include "port.h"

#include "String++.h"
#include "find.h"

static char dir[_MAX_PATH], *found_part;

static int
find_sub (void (*for_each) (char *, unsigned int))
{
  WIN32_FIND_DATA wfd;
  HANDLE h;
  char *end = dir + strlen (dir);
  int rv = 0;

  *end++ = '/';
  strcpy (end, "*");

  h = FindFirstFile (dir, &wfd);

  if (h == INVALID_HANDLE_VALUE)
    return 0;

  do
    {
      if (strcmp (wfd.cFileName, ".") == 0
	  || strcmp (wfd.cFileName, "..") == 0)
	continue;

      strcpy (end, wfd.cFileName);

      if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	find_sub (for_each);
      else
	{
	  for_each (found_part, wfd.nFileSizeLow);
	  rv++;
	}

    }
  while (FindNextFile (h, &wfd));

  FindClose (h);
  return rv;
}

int
find (String const &starting_dir, void (*_for_each) (char *, unsigned int))
{
  strcpy (dir, starting_dir.cstr_oneuse());
  found_part = dir + strlen (dir) + 1;

  return find_sub (_for_each);
}
