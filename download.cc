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
#include "dialog.h"
#include "String++.h"
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

#include "md5.h"

#include "Exception.h"

#include "download.h"
  
extern ThreeBarProgressPage Progress;

/* 0 on failure
 */
int
check_for_cached (packagesource & pkgsource)
{
  /* search algo:
     1) is there a legacy version in the cache dir available.
     (Note that the cache dir is represented by a mirror site of
     file://local_dir
   */

  // Already found one.
  if (pkgsource.Cached())
    return 1;
  
  String prefix = String ("file://") + local_dir +  "/";
  DWORD size;
  if ((size = get_file_size (prefix + pkgsource.Canonical ())) > 0)
    if (size == pkgsource.size)
      {
	pkgsource.set_cached (prefix + pkgsource.Canonical ());
	return 1;
      }

  /*
     2) is there a version from one of the selected mirror sites available ?
   */
  for (size_t n = 1; n <= pkgsource.sites.number (); n++)
    {
      String fullname = prefix +
	rfc1738_escape_part (pkgsource.sites[n]->key) + "/" +
	pkgsource.Canonical ();
    if ((size = get_file_size (fullname)) > 0)
      if (size == pkgsource.size)
	{
	  if (pkgsource.md5.isSet())
	    {
	      // check the MD5 sum of the cached file here
	      io_stream *thefile = io_stream::open (fullname, "rb");
	      if (!thefile)
		return 0;
	      md5_state_t pns;
	      md5_init (&pns);

	      unsigned char buffer[16384];
	      ssize_t count;
	      while ((count = thefile->read (buffer, 16384)) > 0)
		md5_append (&pns, buffer, count);
	      delete thefile;
	      if (count < 0)
		throw new Exception ("__LINE__ __FILE__", (String ("IO Error reading ") + pkgsource.Cached()).cstr_oneuse(), APPERR_IO_ERROR);

	      md5_byte_t tempdigest[16];
	      md5_finish(&pns, tempdigest);
	      md5 tempMD5;
	      tempMD5.set (tempdigest);
	      
	      log (LOG_BABBLE, String ("For file '") + fullname + 
		   " ini digest is " + pkgsource.md5.print() + 
		   " file digest is " + tempMD5.print());
	      
	      if (pkgsource.md5 != tempMD5)
		throw new Exception ("__LINE__ __FILE__", (String ("MD5 Checksum failure for ") + pkgsource.Cached()).cstr_oneuse(), APPERR_CORRUPT_PACKAGE);
	    }
	  pkgsource.
	    set_cached (fullname );
	  return 1;
	}
	}
  return 0;
}

/* download a file from a mirror site to the local cache. */
static int
download_one (packagesource & pkgsource, HWND owner)
{
  try
    {
      if (check_for_cached (pkgsource))
        return 0;
    }
  catch (Exception * e)
    {
      // We know what to do with these..
      if (e->errNo() == APPERR_CORRUPT_PACKAGE)
	{
	  fatal (owner, IDS_CORRUPT_PACKAGE, pkgsource.Canonical());
    	  return 1;
	}
      // Unexpected exception.
      throw e;
    }
  /* try the download sites one after another */

  int success = 0;
  for (size_t n = 1; n <= pkgsource.sites.number () && !success; n++)
    {
      String const local = local_dir + "/" +
				  rfc1738_escape_part (pkgsource.sites[n]->
						       key) + "/" +
				  pkgsource.Canonical ();
      io_stream::mkpath_p (PATH_TO_FILE, String ("file://") + local);

      if (get_url_to_file(pkgsource.sites[n]->key +  "/" +
			  pkgsource.Canonical (),
			  local + ".tmp", pkgsource.size, owner))
	{
	  /* FIXME: note new source ? */
	  continue;
	}
      else
	{
	  size_t size = get_file_size (String("file://") + local + ".tmp");
	  if (size == pkgsource.size)
	    {
	      log (LOG_PLAIN, String ("Downloaded ") + local);
	      if (_access (local.cstr_oneuse(), 0) == 0)
		remove (local.cstr_oneuse());
	      rename ((local + ".tmp").cstr_oneuse(), local.cstr_oneuse());
	      success = 1;
	      pkgsource.set_cached (String ("file://") + local);
	      // FIXME: move the downloaded file to the 
	      //  original locations - without the mirror site dir in the way
	      continue;
	    }
	  else
	    {
	      log (LOG_PLAIN,
		   "Download %s wrong size (%u actual vs %d expected)",
		   local.cstr_oneuse(), size, pkgsource.size);
	      remove ((local + ".tmp").cstr_oneuse());
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
  for (size_t n = 1; n <= db.packages.number (); n++)
    {
      packagemeta & pkg = *db.packages[n];
      if (pkg.desired && (pkg.desired->srcpicked || pkg.desired->binpicked))
	{
	  packageversion *version = pkg.desired;
	  try 
	    {
    	      if (!check_for_cached (version->bin) && pkg.desired->binpicked)
      		  total_download_bytes += version->bin.size;
    	      if (!check_for_cached (version->src) && pkg.desired->srcpicked)
      		  total_download_bytes += version->src.size;
	    }
	  catch (Exception * e)
	    {
	      // We know what to do with these..
	      if (e->errNo() == APPERR_CORRUPT_PACKAGE)
		fatal (owner, IDS_CORRUPT_PACKAGE, pkg.name.cstr_oneuse());
	      // Unexpected exception.
	      throw e;
	    }
	}
    }

  /* and do the download. FIXME: This here we assign a new name for the cached version
   * and check that above.
   */
  for (size_t n = 1; n <= db.packages.number (); n++)
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

static DWORD WINAPI
do_download_reflector (void *p)
{
  HANDLE *context;
  context = (HANDLE *) p;

  do_download_thread ((HINSTANCE) context[0], (HWND) context[1]);

  // Tell the progress page that we're done downloading
  Progress.PostMessage (WM_APP_DOWNLOAD_THREAD_COMPLETE, 0, next_dialog);

  ExitThread(0);
}

static HANDLE context[2];

void
do_download (HINSTANCE h, HWND owner)
{
  context[0] = h;
  context[1] = owner;

  DWORD threadID;
  CreateThread (NULL, 0, do_download_reflector, context, 0, &threadID);
}
