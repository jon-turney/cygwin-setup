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

#include <stdio.h>
#include <unistd.h>
#include <ctype.h>

#include "filemanip.h"
#include <strings.h>
#include "io_stream.h"

/* legacy wrapper.
 * Clients should use io_stream.get_size() */
size_t
get_file_size (String const &name)
{
  io_stream *theFile = io_stream::open (name, "rb");
  if (!theFile)
    /* To consider: throw an exception ? */
    return 0;
  ssize_t rv = theFile->get_size();
  delete theFile;
  return rv;
}

String 
base (String const &aString)
{
  if (!aString.size())
    return 0;
  const char *t = aString.cstr();
  const char *s = t;
  String rv = s;
  while (*s)
    {
      if ((*s == '/' || *s == ':' || *s == '\\') && s[1])
	rv = s + 1;
      s++;
    }
  delete[] t;
  return rv;
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
parse_filename (String const &in_fn, fileparse & f)
{
  char *p, *ver;
  char fn[in_fn.size() + 1];
  strcpy (fn, in_fn.cstr_oneuse());
  
  int n = find_tar_ext (fn);

  if (!n)
    return 0;

  f.tail = fn + n;
  fn[n] = '\0';
  f.pkg = f.what = String();
  p = base (fn).cstr();
  for (ver = p; *ver; ver++)
    if (*ver == '-')
      if (isdigit (ver[1]))
	{
	  *ver++ = 0;
	  f.pkg = p;
	  break;
	}
      else if (strcasecmp (ver, "-src") == 0 ||
	       strcasecmp (ver, "-patch") == 0)
	{
	  *ver++ = 0;
	  f.pkg = p;
	  f.what = strlwr (ver);
	  ver = strchr (ver, '\0');
	  break;
	}

  if (!f.pkg.size())
    f.pkg = p;

  if (!f.what.size())
    {
      int n;
      char *p1 = strchr (ver, '\0');
      if ((p1 -= 4) >= ver && strcasecmp (p1, "-src") == 0)
	n = 4;
      else if ((p1 -= 2) >= ver && strcasecmp (p1, "-patch") == 0)
	n = 6;
      else
	n = 0;
      if (n)
	{
	  // get the 'src' or 'patch'.
	  f.what = p1 + 1;
	}
    }

  f.ver = *ver ? ver : "0.0";
  delete[] p;
  return 1;
}

String
backslash (String const & aString)
{
  char * tempString = aString.cstr();
  for (char *t = tempString; *t; t++)
    if (*t == '/')
      *t = '\\';
  String theString(tempString);
  delete[] tempString;
  return theString;
}
