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
typedef enum
{
  TRUST_UNKNOWN,
  TRUST_PREV,
  TRUST_CURR,
  TRUST_TEST,
  NTRUST,
} trusts;

typedef enum
{
  ACTION_UNKNOWN,
  /* Note that the next six items must be in the same order as the
     TRUST items above. */
  ACTION_PREV,
  ACTION_CURR,
  ACTION_TEST,
  ACTION_SKIP,
  ACTION_UNINSTALL,
  ACTION_REDO,
  ACTION_SRC_ONLY,
  ACTION_LAST,
  ACTION_ERROR,
  ACTION_SAME = 100,
  ACTION_SAME_PREV = ACTION_PREV + ACTION_SAME,
  ACTION_SAME_CURR = ACTION_CURR + ACTION_SAME,
  ACTION_SAME_TEST = ACTION_TEST + ACTION_SAME,
  ACTION_SAME_LAST
} actions;

typedef enum
{
  EXCLUDE_NONE = 0,
  EXCLUDE_BY_SETUP,
  EXCLUDE_NOT_FOUND
} excludes;

#define is_download_action(pkg) \
  ((pkg)->action == ACTION_PREV || \
   (pkg)->action == ACTION_CURR || \
   (pkg)->action == ACTION_TEST || \
   (pkg)->action == ACTION_REDO || \
   (pkg)->action == ACTION_SRC_ONLY)

#define is_upgrade_action(pkg) \
  (((pkg)->action >= ACTION_CURR && \
    (pkg)->action <= ACTION_TEST) || \
   (pkg)->action == ACTION_REDO)

#define is_uninstall_action(pkg) \
  (is_upgrade_action (pkg) || \
   (pkg)->action == ACTION_PREV || \
   (pkg)->action == ACTION_UNINSTALL)

#define SRCACTION_NO		0
#define SRCACTION_YES		1
typedef struct
{
  char *version;	/* version part of filename */
  char *install;	/* file name to install */
  int install_size;	/* in bytes */
  int install_exists;	/* install file exists on disk */
  char *source;		/* sources for installed binaries */
  int source_size;	/* in bytes */
  int source_exists;	/* source file exists on disk */
} Info;			/* +1 for TRUST_UNKNOWN */

typedef struct
{
  char *name;		/* package name, like "cygwin" */
  char *sdesc;		/* short description (replaces "name" if provided) */
  char *ldesc;		/* long description (multi-line) */
  actions action;	/* ACTION_* - only NEW and UPGRADE get installed */
  trusts trust;		/* TRUST_* (selects among info[] below) */
  int srcpicked;	/* SRCACTION_ */

  Info *installed;
  int installed_ix;
  excludes exclude;	/* true if this package should be excluded */

  Info info[NTRUST];	/* +1 for TRUST_UNKNOWN */
} Package;

extern Package *package;
extern int npackages;

#ifdef __cplusplus
extern "C" {
#endif

Package *new_package (char *name);
void	ini_init (char *string);
Package *getpkgbyname (const char *pkgname);

#ifdef __cplusplus
}
#endif
