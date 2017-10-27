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

#ifndef SETUP_FILEMANIP_H
#define SETUP_FILEMANIP_H

#include <sys/types.h>
#include <string>

extern int find_tar_ext (const char *path);

struct fileparse
{
  std::string pkg;
  std::string ver;
  std::string tail;
  std::string what;
};

int parse_filename (const std::string& fn, fileparse & f);
size_t get_file_size (const std::string& );
std::string backslash (const std::string& s);
const char * trail (const char *, const char *);
int mklongpath (wchar_t *tgt, const char *src, size_t len);
FILE *nt_wfopen (const wchar_t *wpath, const char *mode, mode_t perms);
FILE *nt_fopen (const char *path, const char *mode, mode_t perms = 0644);

#endif /* SETUP_FILEMANIP_H */
