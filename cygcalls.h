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

#include "../cygwin/include/sys/mount.h"
#include "../cygwin/include/mntent.h"

/* returns zero on success, nonzero on failure */
int cygcall_load_dll (char *name); 

int cygcall_unload_dll (); 
