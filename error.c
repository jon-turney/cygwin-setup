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
 * Written by Ron Parker <parkerrd@hotmail.com>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include "setup.h"

void
lowmem ()
{
  fputs ("Insufficient memory.\n", stderr);
  exit (1);
}

void
winerror ()
{
  LPVOID msgbuf;
  int lasterr = GetLastError ();

  /* If !lasterr then assume a standard file error happened and was
     already displayed. */
  if (lasterr)
    {
      FormatMessage (FORMAT_MESSAGE_ALLOCATE_BUFFER
		     | FORMAT_MESSAGE_FROM_SYSTEM
		     | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, lasterr,
		     MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
		     (LPTSTR) & msgbuf, 0, NULL);
      if (msgbuf)
	{
	  fprintf (stderr, "%s\n", msgbuf);
	  LocalFree (msgbuf);
	}
      else
	fprintf (stderr, "Unexpected error #%d\n", lasterr);
    }
}
