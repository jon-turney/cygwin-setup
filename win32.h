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
 * and Robert Collins <rbtcollins@hotmail.com>
 *
 */

/* The inital purpose of this file was limit the number of Win32 headers we
   actually have to parse.  The Setup program only uses a few of them,
   so there's no point in parsing them all (even lean-n-mean).  Doing
   this cuts compile time in half. Now it's also the header for the Win32
   class which provides runtime information about the OS
 */

#ifndef SETUP_WIN32_H
#define SETUP_WIN32_H

#define NOCOMATTRIBUTE

#include <stdarg.h>

#define WIN32_LEAN_AND_MEAN
/* We want a minimum IE version of 3 */
#define _WIN32_IE 0x0300
#include <windows.h>

#include <wininet.h>
#include <windowsx.h>

/* =========================== */
#ifdef __cplusplus
class Win32 {
  public:
    enum _os {
      Win9x,
      WinNT
    };
  static enum _os OS ();
};

#endif //_cplusplus
#endif // SETUP_WIN32_H
