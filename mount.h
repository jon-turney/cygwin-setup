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

/* Finds the existing root mount, or returns NULL.  istext is set to
   nonzero if the existing mount is a text mount, else zero for
   binary. */

char *	find_mount (int *istext, int *issystem, char *path);

/* Similar to the mount and umount functions, but simplified */

void	create_mount (char *posix, char *win32, int istext, int issystem);
void	remove_mount (char *posix);
void	read_mounts ();

/* Sets the cygdrive flags.  Used to make the automounted drives' binary/text
mode consistent with the standard Cygwin mounts. */

void set_cygdrive_flags (int istext, int issystem);
char *cygpath (const char *s, ...);
