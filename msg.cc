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

/* The purpose of this file is to centralize all the message
   functions. */

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include "win32.h"
#include <stdio.h>
#include <stdarg.h>
#include "dialog.h"
#include "log.h"

void
msg (const char *fmt, ...)
{
  char buf[2000];
  va_list args;
  va_start (args, fmt);
  vsnprintf (buf, 2000, fmt, args);
  OutputDebugString (buf);
}

static int
mbox (HWND owner, const char *name, int type, int id, va_list args)
{
  char buf[1000], fmt[1000];

  if (LoadString (hinstance, id, fmt, sizeof (fmt)) <= 0)
    ExitProcess (0);

  vsnprintf (buf, 1000, fmt, args);
  log (LOG_PLAIN, String ("mbox ") + name + ": " + buf);
  return MessageBox (owner, buf, "Cygwin Setup", type);
}

void
note (HWND owner, int id, ...)
{
  va_list args;
  va_start (args, id);
  mbox (owner, "note", 0, id, args);
}

void
fatal (HWND owner, int id, ...)
{
  va_list args;
  va_start (args, id);
  mbox (owner, "fatal", 0, id, args);
  LogSingleton::GetInstance().exit (1);
}

int
yesno (HWND owner, int id, ...)
{
  va_list args;
  va_start (args, id);
  return mbox (owner, "yesno", MB_YESNO, id, args);
}
