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

#include "String++.h"

extern int find_tar_ext (const char *path);

typedef struct
{
  String pkgtar;
  String pkg;
  String ver;
  String tail;
  String what;
}
fileparse;

int parse_filename (String const & in_fn, fileparse & f);
String base (String const &);
unsigned int get_file_size (String const &);
String backslash (String const &);
