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

/* this is the package database class.
 * It lists all known packages, including custom ones, ones from a mirror and
 * installed ones.
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

#include "package.h"
#include "cygpackage.h"
#include "package_db.h"
#include "package_meta.h"

/* static members */

packagemeta *
packagedb::getpackagebyname (const char *search)
{
  /* dumb search, we can add a index and do a btree later */
  size_t index = 0;
  while (index < packagecount)
    {
      if (!strcasecmp ((*packages[index]).name, search))
	return packages[index];
      index++;
    }
  return 0;
}

packagedb::packagedb ()
{
  if (!installeddbread)
    {
      /* no parameters. Read in the local installation database. */
      db = io_stream::open ("cygfile:///etc/setup/installed.db", "rt");
      if (!db)
	return;
      /* flush_local_db_package_data */
      char line[1000], pkgname[1000], inst[1000], src[1000];
      int instsz, srcsz;
      while (db->gets (line, 1000))
	{
	  int parseable;
	  src[0] = 0;
	  srcsz = 0;
	  sscanf (line, "%s %s %d %s %d", pkgname, inst, &instsz, src,
		  &srcsz);
	  fileparse f;
	  parseable = parse_filename (inst, f);
	  if (!parseable)
	    continue;

	  packagemeta *pkg = getpackagebyname (pkgname);
	  if (!pkg)
	    {
	      pkg = new packagemeta (pkgname);
	      /* we should install a new handler then not check this...
	       */
	      //if (!pkg)
	      //die badly
	    }

	  cygpackage *binary =
	    new cygpackage (pkgname, inst, instsz, f.ver, package_installed,
			    package_binary);

	  pkg->add_version (*binary);
	  pkg->set_installed (*binary);
	  addpackage (*pkg);

#if 0
	  if (src[0] && srcsz)
	    {
	      cygpackage *source =
		new cygpackage (pkgname, src, srcsz, f.ver, package_installed,
				package_source);
	    }
#endif

#if 0
	  if (pkg == NULL)
	    {
	      pkg = new_package (pkgname);
	      pkg->info[TRUST_CURR].version = strdup (f.ver);
	      pkg->info[TRUST_CURR].install = strdup (inst);
	      pkg->info[TRUST_CURR].install_size = instsz;
	      if (src[0] && srcsz)
		{
		  pkg->info[TRUST_CURR].source = strdup (src);
		  pkg->info[TRUST_CURR].source_size = srcsz;
		}
	      pkg->installed_ix = TRUST_CURR;
	      /* Exists on local system but not on download system */
	      pkg->exclude = EXCLUDE_NOT_FOUND;
	    }
	  pkg->installed = new Info (inst, f.ver, instsz);

	  if (!pkg->installed_ix)
	    for (trusts t = TRUST_PREV; t < NTRUST; ((int) t)++)
	      if (pkg->info[t].install
		  && strcmp (f.ver, pkg->info[t].version) == 0)
		{
		  pkg->installed_ix = t;
		  break;
		}
#endif
	}
      delete db;
    }
  db = 0;
  curr_package = 0;
}

packagemeta *
packagedb::getfirstpackage ()
{
  curr_package = 0;
  if (packages)
    return packages[0];
  return 0;
}

packagemeta *
packagedb::getnextpackage ()
{
  curr_package++;
  if (packagecount >= curr_package)
    return packages[curr_package];
  return 0;
}

int
packagedb::addpackage (packagemeta & newpackage)
{
  if (getpackagebyname (newpackage.name))
    return 1;
  /* lock the mutex */
  if (packagecount == packagespace)
    {
      packagemeta **newpackages =
	(packagemeta **) realloc (packages,
				  sizeof (packagemeta *) * (packagespace +
							    100));
      if (!newpackages)
	{
	  //die
	  exit (100);
	}
      packages = newpackages;
      packagespace += 100;
    }
  packages[packagecount] = &newpackage;
  packagecount++;
  return 0;
}

packagemeta **
  packagedb::packages =
  0;
size_t
  packagedb::packagecount =
  0;
size_t
  packagedb::packagespace =
  0;
int
  packagedb::installeddbread =
  0;
