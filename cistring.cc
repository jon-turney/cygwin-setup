/*
 * Copyright (c) 2001, Gary R. Van Sickle.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Gary R. Van Sickle <g.r.vansickle@worldnet.att.net>
 *
 */

// Yep, another string class

#include "cistring.h"
#include <stdlib.h>

DWORD
cistring::Format (UINT i, ...)
{
  TCHAR FormatStringBuffer[256];
  TCHAR *Buff;
  va_list arglist;
  DWORD numchars;

  // Get the string from the stringtable (FormatMessage() can only work with
  // literal strings or *message*table entries, which are different for some
  // inexplicable reason).
  LoadString (GetModuleHandle (NULL), i, FormatStringBuffer, 256);

  va_start (arglist, i);
  numchars =::
    FormatMessage (FORMAT_MESSAGE_ALLOCATE_BUFFER |
		   FORMAT_MESSAGE_FROM_STRING, FormatStringBuffer, i, 0,
		   (LPTSTR) & Buff, 0, &arglist);
  va_end (arglist);

  if (numchars == 0)
    {
      // Something went wrong.
      return 0;
    }

  buffer = new TCHAR[(numchars + 1) * sizeof (TCHAR)];
  memcpy (buffer, Buff, (numchars + 1) * sizeof (TCHAR));
  LocalFree (Buff);

  return numchars;
}
