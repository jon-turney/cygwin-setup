/*
 * Copyright (c) 2002, Robert Collins
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Robert Collins <rbtcollins@hotmail.com>
 *
 */

/* The purpose of this file is to centralize all the message
   functions. */

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include "win32.h"

enum Win32::_os
Win32::OS ()
{
  OSVERSIONINFO VersionInfo;
  VersionInfo.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
  if (!GetVersionEx (&VersionInfo))
    {
      // Throw an exception or something ??
    }
  if (VersionInfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)
    return Win9x;
  else
    return WinNT;
}
