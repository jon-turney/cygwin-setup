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

#include "win32.h"
#include "filemanip.h"
#include "find.h"

#include "FindVisitor.h"
#include <stdexcept>

using namespace std;

Find::Find(const std::string& starting_dir)
  : h(INVALID_HANDLE_VALUE)
{
  _start_dir = starting_dir;
  int l = _start_dir.size ();
  
  /* Ensure that _start_dir has a trailing slash if it doesn't already.  */
  if (l < 1 || (starting_dir[l - 1] != '/' && starting_dir[l - 1] != '\\'))
    _start_dir += '/';
}

Find::~Find()
{
  if (h != INVALID_HANDLE_VALUE && h)
    FindClose (h);
}

void
Find::accept (FindVisitor &aVisitor, int level)
{
  /* The usage of multibyte strings within setup is so entangled into
     the various C++ classes, it's very hard to disentangle it and use
     UNICODE strings throughout without ripping everything apart.
     On the other hand, we want to support paths > MAX_PATH, but this is
     only supported by the UNICODE API.
     What you see here is nothing less than a hack.  We get the string
     as a multibyte string, convert it, call the UNICODE FindFile functions,
     then convert the returned structure back to multibyte to call the
     visitor methods, which in turn call other methods expecting multibyte
     strings. */
  WIN32_FIND_DATAW w_wfd;

  size_t len = _start_dir.size() + 9;
  WCHAR wstart[len];
  mklongpath (wstart, _start_dir.c_str (), len);
  wcscat (wstart, L"\\*");

  h = FindFirstFileW (wstart, &w_wfd);

  if (h == INVALID_HANDLE_VALUE)
    return;

  do
    {
      if (wcscmp (w_wfd.cFileName, L".") == 0
	  || wcscmp (w_wfd.cFileName, L"..") == 0)
	continue;

      /* TODO: make a non-win32 file and dir info class and have that as the 
       * visited node 
       */
      WIN32_FIND_DATAA wfd;
      memcpy (&wfd, &w_wfd, sizeof (wfd));
      WideCharToMultiByte (CP_UTF8, 0, w_wfd.cFileName, MAX_PATH,
			   wfd.cFileName, MAX_PATH, NULL, NULL);
      if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	aVisitor.visitDirectory (_start_dir, &wfd, level);
      else
	aVisitor.visitFile (_start_dir, &wfd);
    }
  while (FindNextFileW (h, &w_wfd));
}
