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

#ifndef SETUP_INI_H
#define SETUP_INI_H

class io_stream;
#include <string>
class IniState;
class IniDBBuilder;
class IniParseFeedback;
void ini_init (io_stream *, IniDBBuilder *, IniParseFeedback &);
#define YYSTYPE char *

#ifdef __cplusplus

/* When setup.ini is parsed, the information is stored according to
   the declarations here.  ini.cc (via inilex and iniparse)
   initializes these structures.  choose.cc sets the action and trust
   fields.  download.cc downloads any needed files for selected
   packages (the chosen "install" field).  install.cc installs
   selected packages. */

typedef enum
{
  EXCLUDE_NONE = 0,
  EXCLUDE_BY_SETUP,
  EXCLUDE_NOT_FOUND
}
excludes;

#define SETUP_INI_FILENAME (IsWindowsNT () ? "setup.ini" : "setup_9x.ini")
#define SETUP_BZ2_FILENAME (IsWindowsNT () ? "setup.bz2" : "setup_9x.bz2")

#endif

#endif /* SETUP_INI_H */
