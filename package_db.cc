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

#include "package_version.h"
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
  db = 0;
  curr_package = 0;
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
	  db = 0;
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
		      pkg = new packagemeta (pkgname, inst);
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
		  pkg->desired = pkg->installed;
		  addpackage (*pkg);

		}
	      delete db;
	      db = 0;
	    }
	  else
	    // unknown dbversion
	    exit (1);
	}
      installeddbread = 1;
    }
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
  if (packagecount > curr_package)
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
  size_t n;
  for (n = 0;
       n < packagecount
       && strcasecmp (packages[n]->name, newpackage.name) < 0; n++);
  /* insert at n */
  memmove (&packages[n + 1], &packages[n],
	   (packagecount - n) * sizeof (packagemeta *));
  packages[n] = &newpackage;
  packagecount++;
  return 0;
}

int
packagedb::flush ()
{
  /* naive approach - just dump the lot */
  char const *odbn = "cygfile:///etc/setup/installed.db";
  char const *ndbn = "cygfile:///etc/setup/installed.db.new";

  io_stream::mkpath_p (PATH_TO_FILE, ndbn);

  io_stream *ndb = io_stream::open (ndbn, "wb");

  if (!ndb)
    return errno ? errno : 1;

  ndb->write ("INSTALLED.DB 2\n", strlen ("INSTALLED.DB 2\n"));
  if (getfirstpackage ())
    {
      packagemeta *pkgm = getfirstpackage ();
      while (pkgm)
	{
	  if (pkgm->installed)
	    {
	      char line[2048];

	      /* size here is irrelevant - as we can assume that this install source
	       * no longer exists, and it does not correlate to used disk space
	       * also note that we are writing a fictional install source 
	       * to keep cygcheck happy.               
	       */
	      sprintf (line, "%s %s %d\n", pkgm->name,
		       concat (pkgm->name, "-",
			       pkgm->installed->Canonical_version (),
			       ".tar.bz2", 0), 0);
	      ndb->write (line, strlen (line));
	    }
	  pkgm = getnextpackage ();
	}
    }

  delete ndb;

  io_stream::remove (odbn);

  if (io_stream::move (ndbn, odbn))
    return errno ? errno : 1;
  return 0;
}

packagemeta & packagedb::registerpackage (char const *pkgname)
{
  packagemeta *
    tmp =
    getpackagebyname (pkgname);
  if (tmp)
    return *tmp;
  tmp = new packagemeta (pkgname);
  addpackage (*tmp);
  return *tmp;
}


packagemeta **
  packagedb::packages =
  0;
size_t packagedb::packagecount = 0;
size_t packagedb::packagespace = 0;
int
  packagedb::installeddbread =
  0;
list < Category, char const *,
  strcasecmp >
  packagedb::categories;
