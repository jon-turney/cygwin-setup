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

/* The purpose of this file is to limit the number of Win32 headers we
   actually have to parse.  The Setup program only uses a few of them,
   so there's no point in parsing them all (even lean-n-mean).  Doing
   this cuts compile time in half. */

#define _UNION_NAME(x)
#define _STRUCT_NAME(x)
#define NOCOMATTRIBUTE

#include <stdarg.h>
#include <windef.h>
#include <basetyps.h>
#include <winbase.h>
#include <wingdi.h>
#include <winuser.h>
#include <wininet.h>

#include <windowsx.h>
