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

/* The purpose of this file is to hide the mess needed just to figure
   out how full a given disk is.  There is an old API that can't
   handle disks bigger than 2G, and a new API that isn't always
   available. */

#include "win32.h"
#include "filemanip.h"
#include "diskfull.h"

int
diskfull (const std::string& path)
{
  ULARGE_INTEGER avail, total, free;
  size_t len = path.size () + 7;
  WCHAR wpath[len];

  mklongpath (wpath, path.c_str (), len);
  if (GetDiskFreeSpaceExW (wpath, &avail, &total, &free))
    {
      int perc = avail.QuadPart * 100 / total.QuadPart;
      return 100 - perc;
    }
  return 0;
}
