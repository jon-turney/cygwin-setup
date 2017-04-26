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

#include "csu_util/rfc1738.h"

#include "download.h"
  
#include "win32.h"

#include <stdio.h>
#include <unistd.h>
#include <process.h>
#include <vector>

#include "resource.h"
#include "msg.h"
#include "dialog.h"
#include "geturl.h"
#include "state.h"
#include "LogFile.h"
#include "filemanip.h"

#include "io_stream.h"

#include "package_db.h"
#include "package_meta.h"
#include "package_version.h"
#include "package_source.h"

#include "threebar.h"

#include "Exception.h"

#include "getopt++/BoolOption.h"

using namespace std;

extern ThreeBarProgressPage Progress;

BoolOption IncludeSource (false, 'I', "include-source", "Automatically include source download");

static bool
validateCachedPackage (const std::string& fullname, packagesource & pkgsource)
{
  DWORD size = get_file_size(fullname);
  if (size != pkgsource.size)
  {
    Log (LOG_BABBLE) << "INVALID PACKAGE: " << fullname
      << " - Size mismatch: Ini-file: " << pkgsource.size
      << " != On-disk: " << size << endLog;
    return false;
  }
  return true;
}

/* 0 on failure
 */
int
check_for_cached (packagesource & pkgsource, bool mirror_mode)
{
  // Already found one.
  if (pkgsource.Cached())
    return 1;

  /* Note that the cache dir is represented by a mirror site of file://local_dir */
  std::string prefix = "file://" + local_dir + "/";
  std::string fullname = prefix + (pkgsource.Canonical() ? pkgsource.Canonical() : "");

  if (mirror_mode)
    {
      /* Just assume correctness of mirror. */
      pkgsource.set_cached (fullname);
      return 1;
    }

  /*
     1) is there a legacy version in the cache dir available.
  */
  if (io_stream::exists (fullname))
    {
      if (validateCachedPackage (fullname, pkgsource))
        pkgsource.set_cached (fullname);
      else
        throw new Exception (TOSTRING(__LINE__) " " __FILE__,
            "Package validation failure for " + fullname,
            APPERR_CORRUPT_PACKAGE);
      return 1;
    }

  /*
     2) is there a version from one of the selected mirror sites available ?
  */
  for (packagesource::sitestype::const_iterator n = pkgsource.sites.begin();
       n != pkgsource.sites.end(); ++n)
  {
    std::string fullname = prefix + rfc1738_escape_part (n->key) + "/" +
      pkgsource.Canonical ();
    if (io_stream::exists(fullname))
    {
      if (validateCachedPackage (fullname, pkgsource))
        pkgsource.set_cached (fullname);
      else
        throw new Exception (TOSTRING(__LINE__) " " __FILE__,
            "Package validation failure for " + fullname,
            APPERR_CORRUPT_PACKAGE);
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
  for (packagesource::sitestype::const_iterator n = pkgsource.sites.begin();
       n != pkgsource.sites.end() && !success; ++n)
    {
      const std::string local = local_dir + "/" +
				  rfc1738_escape_part (n->key) + "/" +
				  pkgsource.Canonical ();
      io_stream::mkpath_p (PATH_TO_FILE, "file://" + local, 0);

      if (get_url_to_file(n->key + pkgsource.Canonical (),
			  local + ".tmp", pkgsource.size, owner))
	{
	  /* FIXME: note new source ? */
	  continue;
	}
      else
	{
	  size_t size = get_file_size ("file://" + local + ".tmp");
	  if (size == pkgsource.size)
	    {
	      Log (LOG_PLAIN) << "Downloaded " << local << endLog;
	      if (_access (local.c_str(), 0) == 0)
		remove (local.c_str());
	      rename ((local + ".tmp").c_str(), local.c_str());
	      success = 1;
	      pkgsource.set_cached ("file://" + local);
	      // FIXME: move the downloaded file to the 
	      //  original locations - without the mirror site dir in the way
	      continue;
	    }
	  else
	    {
	      Log (LOG_PLAIN) << "Download " << local << " wrong size (" <<
		size << " actual vs " << pkgsource.size << " expected)" << 
		endLog;
	      remove ((local + ".tmp").c_str());
	      continue;
	    }
	}
    }
  if (success)
    return 0;
  return 1;
}

static std::vector <packageversion> download_failures;
static std::string download_warn_pkgs;

static INT_PTR CALLBACK
download_error_proc (HWND h, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
    {
    case WM_INITDIALOG:
      eset (h, IDC_DOWNLOAD_EDIT, download_warn_pkgs);
      SetFocus (GetDlgItem(h, IDRETRY));
      return FALSE;

    case WM_COMMAND:
      switch (LOWORD (wParam))
	{
	case IDRETRY:
	case IDC_BACK:
	case IDIGNORE:
	case IDABORT:
	  EndDialog (h, LOWORD (wParam));
	default:
	  // Not reached.
	  return 0;
	}

    default:
      // Not handled.
      return FALSE;
    }
  return TRUE;
}

static int
query_download_errors (HINSTANCE h, HWND owner)
{
  download_warn_pkgs = "";
  Log (LOG_PLAIN) << "The following package(s) had download errors:" << endLog;
  for (std::vector <packageversion>::const_iterator i = download_failures.begin (); i != download_failures.end (); i++)
    {
      packageversion pv = *i;
      std::string pvs = pv.Name () + "-" + pv.Canonical_version ();
      Log (LOG_PLAIN) << "  " << pvs << endLog;
      download_warn_pkgs += pvs + "\r\n";
    }
  return DialogBox (h, MAKEINTRESOURCE (IDD_DOWNLOAD_ERROR), owner,
		    download_error_proc);
}

static int
do_download_thread (HINSTANCE h, HWND owner)
{
  int errors = 0;
  total_download_bytes = 0;
  total_download_bytes_sofar = 0;
  download_failures.clear ();

  Progress.SetText1 ("Checking for packages to download...");
  Progress.SetText2 ("");
  Progress.SetText3 ("");

  packagedb db;
  /* calculate the amount needed */
  for (packagedb::packagecollection::iterator i = db.packages.begin ();
       i != db.packages.end (); ++i)
    {
      packagemeta & pkg = *(i->second);
      if (pkg.desired && (pkg.picked () || pkg.srcpicked ()))
	{
	  packageversion version = pkg.desired;
	  packageversion sourceversion = version.sourcePackage();
	  try 
	    {
	      if (pkg.picked())
		{
		    if (!check_for_cached (*version.source()))
		      total_download_bytes += version.source()->size;
		}
	      if (pkg.srcpicked () || IncludeSource)
		{
		    if (!check_for_cached (*sourceversion.source()))
		      total_download_bytes += sourceversion.source()->size;
		}
	    }
	  catch (Exception * e)
	    {
	      // We know what to do with these..
	      if (e->errNo() == APPERR_CORRUPT_PACKAGE)
		fatal (owner, IDS_CORRUPT_PACKAGE, pkg.name.c_str());
	      // Unexpected exception.
	      throw e;
	    }
	}
    }

  /* and do the download. FIXME: This here we assign a new name for the cached version
   * and check that above.
   */
  for (packagedb::packagecollection::iterator i = db.packages.begin ();
       i != db.packages.end (); ++i)
    {
      packagemeta & pkg = *(i->second);
      if (pkg.desired && (pkg.picked () || pkg.srcpicked ()))
	{
	  int e = 0;
	  packageversion version = pkg.desired;
	  packageversion sourceversion = version.sourcePackage();
	  if (pkg.picked())
	    {
		e += download_one (*version.source(), owner);
	    }
	  if (sourceversion && (pkg.srcpicked() || IncludeSource))
	    {
		e += download_one (*sourceversion.source (), owner);
	    }
	  errors += e;
	  if (e)
	    download_failures.push_back (version);
#if 0
	  if (e)
	    pkg->action = ACTION_ERROR;
#endif
	}
    }

  if (errors)
    {
      // In unattended mode we retry the download, but not forever.
      static int retries = 5;
      int rc;
      if (unattended_mode && --retries <= 0)
        {
	  Log (LOG_PLAIN) << "download error in unattended_mode: out of retries" << endLog;
	  rc = IDABORT;
	}
      else if (unattended_mode)
        {
	  Log (LOG_PLAIN) << "download error in unattended_mode: " << retries
	    << (retries > 1 ? " retries" : " retry") << " remaining." << endLog;
	  rc = IDRETRY;
	}
      else
	rc = query_download_errors (h, owner);
      switch (rc)
	{
	case IDRETRY:
	  Progress.SetActivateTask (WM_APP_START_DOWNLOAD);
	  return IDD_INSTATUS;
	case IDC_BACK:
	  return IDD_CHOOSE;
	case IDABORT:
	  Logger ().setExitMsg (IDS_DOWNLOAD_INCOMPLETE_EXIT);
	  Logger ().exit (1);
	case IDIGNORE:
	  break;
	default:
	  break;
	}
    }

  if (source == IDC_SOURCE_DOWNLOAD)
    {
      if (errors)
	Logger ().setExitMsg (IDS_DOWNLOAD_INCOMPLETE_EXIT);
      else if (!unattended_mode)
	Logger ().setExitMsg (IDS_DOWNLOAD_COMPLETE);
      return IDD_DESKTOP;
    }
  else
    return IDD_S_INSTALL;
}

static DWORD WINAPI
do_download_reflector (void *p)
{
  HANDLE *context;
  context = (HANDLE *) p;

  try
  {
    int next_dialog =
      do_download_thread ((HINSTANCE) context[0], (HWND) context[1]);

    // Tell the progress page that we're done downloading
    Progress.PostMessageNow (WM_APP_DOWNLOAD_THREAD_COMPLETE, 0, next_dialog);
  }
  TOPLEVEL_CATCH("download");

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
