/*
 * Copyright (c) 2001, Robert Collins.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Robert Collins  <rbtcollins@hotmail.com>
 *
 */

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include "concat.h"

#include "io_stream.h"
#include "compress.h"

#include "filemanip.h"
#include "hash.h"
#include "log.h"
/* io_stream needs a bit of tweaking to get rid of this. TODO */
#include "mount.h"
/* this goes at the same time */
#include "win32.h"

  
#include "category.h"

#include "package_version.h"
#include "cygpackage.h"
#include "package_meta.h"

static const char *standard_dirs[] = {
  "/bin",
  "/etc",
  "/lib",
  "/tmp",
  "/usr",
  "/usr/bin",
  "/usr/lib",
  "/usr/src",
  "/usr/local",
  "/usr/local/bin",
  "/usr/local/etc",
  "/usr/local/lib",
  "/usr/tmp",
  "/var/run",
  "/var/tmp",
  0
};

void
hash::add_subdirs (char const *tpath)
{
  char *nonp, *pp;
  char *path = strdup (tpath);
  for (nonp = path; *nonp == '\\' || *nonp == '/'; nonp++);
  for (pp = path + strlen (path) - 1; pp > nonp; pp--)
    if (*pp == '/' || *pp == '\\')
      {
	int i, s = 0;
	char c = *pp;
	*pp = 0;
	for (i = 0; standard_dirs[i]; i++)
	  if (strcmp (standard_dirs[i] + 1, path) == 0)
	    {
	      s = 1;
	      break;
	    }
	if (s == 0)
	  add (path);
	*pp = c;
      }
}

void
packagemeta::add_version (packageversion & thepkg)
{
  if (versionspace == versioncount)
    {
      packageversion **newversions = (packageversion **) realloc (versions,
								  sizeof
								  (packageversion
								   *) *
								  (versionspace
								   + 5));
      if (!newversions)
	{
	  //die badly
	  exit (101);
	}
      versions = newversions;
      versionspace += 5;
    }
  /* FIXME: insert in sorted order */
  /* FIXME: And handle unversioned items */
  versions[versioncount] = &thepkg;
  versioncount++;
}

/* assumption: package thepkg is already in the metadata list. */
void
packagemeta::set_installed (packageversion & thepkg)
{
  for (size_t n = 0; n < versioncount; n++)
    {
      if (versions[n] == &thepkg)
	{
	  installed = &thepkg;
	  return;
	}
    }
}

/* uninstall a package if it's installed */
void
packagemeta::uninstall ()
{
  if (installed)
    {
      /* this will need to be pushed down to the version, or even the source level
       * to allow differences between formats to be seamlessly managed
       * but for now: here is ok
       */
      hash dirs;
      const char *line = installed->getfirstfile ();
      while (line)
	{
	  dirs.add_subdirs (line);

	  char *d = cygpath ("/", line, NULL);
	  DWORD dw = GetFileAttributes (d);
	  if (dw != 0xffffffff && !(dw & FILE_ATTRIBUTE_DIRECTORY))
	    {
	      log (LOG_BABBLE, "unlink %s", d);
	      DeleteFile (d);
	    }
	  line = installed->getnextfile ();
	}
      installed->uninstall ();

      dirs.reverse_sort ();
      char *subdir = 0;
      while ((subdir = dirs.enumerate (subdir)) != 0)
	{
	  char *d = cygpath ("/", subdir, NULL);
	  if (RemoveDirectory (d))
	    log (LOG_BABBLE, "rmdir %s", d);
	}
    }
  installed = 0;
}


void
packagemeta::add_category (Category &cat)
{
  /* add a new record for the package list */
  /* TODO: alpabetical inserts ? */
  categories.register_category (cat.name);

  CategoryPackage *templink = new CategoryPackage (*this);
  /* tell the category we are linking from we exist */
  templink->next = cat.packages;
  cat.packages = templink;
}

char const *
packagemeta::SDesc () {return versions[0]->SDesc ();};
