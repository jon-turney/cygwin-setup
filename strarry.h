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

/* strarry.h: strarry struct */

#include <stddef.h>
typedef struct strarry
{
  char **array;
  size_t count;
  size_t index;
} SA;
void sa_init (SA *);		/* Initialize the struct. */
void sa_add (SA *, const char *);	/* Add a string to the array. */
void sa_cleanup (SA *);	/* Deallocate all of the memory. */

