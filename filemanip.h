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
 * Written by Christopher Faylor <cgf@cygnus.com>
 *
 */

extern int find_tar_ext (const char *path);

typedef struct
{
  char pkgtar[MAX_PATH + 1];
  char pkg[MAX_PATH + 1];
  char ver[MAX_PATH + 1];
  char tail[MAX_PATH + 1];
  char what[16];
} fileparse;

int parse_filename (const char *in_fn, fileparse& f);
