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

/* When setup.ini is parsed, the information is stored according to
   the declarations here.  ini.cc (via inilex and iniparse)
   initializes these structures.  choose.cc sets the action and trust
   fields.  download.cc downloads any needed files for selected
   packages (the chosen "install" field).  install.cc installs
   selected packages. */

#define YYSTYPE char *

/* lowest number must be most trusted, highest least trusted */
#define TRUST_PREV	0
#define TRUST_CURR	1
#define TRUST_TEST	2
#define TRUST_UNKNOWN	3 /* intentionally not in NTRUST */
#define NTRUST 3

#define ACTION_UNKNOWN	0
#define ACTION_SAME	1
#define ACTION_NEW	2
#define ACTION_UPGRADE	3
#define ACTION_ERROR	4

typedef struct {
  char *name;	/* package name, like "cygwin" */
  char *sdesc;	/* short description (replaces "name" if provided) */
  char *ldesc;	/* long description (multi-line) */
  int action;	/* ACTION_* - only NEW and UPGRADE get installed */
  int trust;	/* TRUST_* (selects among info[] below) */
  struct {
    char *version;	/* holds canonical version for fromcwd.cc */
    char *install;	/* file name to install */
    int install_size;	/* in bytes */
    char *source;	/* sources for installed binaries */
    int source_size;	/* in bytes */
  } info[NTRUST+1];	/* +1 for TRUST_UNKNOWN */
} Package;

extern Package *package;
extern int npackages;

#ifdef __cplusplus
extern "C" {
#endif

Package *new_package (char *name);
void	ini_init (char *string);

#ifdef __cplusplus
}
#endif
