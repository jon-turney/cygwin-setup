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

#ifndef SETUP_CISTRING_H
#define SETUP_CISTRING_H

// Yep, another string class

#include "win32.h"

class cistring
{
  TCHAR *buffer;
public:
    cistring ()
  {
    buffer = NULL;
  };
  cistring (const TCHAR * s);
  ~cistring ()
  {
    if (buffer != NULL)
      delete[]buffer;
  };

  const TCHAR *c_str ()
  {
    return buffer;
  };

  DWORD Format (UINT i, ...);
};

#endif /* SETUP_CISTRING_H */
