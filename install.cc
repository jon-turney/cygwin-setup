/*
 * Copyright (c) 2000, Red Hat, Inc.
 * Copyright (c) 2003, Robert Collins <rbtcollins@hotmail.com>
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Originally Written by DJ Delorie <dj@cygnus.com>
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

#include "getopt++/BoolOption.h"
#include "csu_util/MD5Sum.h"
#include "LogSingleton.h"

#include "win32.h"
#include "commctrl.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <process.h>

#include "resource.h"
#include "dialog.h"
#include "geturl.h"
#include "state.h"
#include "diskfull.h"
#include "msg.h"
#include "mount.h"
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

#include "threebar.h"
#include "Exception.h"

using namespace std;

extern ThreeBarProgressPage Progress;

static int total_bytes = 0;
static int total_bytes_sofar = 0;
static int package_bytes = 0;

static BoolOption NoReplaceOnReboot (false, 'r', "no-replaceonreboot", 
				     "Disable replacing in-use files on next "
				     "reboot.");

class Installer
{
  public:
    static const char *StandardDirs[];
    Installer();
    void initDialog();
    void progress (int bytes);
    void preremoveOne (packagemeta &);
    void uninstallOne (packagemeta &);
    void replaceOnRebootFailed (String const &fn);
    void replaceOnRebootSucceeded (String const &fn, bool &rebootneeded);
    void installOne (packagemeta &pkg, const packageversion &ver,
                     packagesource &source,
                     String const &, String const &);
    int errors;
};

Installer::Installer() : errors(0)
{
}

void
Installer::initDialog()
{
  Progress.SetText2 ("");
  Progress.SetText3 ("");
}

void
Installer::progress (int bytes)
{
  if (package_bytes > 0)
      Progress.SetBar1 (bytes, package_bytes);

  if (total_bytes > 0)
      Progress.SetBar2 (total_bytes_sofar + bytes, total_bytes);
}

const char *
Installer::StandardDirs[] = {
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

static int num_installs, num_uninstalls;
static void md5_one (const packagesource& source);
static bool rebootneeded;

void
Installer::preremoveOne (packagemeta & pkg)
{
  Progress.SetText1 ("Running preremove script...");
  Progress.SetText2 (pkg.name.c_str());
  log (LOG_PLAIN) << "Running preremove script for  " << pkg.name << endLog;
  try_run_script ("/etc/preremove/", pkg.name);
}

void
Installer::uninstallOne (packagemeta & pkg)
{
  Progress.SetText1 ("Uninstalling...");
  Progress.SetText2 (pkg.name.c_str());
  log (LOG_PLAIN) << "Uninstalling " << pkg.name << endLog;
  pkg.uninstall ();
  num_uninstalls++;
}

/* log failed scheduling of replace-on-reboot of a given file. */
/* also increment errors. */
void
Installer::replaceOnRebootFailed (String const &fn)
{
  log (LOG_TIMESTAMP) << "Unable to schedule reboot replacement of file "
    << cygpath (String ("/") + fn).c_str() << " with "
    << cygpath (String ("/") + fn + ".new").c_str()
    << " (Win32 Error " << GetLastError() << ")" << endLog;
  ++errors;
}

/* log successful scheduling of replace-on-reboot of a given file. */
/* also set rebootneeded. */
void
Installer::replaceOnRebootSucceeded (String const &fn, bool &rebootneeded)
{
  log (LOG_TIMESTAMP) << "Scheduled reboot replacement of file "
    << cygpath (String ("/") + fn).c_str() << " with "
    << cygpath (String ("/") + fn + ".new").c_str() << endLog;
  rebootneeded = true;
}

/* install one source at a given prefix. */
void
Installer::installOne (packagemeta &pkgm, const packageversion &ver,
                       packagesource &source,
                       String const &prefixURL, String const &prefixPath)
{
  Progress.SetText2 (source.Base ());
  if (!source.Cached () || !io_stream::exists (source.Cached ()))
    {
      note (NULL, IDS_ERR_OPEN_READ, source.Cached (), "No such file");
      ++errors;
      return;
    }
  bool error_in_this_package = false;
  io_stream *lst = 0;

  package_bytes = source.size;

  char msg[64];
  strcpy (msg, "Installing");
  Progress.SetText1 (msg);
  log (LOG_PLAIN) << msg << " " << source.Cached () << endLog;
  
  io_stream *tmp = io_stream::open (source.Cached (), "rb");
  io_stream *tmp2 = 0;
  archive *thefile = 0;
  if (tmp)
    {
      tmp2 = compress::decompress (tmp);
      if (tmp2)
        {
          thefile = archive::extract (tmp2);
          // tmp2 now owned by archive instance
          if (thefile) tmp2 = 0;
        }
      else
        {
          thefile = archive::extract (tmp);
          // tmp now owned by archive instance
          if (thefile) tmp = 0;
        }
    }
    
  /* FIXME: potential leak of either *tmp or *tmp2 */
  if (thefile)
    {
      String fn;
      if (ver.Type () == package_binary)
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
	      lst->write (tmp.c_str(), tmp.size());
	    }

	  String canonicalfn = prefixPath + fn;
	  if (Script::isAScript (fn))
	    pkgm.desired.addScript (Script (canonicalfn));

	  Progress.SetText3 (canonicalfn.c_str());
	  log (LOG_BABBLE) << "Installing file " << prefixURL << prefixPath 
            << fn << endLog;
	  if (archive::extract_file (thefile, prefixURL, prefixPath) != 0)
	    {
	      if (NoReplaceOnReboot)
		{
		  ++errors;
                  error_in_this_package = true;
		  log (LOG_PLAIN) << "Not replacing in-use file "
                    << prefixURL << prefixPath << fn << endLog;
		}
	      else
	      //extract to temp location
	      if (archive::extract_file (thefile, prefixURL, prefixPath, ".new") != 0)
		{
		  log (LOG_PLAIN) << "Unable to install file "
                    << prefixURL << prefixPath << fn << endLog;
		  ++errors;
                  error_in_this_package = true;
		}
	      else
		{
                  if (!IsWindowsNT())
		    {
		      /* Get the short file names */
		      char source[MAX_PATH];
		      unsigned int len =
			GetShortPathName (cygpath (String ("/") + fn +
						   ".new").c_str(),
					  source, MAX_PATH);
		      if (!len || len > MAX_PATH)
			{
			  replaceOnRebootFailed(fn);
			}
		      else
			{
			  char dest[MAX_PATH];
			  len =
			    GetShortPathName (cygpath (String ("/") +
						       fn).c_str(),
					      dest, MAX_PATH);
			  if (!len || len > MAX_PATH)
			      replaceOnRebootFailed (fn);
			  else
			    /* trigger a replacement on reboot */
			  if (!WritePrivateProfileString
				("rename", dest, source, "WININIT.INI"))
			      replaceOnRebootFailed (fn);
			  else
			      replaceOnRebootSucceeded (fn, rebootneeded);
			}
		    }
                  else
                    {
		      /* XXX FIXME: prefix may not be / for in use files -
		       * although it most likely is
		       * - we need a io method to get win32 paths 
		       * or to wrap this system call
		       */
		      if (!MoveFileEx (cygpath (String ("/") + fn +
						".new").c_str(),
				       cygpath (String ("/") + fn).c_str(),
				       MOVEFILE_DELAY_UNTIL_REBOOT |
				       MOVEFILE_REPLACE_EXISTING))
			{
			  replaceOnRebootFailed (fn);
			}
		      else
			{
			  replaceOnRebootSucceeded (fn, rebootneeded);
			}
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

  int df = diskfull (get_root_dir ().c_str());
  Progress.SetBar3 (df);

  if (lst) delete lst;
  if (tmp2) delete tmp2;
  if (tmp) delete tmp;

  if (ver.Type () == package_binary && !error_in_this_package)
    pkgm.installed = ver;
}

static void
check_for_old_cygwin ()
{
  char buf[MAX_PATH + sizeof ("\\cygwin1.dll")];
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

  num_installs = 0, num_uninstalls = 0;
  rebootneeded = false;

  io_stream::mkpath_p (PATH_TO_DIR, String ("file://") + get_root_dir ());

  for (i = 0; Installer::StandardDirs[i]; i++)
  {
    String p = cygpath (Installer::StandardDirs[i]);
    if (p.size())
      io_stream::mkpath_p (PATH_TO_DIR, String ("file://") + p);
  }

  /* Create /var/run/utmp */
  io_stream *utmp = io_stream::open ("cygfile:///var/run/utmp", "wb");
  delete utmp;

  Installer myInstaller;
  myInstaller.initDialog();

  total_bytes = 0;
  total_bytes_sofar = 0;

  int df = diskfull (get_root_dir ().c_str());
  Progress.SetBar3 (df);

  int istext = (root_text == IDC_ROOT_TEXT) ? 1 : 0;
  int issystem = (root_scope == IDC_ROOT_SYSTEM) ? 1 : 0;

  create_mount ("/", get_root_dir (), istext, issystem);
  create_mount ("/usr/bin", cygpath ("/bin"), istext, issystem);
  create_mount ("/usr/lib", cygpath ("/lib"), istext, issystem);
  set_cygdrive_flags (istext, issystem);

  /* Let's hope people won't uninstall packages before installing [b]ash */
  init_run_script ();

  vector <packagemeta *> install_q, uninstall_q, sourceinstall_q;

  packagedb db;
  for (vector <packagemeta *>::iterator i = db.packages.begin ();
       i != db.packages.end (); ++i)
  {
    packagemeta & pkg = **i;

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
      {
        total_bytes += pkg.desired.source()->size;
        install_q.push_back (&pkg);
      }
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
      {
        total_bytes += pkg.desired.sourcePackage ().source()->size;
        sourceinstall_q.push_back (&pkg);
      }
    }

    if ((pkg.installed && pkg.desired != pkg.installed)
        || pkg.installed.picked ())
    {
      uninstall_q.push_back (&pkg);
    }
  }

  /* start with uninstalls - remove files that new packages may replace */
  for (vector <packagemeta *>::iterator i = uninstall_q.begin ();
       i != uninstall_q.end (); ++i)
  {
    myInstaller.preremoveOne (**i);
  }
  for (vector <packagemeta *>::iterator i = uninstall_q.begin ();
       i != uninstall_q.end (); ++i)
  {
    myInstaller.uninstallOne (**i);
  }

  for (vector <packagemeta *>::iterator i = install_q.begin ();
       i != install_q.end (); ++i)
  {
    packagemeta & pkg = **i;
    try {
      myInstaller.installOne (pkg, pkg.desired, *pkg.desired.source(),
                              "cygfile://", "/");
    }
    catch (exception *e)
    {
      if (yesno (owner, IDS_INSTALL_ERROR, e->what()) != IDYES)
      {
        log (LOG_TIMESTAMP)
          << "User cancelled setup after install error" << endLog;
        LogSingleton::GetInstance().exit (1);
        return;
      }
    }
  }

  for (vector <packagemeta *>::iterator i = sourceinstall_q.begin ();
       i != sourceinstall_q.end (); ++i)
  {
    packagemeta & pkg = **i;
    myInstaller.installOne (pkg, pkg.desired.sourcePackage(),
                            *pkg.desired.sourcePackage().source(),
                            "cygfile://", "/usr/src/");
  }

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

  if (!myInstaller.errors)
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

  if (myInstaller.errors)
    exit_msg = IDS_INSTALL_INCOMPLETE;
  else if (!unattended_mode)
    exit_msg = IDS_INSTALL_COMPLETE;
}

static DWORD WINAPI
do_install_reflector (void *p)
{
  HANDLE *context;
  context = (HANDLE *) p;

  try
  {
    do_install_thread ((HINSTANCE) context[0], (HWND) context[1]);

    // Tell the progress page that we're done downloading
    Progress.PostMessage (WM_APP_INSTALL_THREAD_COMPLETE);
  }
  TOPLEVEL_CATCH("install");

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

void md5_one (const packagesource& pkgsource)
{
  if (pkgsource.md5.isSet())
  {
    String fullname (pkgsource.Cached ());

    io_stream *thefile = io_stream::open (fullname, "rb");
    if (!thefile)
      throw new Exception (TOSTRING (__LINE__) " " __FILE__,
                           String ("IO Error opening ") + fullname,
                           APPERR_IO_ERROR);
    MD5Sum tempMD5;
    tempMD5.begin ();

    log (LOG_BABBLE) << "Checking MD5 for " << fullname << endLog;

    Progress.SetText1 ((String ("Checking MD5 for ")
                        + pkgsource.Base ()).c_str ());
    Progress.SetText4 ("Progress:");
    Progress.SetBar1 (0);

    unsigned char buffer[16384];
    ssize_t count;
    while ((count = thefile->read (buffer, sizeof (buffer))) > 0)
    {
      tempMD5.append (buffer, count);
      Progress.SetBar1 (thefile->tell (), thefile->get_size ());
    }
    delete thefile;
    if (count < 0)
      throw new Exception (TOSTRING(__LINE__) " " __FILE__,
                           String ("IO Error reading ") + fullname,
                           APPERR_IO_ERROR);

    tempMD5.finish ();

    if (pkgsource.md5 != tempMD5)
    {
      log (LOG_BABBLE) << "INVALID PACKAGE: " << fullname
        << " - MD5 mismatch: Ini-file: " << pkgsource.md5.str()
        << " != On-disk: " << tempMD5.str() << endLog;
      throw new Exception (TOSTRING(__LINE__) " " __FILE__,
                           String ("MD5 failure for ") + fullname,
                           APPERR_CORRUPT_PACKAGE);
    }

    log (LOG_BABBLE) << "MD5 verified OK: " << fullname << " "
      << pkgsource.md5.str() << endLog;
  }
}
