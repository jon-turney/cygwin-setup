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
  NTRUST
} trusts;

typedef enum
{
  /* Note that the next four items must be in the same order as the
     TRUST items above. */
  ACTION_UNKNOWN,
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
  /* Actions taken when installed version matches the selected version. */
  ACTION_SAME_PREV = ACTION_PREV + ACTION_SAME,
  ACTION_SAME_CURR = ACTION_CURR + ACTION_SAME,
  ACTION_SAME_TEST = ACTION_TEST + ACTION_SAME,
  /* Last action. */
  ACTION_SAME_LAST
} actions;

typedef enum
{
  EXCLUDE_NONE = 0,
  EXCLUDE_BY_SETUP,
  EXCLUDE_NOT_FOUND
} excludes;

typedef enum 
{
  VIEW_UNKNOWN,
  VIEW_PACKAGE_FULL,
  VIEW_PACKAGE,
  VIEW_CATEGORY,
  NVIEW
} views;

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
typedef struct _Info
{
  char *version;	/* version part of filename */
  char *install;	/* file name to install */
  int install_size;	/* in bytes */
  int install_exists;	/* install file exists on disk */
  char *source;		/* sources for installed binaries */
  int source_size;	/* in bytes */
  int source_exists;	/* source file exists on disk */
#ifdef __cplusplus
  _Info (const char *_install, const char *_version,
	 int _install_size, const char *_source = NULL,
	 int _source_size = 0);
#endif
} Info;			/* +1 for TRUST_UNKNOWN */

typedef struct _CategoryPackage
{
  struct _CategoryPackage *next;  /* The next package pointer in the list */
  char *pkg;			  /* This should be Package *, but the packages can move*/
} CategoryPackage;

typedef struct _Category
{
  struct _Category *next; /* the next category in the list */
  char *name;		  /* the category */
  CategoryPackage *packages; /* the packages in this category */
} Category;

typedef struct _Dependency
{
  struct _Dependency *next; /* the next package in this dependency list */
  char *package;	/* the name of the package that is depended on */
} Dependency; 		/* Dependencies can be used for
			   recommended/required/related... */
typedef struct
{
  char *name;		/* package name, like "cygwin" */
  char *sdesc;		/* short description (replaces "name" if provided) */
  char *ldesc;		/* long description (multi-line) */
  Category *category;   /* the categories the package belongs to */ 
  Dependency *required; /* the packages required for this package to work */
  actions action;	/* A range of states applicable to this package */
  trusts trust;		/* Selects among info[] below, a subset of action */
  int srcpicked;	/* True if source is required */

  Info *installed;	/* Info on installed package */
  trusts installed_ix;	/* Index into info array for currently installed package */
  excludes exclude;	/* true if this package should be excluded */

  /* The reason for this weird layout is to allow for loops that scan either
     the info array, based on trust value or the infoscan array based on a pointer,
     looking for a particular version. */
  Info info[1];			/* First element.  Intentionally allocated prior
				   to infoscan */
  Info infoscan[NTRUST - 1];	/* +1 for TRUST_UNKNOWN */
  Info infoend[0];		/* end marker */
} Package;

extern Package *package;
extern int npackages;
extern Category *category;
extern int ncategories;

#ifdef __cplusplus
extern "C" {
#endif

Package *new_package (char *name);
void	ini_init (char *string);
Package *getpkgbyname (const char *pkgname);
void    new_requirement (Package *package, char *dependson);
Category *getcategorybyname (const char *categoryname);
Category *getpackagecategorybyname (Package *pkg, const char *categoryname); 
Category *register_category (char *name);
void    add_category (Package *package, Category *cat);

#ifdef __cplusplus
}
#endif

#endif /* _INI_H_ */
