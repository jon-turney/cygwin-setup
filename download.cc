/*
 * Copyright (c) 2000, 2001, Red Hat, Inc.
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

/* The purpose of this file is to download all the files we need to
   do the installation. */

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include "win32.h"

#include <stdio.h>
#include <unistd.h>
#include <process.h>

#include "resource.h"
#include "msg.h"
#include "ini.h"
#include "dialog.h"
#include "concat.h"
#include "geturl.h"
#include "state.h"
#include "mkdir.h"
#include "log.h"
#include "filemanip.h"
#include "port.h"

#include "io_stream.h"

#include "package_db.h"
#include "package_meta.h"
#include "package_version.h"
#include "package_source.h"

#include "rfc1738.h"

#include "threebar.h"
extern ThreeBarProgressPage Progress;

/* 0 on failure
 */
static int
check_for_cached (packagesource & pkgsource)
{
  /* search algo:
     1) is there a legacy version in the cache dir available.
     (Note that the cache dir is represented by a mirror site of
     file://local_dir
   */

  DWORD size;
  if ((size =
       get_file_size (concat (local_dir, "/", pkgsource.Canonical (), 0))) >
      0)
    if (size == pkgsource.size)
      {
	pkgsource.
	  set_cached (concat
		      ("file://", local_dir, "/", pkgsource.Canonical (), 0));
	return 1;
      }

  /*
     2) is there a version from one of the selected mirror sites available ?
   */
  for (size_t n = 1; n <= pkgsource.sites.number (); n++)
    if ((size =
	 get_file_size (concat
			(local_dir, "/",
			 rfc1738_escape_part (pkgsource.sites[n]->key), "/",
			 pkgsource.Canonical (), 0))) > 0)
      if (size == pkgsource.size)
	{
	  pkgsource.
	    set_cached (concat
			("file://", local_dir, "/",
			 rfc1738_escape_part (pkgsource.sites[n]->
					      key), "/",
			 pkgsource.Canonical (), 0));
	  return 1;
	}

  return 0;
}

/* download a file from a mirror site to the local cache. */
static int
download_one (packagesource & pkgsource, HWND owner)
{
  if (check_for_cached (pkgsource) && source != IDC_SOURCE_DOWNLOAD)
    return 0;

  /* try the download sites one after another */

  int success = 0;
  for (size_t n = 1; n <= pkgsource.sites.number () && !success; n++)
    {
      const char *local = concat (local_dir, "/",
				  rfc1738_escape_part (pkgsource.sites[n]->
						       key), "/",
				  pkgsource.Canonical (), 0);
      io_stream::mkpath_p (PATH_TO_FILE, concat ("file://", local, 0));

      if (get_url_to_file
	  (concat
	   (pkgsource.sites[n]->key, "/", pkgsource.Canonical (), 0),
	   concat (local, ".tmp", 0), pkgsource.size, owner))
	{
	  /* FIXME: note new source ? */
	  continue;
	}
      else
	{
	  size_t size = get_file_size (concat (local, ".tmp", 0));
	  if (size == pkgsource.size)
	    {
	      log (0, "Downloaded %s", local);
	      if (_access (local, 0) == 0)
		remove (local);
	      rename (concat (local, ".tmp", 0), local);
	      success = 1;
	      pkgsource.set_cached (concat ("file://", local, 0));
	      continue;
	    }
	  else
	    {
	      log (0, "Download %s wrong size (%u actual vs %d expected)",
		   local, size, pkgsource.size);
	      remove (concat (local, ".tmp", 0));
	      continue;
	    }
	}
    }
  if (success)
    return 0;
  /* FIXME: Do we want to note this? if so how? */
  return 1;
}

static void
do_download_thread (HINSTANCE h, HWND owner)
{
  int errors = 0;
  total_download_bytes = 0;
  total_download_bytes_sofar = 0;

  packagedb db;
  /* calculate the amount needed */
  for (size_t n = 1; n < db.packages.number (); n++)
    {
      packagemeta & pkg = *db.packages[n];
      if (pkg.desired && (pkg.desired->srcpicked || pkg.desired->binpicked))
	{
	  packageversion *version = pkg.desired;
	  if (!
	      (check_for_cached (version->bin)
	       && source != IDC_SOURCE_DOWNLOAD) && pkg.desired->binpicked)
	    total_download_bytes += version->bin.size;
	  if (!
	      (check_for_cached (version->src)
	       && source != IDC_SOURCE_DOWNLOAD) && pkg.desired->srcpicked)
	    total_download_bytes += version->src.size;
	}
    }

  /* and do the download. FIXME: This here we assign a new name for the cached version
   * and check that above.
   */
  for (size_t n = 1; n < db.packages.number (); n++)
    {
      packagemeta & pkg = *db.packages[n];
      if (pkg.desired && (pkg.desired->srcpicked || pkg.desired->binpicked))
	{
	  int e = 0;
	  packageversion *version = pkg.desired;
	  if (version->binpicked)
	    e += download_one (version->bin, owner);
	  if (version->srcpicked)
	    e += download_one (version->src, owner);
	  errors += e;
#if 0
	  if (e)
	    pkg->action = ACTION_ERROR;
#endif
	}
    }

  if (errors)
    {
      if (yesno (owner, IDS_DOWNLOAD_INCOMPLETE) == IDYES)
	{
	  next_dialog = IDD_SITE;
	  return;
	}
    }

  if (source == IDC_SOURCE_DOWNLOAD)
    {
      if (errors)
	exit_msg = IDS_DOWNLOAD_INCOMPLETE;
      else
	exit_msg = IDS_DOWNLOAD_COMPLETE;
      next_dialog = 0;
    }
  else
    next_dialog = IDD_S_INSTALL;
}

static void
do_download_reflector (void *p)
{
  HANDLE *context;
  context = (HANDLE *) p;

  do_download_thread ((HINSTANCE) context[0], (HWND) context[1]);

  // Tell the progress page that we're done downloading
  Progress.PostMessage (WM_APP_DOWNLOAD_THREAD_COMPLETE, 0, next_dialog);

  _endthread ();
}

static HANDLE context[2];

void
do_download (HINSTANCE h, HWND owner)
{
  context[0] = h;
  context[1] = owner;

  _beginthread (do_download_reflector, 0, context);
}
