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

#endif

#endif /* _INI_H_ */
