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
 * Written by DJ Delorie <dj@cygnus.com>
 * Rewritten by Robert Collins <rbtcollins@hotmail.com>
 *
 */

/* The purpose of this file is to doa recursive find on a given
   directory, calling a given function for each file found. */

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include "find.h"
#include "win32.h"
#include <stdlib.h>

//#include "port.h"

#include "String++.h"
#include "FindVisitor.h"
#include <stdexcept>

Find::Find(String const &starting_dir) : _start_dir (starting_dir), h(INVALID_HANDLE_VALUE)
{
}

Find::~Find()
{
  if (h != INVALID_HANDLE_VALUE && h)
    FindClose (h);
}

void
Find::accept (FindVisitor &aVisitor)
{
  WIN32_FIND_DATA wfd;
  if (_start_dir.size() > _MAX_PATH)
    throw new length_error ("starting dir longer than _MAX_PATH");

  h = FindFirstFile ((_start_dir + "/*").cstr_oneuse(), &wfd);

  if (h == INVALID_HANDLE_VALUE)
    return;

  String basePath = _start_dir + "/";

  do
    {
      if (strcmp (wfd.cFileName, ".") == 0
	  || strcmp (wfd.cFileName, "..") == 0)
	continue;

      /* TODO: make a non-win32 file and dir info class and have that as the 
       * visited node 
       */
      if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	aVisitor.visitDirectory (basePath, &wfd);
      else
	aVisitor.visitFile (basePath, &wfd);
    }
  while (FindNextFile (h, &wfd));
}
