/*
 * Copyright (c) 2000, 2001, Red Hat, Inc.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Robert Collins <rbtcollins@redhat.com>
 *
 */

/* The purpose of this file is to put all general purpose file manipulation
   code in one place. */

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include "win32.h"

#include <stdio.h>
#include <unistd.h>
#include <ctype.h>

#include "filemanip.h"

unsigned int
get_file_size (const char *name)
{
  HANDLE h;
  WIN32_FIND_DATA buf;
  DWORD ret = 0;

  h = FindFirstFileA (name, &buf);
  if (h != INVALID_HANDLE_VALUE)
    {
      if (buf.nFileSizeHigh == 0)
	ret = buf.nFileSizeLow;
      FindClose (h);
    }
  return ret;
}

char *
base (const char *s)
{
  if (!s)
    return 0;
  const char *rv = s;
  while (*s)
    {
      if ((*s == '/' || *s == ':' || *s == '\\') && s[1])
	rv = s + 1;
      s++;
    }
  return (char *) rv;
}

/* returns the number of characters of path that
 * precede the extension
 */
int
find_tar_ext (const char *path)
{
  char *end = strchr (path, '\0');
  /* check in longest first order */
  char *ext;
  if ((ext = strstr (path, ".tar.bz2")) && (end - ext) == 8)
    return ext - path;
  if ((ext = strstr (path, ".tar.gz")) && (end - ext) == 7)
    return ext - path;
  if ((ext = strstr (path, ".tar")) && (end - ext) == 4)
    return ext - path;
  return 0;
}

/* Parse a filename into package, version, and extension components. */
int
parse_filename (const char *in_fn, fileparse & f)
{
  char *p, *ver;
  char fn[strlen (in_fn) + 1];

  strcpy (fn, in_fn);
  int n = find_tar_ext (fn);

  if (!n)
    return 0;

  strcpy (f.tail, fn + n);
  fn[n] = '\0';
  f.pkg[0] = f.what[0] = '\0';
  p = base (fn);
  for (ver = p; *ver; ver++)
    if (*ver == '-')
      if (isdigit (ver[1]))
	{
	  *ver++ = 0;
	  strcpy (f.pkg, p);
	  break;
	}
      else if (strcasecmp (ver, "-src") == 0 ||
	       strcasecmp (ver, "-patch") == 0)
	{
	  *ver++ = 0;
	  strcpy (f.pkg, p);
	  strcpy (f.what, strlwr (ver));
	  strcpy (f.pkgtar, p);
	  strcat (f.pkgtar, f.tail);
	  ver = strchr (ver, '\0');
	  break;
	}

  if (!f.pkg[0])
    strcpy (f.pkg, p);

  if (!f.what[0])
    {
      int n;
      p = strchr (ver, '\0');
      strcpy (f.pkgtar, in_fn);
      if ((p -= 4) >= ver && strcasecmp (p, "-src") == 0)
	n = 4;
      else if ((p -= 2) >= ver && strcasecmp (p, "-patch") == 0)
	n = 6;
      else
	n = 0;
      if (n)
	{
	  strcpy (f.what, p + 1);
	  *p = '\0';
	  p = f.pkgtar + (p - fn) + n;
	  memmove (p - n, p, strlen (p));
	}
    }

  strcpy (f.ver, *ver ? ver : "0.0");
  return 1;
}
