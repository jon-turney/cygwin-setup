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

      if (db->gets (line, 1000))
	{
	  int dbver;
	  sscanf (line, "%s %d", pkgname, &instsz);
	  if (!strcasecmp (pkgname, "INSTALLED.DB") && instsz == 2)
	    dbver = 2;
	  else
	    dbver = 1;
	  delete db;
	  /* Later versions may not use installed.db other than to record the version. */
	  if (dbver == 1 || dbver == 2)
	    {
	      db =
		io_stream::open ("cygfile:///etc/setup/installed.db", "rt");
	      if (dbver == 2)
		db->gets (line, 1000);
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
		    new cygpackage (pkgname, inst, instsz, f.ver,
				    package_installed,
				    package_binary);

		  pkg->add_version (*binary);
		  pkg->set_installed (*binary);
		  addpackage (*pkg);

		}
	      delete db;
	    }
	  else
	    // unknown dbversion
	    exit (1);
	}
      installeddbread = 1;
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
      packagemeta **newpackages = (packagemeta **) realloc (packages,
							    sizeof
							    (packagemeta *) *
							    (packagespace +
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
CategoryList
  packagedb::categories;
