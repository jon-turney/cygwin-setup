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

/* strarry.c: implementation of the strarry struct routines. */
#include <malloc.h>
#include <windows.h>
#include "setup.h"
#include "strarry.h"

void
sa_init (SA * array)
{
  array->count = 0;
  array->array = NULL;
}

void
sa_add (SA * array, const char *str)
{
  array->array = array->count
    ? xrealloc (array->array, sizeof (char *) * (array->count + 1))
  : xmalloc (sizeof (char *));
  array->array[array->count++] = xstrdup (str);
}

void
sa_cleanup (SA * array)
{
  size_t n = array->count;
  while (n--)
    xfree (array->array[n]);
  xfree (array->array);
}
