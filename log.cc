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
 * Written by DJ Delorie <dj@redhat.com>
 *
 */

/* The purpose of this file is to centralize all the logging functions. */

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include "win32.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>

#include "resource.h"
#include "msg.h"
#include "log.h"
#include "dialog.h"
#include "state.h"
#include "mkdir.h"
#include "mount.h"

#include "io_stream.h"

void 
log (enum log_level level, String const &message)
{
  LogSingleton::GetInstance()(level) << message << endLog;
}

void
log (enum log_level level, const char *fmt, ...)
{
  char buf[1000];
  va_list args;
  va_start (args, fmt);
  vsprintf (buf, fmt, args);
  log (level, String(buf));
}
