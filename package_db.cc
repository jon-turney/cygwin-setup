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

packagedb::packagedb ()
{
  io_stream *db = 0;
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

		  packagemeta *pkg = packages.getbykey (pkgname);
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
		  packages.registerbyobject (*pkg);

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
  for (size_t n = 1; n < packages.number (); n++)
    {
      packagemeta & pkgm = *packages[n];
      if (pkgm.installed)
	{
	  char line[2048];

	  /* size here is irrelevant - as we can assume that this install source
	   * no longer exists, and it does not correlate to used disk space
	   * also note that we are writing a fictional install source 
	   * to keep cygcheck happy.               
	   */
	  sprintf (line, "%s %s %d\n", pkgm.name,
		   concat (pkgm.name, "-",
			   pkgm.installed->Canonical_version (),
			   ".tar.bz2", 0), 0);
	  ndb->write (line, strlen (line));
	}
    }

  delete ndb;

  io_stream::remove (odbn);

  if (io_stream::move (ndbn, odbn))
    return errno ? errno : 1;
  return 0;
}

int
  packagedb::installeddbread =
  0;
list < packagemeta, char const *,
  strcasecmp >
  packagedb::packages;
list < Category, char const *,
  strcasecmp >
  packagedb::categories;
PackageDBActions
  packagedb::task =
  PackageDB_Install;
