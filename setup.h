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

#include <stddef.h>

/* Routines in error.c. */
void lowmem ();			/* Report low memory and exit the
				   application. */
void winerror ();		/* Report errors from GetLastError. */

/* Routines in memory.c. */
void *xmalloc (size_t);		/* Like malloc, but exit on error. */
void *xrealloc (void *, size_t);	/* Like realloc, but exit on error. */
char *xstrdup (const char *);	/* Like strdup, but exit on error. */

#define xfree(ptr) free(ptr)

char *pathcat (const char *arg1, const char *arg2);
char *pathcvt (char target, const char *path);
char *dtoupath (const char *path);
char *utodpath (const char *path);

int xsystem (const char *cmd);
DWORD xcreate_process (int wait, HANDLE in, HANDLE out, HANDLE err, const char *cmd);
