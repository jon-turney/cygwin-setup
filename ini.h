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

#ifndef _INI_H_
#define _INI_H_


#ifdef __cplusplus
extern "C"
{
#endif
  void ini_init (char *string, char *mirror);
#define YYSTYPE char *

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
/* For enums */
#include "choose.h"

/* When setup.ini is parsed, the information is stored according to
   the declarations here.  ini.cc (via inilex and iniparse)
   initializes these structures.  choose.cc sets the action and trust
   fields.  download.cc downloads any needed files for selected
   packages (the chosen "install" field).  install.cc installs
   selected packages. */

/* the classes here store installation info *shrug* */

/* lowest number must be most trusted, highest least trusted */
typedef enum
{
  TRUST_UNKNOWN,
  TRUST_PREV,
  TRUST_CURR,
  TRUST_TEST,
  NTRUST
}
trusts;

typedef enum
{
  EXCLUDE_NONE = 0,
  EXCLUDE_BY_SETUP,
  EXCLUDE_NOT_FOUND
}
excludes;

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

#define is_full_action(pkg) \
  (((pkg)->action >= ACTION_SAME_PREV && (pkg)->action <= ACTION_SAME_TEST) \
   || (pkg)->action == ACTION_SKIP)

#define SRCACTION_NO		0
#define SRCACTION_YES		1
typedef struct _Info
{
  char *version;		/* version part of filename */
  char *install;		/* file name to install */
  unsigned int install_size;	/* in bytes */
  int install_exists;		/* install file exists on disk */
  int derived;			/* True if we "figured out" that this version should
				   go here but it was not in setup.ini */
  char *source;			/* sources for installed binaries */
  unsigned int source_size;	/* in bytes */
  int source_exists;		/* source file exists on disk */
    _Info ():version (0), install (0), install_size (0), install_exists (0),
    derived (0), source (0), source_size (0)
  {
  };
  _Info (const char *_install, const char *_version,
	 int _install_size, const char *_source = NULL, int _source_size = 0);
}

Info;				/* +1 for TRUST_UNKNOWN */

#if 0
class Package
{
public:
  Package ():name (0), sdesc (0), ldesc (0), category (0), required (0),
    srcpicked (0), installed (0)
  {
  };
  char *name;			/* package name, like "cygwin" */
  char *sdesc;			/* short description (replaces "name" if provided) */
  char *ldesc;			/* long description (multi-line) */
  Category *category;		/* the categories the package belongs to */
  Dependency *required;		/* the packages required for this package to work */
  actions action;		/* A range of states applicable to this package */
  trusts trust;			/* Selects among info[] below, a subset of action */
  int srcpicked;		/* True if source is required */

  Info *installed;		/* Info on installed package */
  trusts installed_ix;		/* Index into info array for currently installed package */
  excludes exclude;		/* true if this package should be excluded */

  /* The reason for this weird layout is to allow for loops that scan either
     the info array, based on trust value or the infoscan array based on a pointer,
     looking for a particular version. */
  Info info[1];			/* First element.  Intentionally allocated prior
				   to infoscan */
  Info infoscan[NTRUST - 1];	/* +1 for TRUST_UNKNOWN */
  Info infoend[0];		/* end marker */
};

extern Package *package;
extern int npackages;


Package *new_package (char *name);
Package *getpkgbyname (const char *pkgname);
void new_requirement (Package * package, char *dependson);
Category *getpackagecategorybyname (Package * pkg, const char *categoryname);
void add_category (Package * package, Category * cat);
#endif




#endif

#endif /* _INI_H_ */
