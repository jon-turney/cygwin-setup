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

/* The purpose of this file is to install all the packages selected in
   the install list (in ini.h).  Note that we use a separate thread to
   maintain the progress dialog, so we avoid the complexity of
   handling two tasks in one thread.  We also create or update all the
   files in /etc/setup/\* and create the mount points. */

#if 0
static const char *cvsid = "\n%%% $Id$\n";
#endif

#include "win32.h"
#include "commctrl.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <process.h>

#include "zlib/zlib.h"

#include "resource.h"
#include "dialog.h"
#include "geturl.h"
#include "state.h"
#include "diskfull.h"
#include "msg.h"
#include "mount.h"
#include "log.h"
#include "mount.h"
#include "filemanip.h"
#include "io_stream.h"
#include "compress.h"
#include "compress_gz.h"
#include "archive.h"
#include "archive_tar.h"
#include "script.h"

#include "package_db.h"
#include "package_meta.h"
#include "package_version.h"
#include "package_source.h"

#include "port.h"

#include "threebar.h"

#include "md5.h"

#include "Exception.h"
#include "getopt++/BoolOption.h"

using namespace std;

extern ThreeBarProgressPage Progress;

static int total_bytes = 0;
static int total_bytes_sofar = 0;
static int package_bytes = 0;

static BoolOption NoReplaceOnReboot (false, 'r', "no-replaceonreboot", 
				     "Disable replacing in-use files on next "
				     "reboot.");

static void
init_dialog ()
{
  Progress.SetText2 ("");
  Progress.SetText3 ("");
}

static void
progress (int bytes)
{
  if (package_bytes > 0)
    {
      Progress.SetBar1 (bytes, package_bytes);
    }

  if (total_bytes > 0)
    {
      Progress.SetBar2 (total_bytes_sofar + bytes, total_bytes);
    }
}

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

static int num_installs, num_replacements, num_uninstalls;
static void uninstall_one (packagemeta &);
static int replace_one (packagemeta &);
static int install_one_source (packagemeta &, packagesource &, String const &,
			       String const &, package_type_t);
static void md5_one (const packagesource& source);
static bool rebootneeded;

/* FIXME: upgrades should be a method too */
static void
uninstall_one (packagemeta & pkgm)
{
  Progress.SetText1 ("Uninstalling...");
  Progress.SetText2 (pkgm.name.cstr_oneuse());
  log (LOG_PLAIN, String("Uninstalling ") + pkgm.name);
  pkgm.uninstall ();
  num_uninstalls++;
}

/* uninstall and install a package, preserving configuration
 * files and the like.
 * This method should also know about replacing in-use file.
 * ASSUMPTIONS: pkgm is installed.
 *		pkgm has a desired package.
 */
static int
replace_one (packagemeta & pkg)
{
  int errors = 0;
  Progress.SetText1 ("Replacing...");
  Progress.SetText2 (pkg.name.cstr_oneuse());
  log (LOG_PLAIN, String( "Replacing ")  + pkg.name);
  pkg.uninstall ();

  errors +=
    install_one_source (pkg, *pkg.desired.source(), "cygfile://","/", package_binary);
  if (!errors)
    pkg.installed = pkg.desired;
  num_replacements++;
  return errors;
}

/* log failed scheduling of replace-on-reboot of a given file. */
/* also increment errors. */
static void
log_ror_failure (String const &fn, int &errors)
{
  log (LOG_TIMESTAMP,
       "Unable to schedule reboot replacement of file %s with %s (Win32 Error %ld)",
       cygpath (String ("/") + fn).cstr_oneuse(),
       cygpath (String ("/") + fn + ".new").cstr_oneuse(),
       GetLastError ());
  ++errors;
}

/* log successful scheduling of replace-on-reboot of a given file. */
/* also set rebootneeded. */
static void
log_ror_success (String const &fn, bool &rebootneeded)
{
  log (LOG_TIMESTAMP,
       "Scheduled reboot replacement of file %s with %s",
       cygpath (String ("/") + fn).cstr_oneuse(),
       cygpath (String ("/") + fn + ".new").cstr_oneuse());
  rebootneeded = true;
}

/* install one source at a given prefix. */
static int
install_one_source (packagemeta & pkgm, packagesource & source,
		    String const &prefixURL, String const &prefixPath, package_type_t type)
{
  int errors = 0;
  Progress.SetText2 (source.Base ());
  if (!source.Cached () || !io_stream::exists (source.Cached ()))
    {
      note (NULL, IDS_ERR_OPEN_READ, source.Cached (), "No such file");
      return 1;
    }
  io_stream *lst = 0;

  package_bytes = source.size;

  char msg[64];
  strcpy (msg, "Installing");
  Progress.SetText1 (msg);
  log (LOG_PLAIN, String (msg) + " " + source.Cached ());
  
  io_stream *tmp = io_stream::open (source.Cached (), "rb");
  archive *thefile = 0;
  if (tmp)
    {
      io_stream *tmp2 = compress::decompress (tmp);
      if (tmp2)
	thefile = archive::extract (tmp2);
      else
	thefile = archive::extract (tmp);
    }
    
  /* FIXME: potential leak of either *tmp or *tmp2 */
  if (thefile)
    {
      String fn;
      if (type == package_binary)
	{
	  io_stream *tmp = io_stream::open (String ("cygfile:///etc/setup/") +
	                                    pkgm.name + ".lst.gz", "wb");
	  lst = new compress_gz (tmp, "w9");
	  if (lst->error ())
	    {
              delete lst;
	      lst = NULL;
	    }
	}
      while ((fn = thefile->next_file_name ()).size())
	{
	  if (lst)
	    {
	      String tmp=fn + "\n";
	      lst->write (tmp.cstr_oneuse(), tmp.size());
	    }

	  String canonicalfn = prefixPath + fn;

	  Progress.SetText3 (canonicalfn.cstr_oneuse());
	  log (LOG_BABBLE, String("Installing file ") + prefixURL + prefixPath + fn);
	  if (archive::extract_file (thefile, prefixURL, prefixPath) != 0)
	    {
	      if (NoReplaceOnReboot)
		{
		  ++errors;
		  log (LOG_PLAIN, String("Not replacing in-use file ") +
		       prefixURL + prefixPath + fn);
		}
	      else
	      //extract to temp location
	      if (archive::extract_file (thefile, prefixURL, prefixPath, ".new") != 0)
		{
		  log (LOG_PLAIN,
		       String("Unable to install file ") +
		       prefixURL + prefixPath + fn);
		  errors++;
		}
	      else
		//switch Win32::OS
		{
		  switch (Win32::OS ())
		    {
		    case Win32::Win9x:{
		      /* Get the short file names */
		      char source[MAX_PATH];
		      unsigned int len =
			GetShortPathName (cygpath (String ("/") + fn +
						   ".new").cstr_oneuse(),
					  source, MAX_PATH);
		      if (!len || len > MAX_PATH)
			{
			  log_ror_failure (fn, errors);
			}
		      else
			{
			  char dest[MAX_PATH];
			  len =
			    GetShortPathName (cygpath (String ("/") +
						       fn).cstr_oneuse(),
					      dest, MAX_PATH);
			  if (!len || len > MAX_PATH)
			    {
			      log_ror_failure (fn, errors);
			    }
			  else
			    /* trigger a replacement on reboot */
			  if (!WritePrivateProfileString
				("rename", dest, source, "WININIT.INI"))
			    {
			      log_ror_failure (fn, errors);
			    }
			  else
			    {
			      log_ror_success (fn, rebootneeded);
			    }
			}
		    }
		      break;
		    case Win32::WinNT:
		      /* XXX FIXME: prefix may not be / for in use files -
		       * although it most likely is
		       * - we need a io method to get win32 paths 
		       * or to wrap this system call
		       */
		      if (!MoveFileEx (cygpath (String ("/") + fn +
						".new").cstr_oneuse(),
				       cygpath (String ("/") + fn).cstr_oneuse(),
				       MOVEFILE_DELAY_UNTIL_REBOOT |
				       MOVEFILE_REPLACE_EXISTING))
			{
			  log_ror_failure (fn, errors);
			}
		      else
			{
			  log_ror_success (fn, rebootneeded);
			}
		      break;
		    }
		}
	    }

	  progress (tmp->tell ());
	  num_installs++;
	}
      delete thefile;

      total_bytes_sofar += package_bytes;
    }


  progress (0);

  int df = diskfull (get_root_dir ().cstr_oneuse());
  Progress.SetBar3 (df);

  if (lst)
    delete lst;

  return errors;
}

/* install a package, install both the binary and source aspects if needed */
static int
install_one (packagemeta & pkg)
{
  int errors = 0;

  if (pkg.installed != pkg.desired && pkg.desired.picked())
    {
      errors +=
	install_one_source (pkg, *pkg.desired.source(), "cygfile://","/",
			    package_binary);
      if (!errors)
	pkg.installed = pkg.desired;
    }
  if (pkg.desired.sourcePackage().picked())
    errors +=
      install_one_source (pkg, *pkg.desired.sourcePackage().source(), "cygfile://","/usr/src/",
			  package_source);

  /* FIXME: make a upgrade method and reinstate this */
#if 0
  String msg;
  if (!pkg->installed)
    msg = "Installing";
  else
    {
      int n = strcmp (pi->version, pkg->installed->version);
      if (n < 0)
	msg = "Reverting";
      else if (n == 0)
	msg = "Reinstalling";
      else
	msg = "Upgrading";
    }

  switch (pkg->action)
    {
    case ACTION_PREV:
      msg += " previous version...";
      break;
    case ACTION_CURR:
      msg += "...";
      break;
    case ACTION_TEST:
      msg += " test version...";
      break;
    default:
      /* FIXME: log this somehow */
      break;
    }
  SetWindowText (ins_action, msg.cstr_oneuse());
  log (LOG_PLAIN, msg + " " + file);
#endif

  return errors;
}

static void
check_for_old_cygwin ()
{
  char buf[_MAX_PATH + sizeof ("\\cygwin1.dll")];
  if (!GetSystemDirectory (buf, sizeof (buf)))
    return;
  strcat (buf, "\\cygwin1.dll");
  if (_access (buf, 0) != 0)
    return;

  char msg[sizeof (buf) + 132];
  sprintf (msg,
	   "An old version of cygwin1.dll was found here:\r\n%s\r\nDelete?",
	   buf);
  switch (MessageBox
	  (NULL, msg, "What's that doing there?",
	   MB_YESNO | MB_ICONQUESTION | MB_TASKMODAL))
    {
    case IDYES:
      if (!DeleteFile (buf))
	{
	  sprintf (msg, "Couldn't delete file %s.\r\n"
		   "Is the DLL in use by another application?\r\n"
		   "You should delete the old version of cygwin1.dll\r\n"
		   "at your earliest convenience.", buf);
	  MessageBox (NULL, buf, "Couldn't delete file",
		      MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);
	}
      break;
    default:
      break;
    }

  return;
}

static void
do_install_thread (HINSTANCE h, HWND owner)
{
  int i;
  int errors = 0;

  num_installs = 0, num_uninstalls = 0, num_replacements = 0;
  rebootneeded = false;

  next_dialog = IDD_DESKTOP;

  io_stream::mkpath_p (PATH_TO_DIR, String ("file://") + get_root_dir ());

  for (i = 0; standard_dirs[i]; i++)
    {
      String p = cygpath (standard_dirs[i]);
      if (p.size())
	io_stream::mkpath_p (PATH_TO_DIR, String ("file://") + p);
    }

  /* Create /var/run/utmp */
  io_stream *utmp = io_stream::open ("cygfile:///var/run/utmp", "wb");
  delete utmp;

  init_dialog ();

  total_bytes = 0;
  total_bytes_sofar = 0;

  int df = diskfull (get_root_dir ().cstr_oneuse());
  Progress.SetBar3 (df);

  int istext = (root_text == IDC_ROOT_TEXT) ? 1 : 0;
  int issystem = (root_scope == IDC_ROOT_SYSTEM) ? 1 : 0;

  create_mount ("/", get_root_dir (), istext, issystem);
  create_mount ("/usr/bin", cygpath ("/bin"), istext, issystem);
  create_mount ("/usr/lib", cygpath ("/lib"), istext, issystem);
  set_cygdrive_flags (istext, issystem);

  /* Let's hope people won't uninstall packages before installing [b]ash */
  init_run_script ();

  packagedb db;
  for (vector <packagemeta *>::iterator i = db.packages.begin ();
       i != db.packages.end (); ++i)
    {
      packagemeta & pkg = **i;

      if (pkg.desired.changeRequested())
	{
	  if (pkg.desired.picked())
	    {
	      try
		{
		  md5_one (*pkg.desired.source ());
		}
	      catch (Exception *e)
		{
		  if (yesno (owner, IDS_SKIP_PACKAGE, e->what()) == IDYES)
		      pkg.desired.pick (false);
		}
	      if (pkg.desired.picked())
		total_bytes += pkg.desired.source()->size;
	    }
	  if (pkg.desired.sourcePackage ().picked())
	    {
	      try
		{
		  md5_one (*pkg.desired.sourcePackage ().source ());
		}
	      catch (Exception *e)
		{
		  if (yesno (owner, IDS_SKIP_PACKAGE, e->what()) == IDYES)
		      pkg.desired.sourcePackage ().pick (false);
		}
	      if (pkg.desired.sourcePackage().picked())
	        total_bytes += pkg.desired.sourcePackage ().source()->size;
	    }
	}
    }

  /* start with uninstalls - remove files that new packages may replace */
  for (vector <packagemeta *>::iterator i = db.packages.begin ();
       i != db.packages.end (); ++i)
    {
      packagemeta & pkg = **i;
      if (pkg.installed && (!pkg.desired || (pkg.desired != pkg.installed &&
	  pkg.desired.picked ())))
	uninstall_one (pkg);
    }

  /* now in-place binary upgrades/reinstalls, as these may remove fils 
   * that have been moved into new packages
   */

  for (vector <packagemeta *>::iterator i = db.packages.begin ();
       i != db.packages.end (); ++i)
    {
      packagemeta & pkg = **i;
      if (pkg.installed && pkg.desired.picked())
	{
	  try {
	      int e = 0;
	    e += replace_one (pkg);
 	    if (e)
	      errors++;
	  }
	  catch (exception *e)
	    {
	      if (yesno (owner, IDS_INSTALL_ERROR, e->what()) != IDYES)
		{
		  log (LOG_TIMESTAMP, String ("User cancelled setup after install error"));
		  LogSingleton::GetInstance().exit (1);
		  return;
		}
	    }
	}
    }

  for (vector <packagemeta *>::iterator i = db.packages.begin ();
       i != db.packages.end (); ++i)
    {
      packagemeta & pkg = **i;

      if (pkg.desired && pkg.desired.changeRequested())
	{
	  try
	    {
    	      int e = 0;
    	      e += install_one (pkg);
    	      if (e)
		  errors++;
	    }
	  catch (exception *e)
	    {
	      if (yesno (owner, IDS_INSTALL_ERROR, e->what()) != IDYES)
		{
		  log (LOG_TIMESTAMP, String ("User cancelled setup after install error"));
		  LogSingleton::GetInstance().exit (1);
		  return;
		}
	    }
	}
    }				// end of big package loop

  if (rebootneeded)
    note (owner, IDS_REBOOT_REQUIRED);

  int temperr;
  if ((temperr = db.flush ()))
    {
      const char *err = strerror (temperr);
      if (!err)
	err = "(unknown error)";
      fatal (owner, IDS_ERR_OPEN_WRITE, "Package Database",
	  err);
    }

  if (!errors)
    check_for_old_cygwin ();
  if (num_installs == 0 && num_uninstalls == 0)
    {
      if (!unattended_mode) exit_msg = IDS_NOTHING_INSTALLED;
      return;
    }
  if (num_installs == 0)
    {
      if (!unattended_mode) exit_msg = IDS_UNINSTALL_COMPLETE;
      return;
    }

  if (errors)
    exit_msg = IDS_INSTALL_INCOMPLETE;
  else if (!unattended_mode)
    exit_msg = IDS_INSTALL_COMPLETE;
}

static DWORD WINAPI
do_install_reflector (void *p)
{
  HANDLE *context;
  context = (HANDLE *) p;

  do_install_thread ((HINSTANCE) context[0], (HWND) context[1]);

  // Tell the progress page that we're done downloading
  Progress.PostMessage (WM_APP_INSTALL_THREAD_COMPLETE);

  ExitThread (0);
}

static HANDLE context[2];

void
do_install (HINSTANCE h, HWND owner)
{
  context[0] = h;
  context[1] = owner;

  DWORD threadID;
  CreateThread (NULL, 0, do_install_reflector, context, 0, &threadID);
}

void md5_one (const packagesource& source)
{
  if (source.md5.isSet())
    {
      // check the MD5 sum of the cached file here
      io_stream *thefile = io_stream::open (source.Cached (), "rb");
      if (!thefile)
	throw new Exception ("__LINE__ __FILE__", String ("IO Error opening ") + source.Cached(), APPERR_IO_ERROR);
      md5_state_t pns;
      md5_init (&pns);
      
      unsigned char buffer[16384];
      ssize_t count;
      while ((count = thefile->read (buffer, 16384)) > 0)
	  md5_append (&pns, buffer, count);
      delete thefile;
      if (count < 0)
	throw new Exception ("__LINE__ __FILE__", String ("IO Error reading ") + source.Cached(), APPERR_IO_ERROR);
      
      md5_byte_t tempdigest[16];
      md5_finish(&pns, tempdigest);
      md5 tempMD5;
      tempMD5.set (tempdigest);

      log (LOG_BABBLE, String ("For file ") + source.Cached() + " ini digest is " + source.md5.print() + " file digest is " + tempMD5.print());
      
      if (source.md5 != tempMD5)
	  throw new Exception ("__LINE__ __FILE__", String ("Checksum failure for ") + source.Cached(), APPERR_CORRUPT_PACKAGE);
    }
}
