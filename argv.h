/*
 * Copyright (c) 2001, Red Hat
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by R B Collins <rbtcollins@hotmail.com>
 *
 */

#define FAST_FUNCTION  __attribute__ ((stdcall, regparm(2)))

/* Returns 0 on success.
 * Convert the windows commandline to an argc,argv pair 
 */

int CommandLineToArgV(int &argc, char **&argv) FAST_FUNCTION;
