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

/* The purpose of this file is to handle the case where we're
   installing from files that already exist in the current directory.
   If a setup.ini file is present, we set the mirror site to "." and
   pretend we're installing from the `internet' ;-) else we have to
   find all the .tar.gz files, deduce their versions, and try to
   compare versions in the case where the current directory contains
   multiple versions of any given package.  We do *not* try to compare
   versions with already installed packages; we always choose a
   package in the current directory over one that's already installed
   (otherwise, why would you have asked to install it?).  Note
   that we search recursively. */

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include "win32.h"

#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <unistd.h>
#include <ctype.h>

#include "ini.h"
#include "resource.h"
#include "concat.h"
#include "state.h"
#include "dialog.h"
#include "msg.h"
#include "find.h"
#include "filemanip.h"
#include "version.h"
#include "site.h"
#include "rfc1738.h"

#include "port.h"

#include "package_db.h"
#include "package_meta.h"
#include "package_version.h"
# if 0
static int
is_test_version (char *v)
{
  int i;
  for (i = 0; v[i] && isdigit (v[i]); i++);
  return (i >= 6) ? 1 : 0;
}
#endif

static void
found_file (char *path, unsigned int fsize)
{
  fileparse f;

  if (!parse_filename (path, f))
    return;

  if (f.what[0] != '\0')
    return;

  packagemeta *p = NULL;
  packagedb db;
  p = db.getpackagebyname (f.pkg);
  if (p == NULL)
    p = new packagemeta (f.pkg, path);

#if 0
  // This is handled by the scan2 - there is no need for duplication - or is there?

  int trust = is_test_version (f.ver) ? TRUST_TEST : TRUST_CURR;

  /* See if this version is older than what we have */
  if (p->info[trust].version)
    {
      char *ov = canonicalize_version (p->info[trust].version);
      char *nv = canonicalize_version (f.ver);
      if (strcmp (ov, nv) > 0)
	return;
    }

  if (p->info[trust].version)
    free (p->info[trust].version);
  p->info[trust].version = _strdup (f.ver);

  if (p->info[trust].install)
    free (p->info[trust].install);
  p->info[trust].install = _strdup (path);

  p->info[trust].install_size = fsize;
#endif
}

static bool found_ini;

static void
check_ini (char *path, unsigned int fsize)
{
  if (fsize && strstr (path, "setup.ini"))
    found_ini = true;
}

void
do_fromcwd (HINSTANCE h)
{
  found_ini = true;
  find (".", check_ini);
  if (found_ini)
  {
      next_dialog = IDD_S_LOAD_INI;
      return;
    }

  next_dialog = IDD_CHOOSE;

  find (".", found_file);

#if 0
  // Reinstate this FIXME: 
  // Now see about source tarballs
  int i, t;
  packagemeta *p;
  char srcpath[_MAX_PATH];
  for (i = 0; i < npackages; i++)
    {
      p = package + i;
      /* For each version with a binary after running find */
      for (t = TRUST_PREV; t <= TRUST_TEST; t++)
	if (p->info[t].install)
	  {
	    /* Is there a -src file too? */
	    int n = find_tar_ext (p->info[t].install);
	    strcpy (srcpath, p->info[t].install);
	    strcpy (srcpath + n, "-src.tar.gz");
	    msg ("looking for %s", srcpath);

	    WIN32_FIND_DATA wfd;
	    HANDLE h = FindFirstFile (srcpath, &wfd);
	    if (h == INVALID_HANDLE_VALUE)
	      {
		strcpy (srcpath + n, "-src.tar.bz2");
		h = FindFirstFile (srcpath, &wfd);
	      }
	    if (h != INVALID_HANDLE_VALUE)
	      {
		msg ("-- got it");
		FindClose (h);
		p->info[t].source = _strdup (srcpath);
		p->info[t].source_size = wfd.nFileSizeLow;
	      }
	  }
    }
#endif
  return;
}
