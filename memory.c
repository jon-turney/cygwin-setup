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

#include <malloc.h>
#include <string.h>
#include <windows.h>
#include "setup.h"

void *
xmalloc (size_t size)
{
  void *mem = malloc (size);

  if (!mem)
    lowmem ();
  return mem;
}

void *
xrealloc (void *orig, size_t newsize)
{
  void *mem = realloc (orig, newsize);

  if (!mem)
    lowmem ();
  return mem;
}

char *
xstrdup (const char *arg)
{
  char *str = strdup (arg);

  if (!str)
    lowmem ();
  return str;
}
