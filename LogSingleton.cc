/*
 * Copyright (c) 2002, Robert Collins..
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

#include "LogFile.h"
#include <stdarg.h>

/* Helper functions */

// Log adapators for printf-style output
void
LogBabblePrintf(const char *fmt, ...)
{
  int len;
  char buf[8192];
  va_list args;
  va_start (args, fmt);
  len = vsnprintf (buf, 8192, fmt, args);
  if ((len > 0) && (buf[len-1] == '\n'))
    buf[len-1] = 0;
  Log (LOG_BABBLE) << buf << endLog;
}

void
LogPlainPrintf(const char *fmt, ...)
{
  int len;
  char buf[8192];
  va_list args;
  va_start (args, fmt);
  len = vsnprintf (buf, 8192, fmt, args);
  if ((len > 0) && (buf[len-1] == '\n'))
    buf[len-1] = 0;
  Log (LOG_PLAIN) << buf << endLog;
}
