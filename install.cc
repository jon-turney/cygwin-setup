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

/* The purpose of this file is to intall all the packages selected in
   the install list (in ini.h).  Note that we use a separate thread to
   maintain the progress dialog, so we avoid the complexity of
   handling two tasks in one thread.  We also create or update all the
   files in /etc/setup/\* and create the mount points. */

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include "win32.h"
#include "commctrl.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include "zlib/zlib.h"

#include "resource.h"
#include "ini.h"
#include "dialog.h"
#include "concat.h"
#include "geturl.h"
#include "mkdir.h"
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

#include "package_db.h"
#include "package_meta.h"
#include "package_version.h"
#include "package_source.h"

#include "port.h"

static HWND ins_dialog = 0;
static HWND ins_action = 0;
static HWND ins_pkgname = 0;
static HWND ins_filename = 0;
static HWND ins_pprogress = 0;
static HWND ins_iprogress = 0;
static HWND ins_diskfull = 0;
static HANDLE init_event;

static int total_bytes = 0;
static int total_bytes_sofar = 0;
static int package_bytes = 0;

static bool
dialog_cmd (HWND h, int id, HWND hwndctl, UINT code)
{
  switch (id)
    {
    case IDCANCEL:
      exit_setup (1);
    }
  return 0;
}

static BOOL CALLBACK
dialog_proc (HWND h, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
    {
    case WM_INITDIALOG:
      ins_dialog = h;
      ins_action = GetDlgItem (h, IDC_INS_ACTION);
      ins_pkgname = GetDlgItem (h, IDC_INS_PKG);
      ins_filename = GetDlgItem (h, IDC_INS_FILE);
      ins_pprogress = GetDlgItem (h, IDC_INS_PPROGRESS);
      ins_iprogress = GetDlgItem (h, IDC_INS_IPROGRESS);
      ins_diskfull = GetDlgItem (h, IDC_INS_DISKFULL);
      SetEvent (init_event);
      return TRUE;
    case WM_COMMAND:
      return HANDLE_WM_COMMAND (h, wParam, lParam, dialog_cmd);
    }
  return FALSE;
}

static WINAPI DWORD
dialog (void *)
{
  int rv = 0;
  MSG m;
  HWND ins_dialog = CreateDialog (hinstance, MAKEINTRESOURCE (IDD_INSTATUS),
				  0, dialog_proc);
  if (ins_dialog == 0)
    fatal ("create dialog");
  ShowWindow (ins_dialog, SW_SHOWNORMAL);
  UpdateWindow (ins_dialog);
  while (GetMessage (&m, 0, 0, 0) > 0)
    {
      TranslateMessage (&m);
      DispatchMessage (&m);
    }
  return rv;
}

static void
init_dialog ()
{
  if (ins_dialog == 0)
    {
      DWORD tid;
      HANDLE thread;
      init_event = CreateEvent (0, 0, 0, 0);
      thread = CreateThread (0, 0, dialog, 0, 0, &tid);
      WaitForSingleObject (init_event, 10000);
      CloseHandle (init_event);
      SendMessage (ins_pprogress, PBM_SETRANGE, 0, MAKELPARAM (0, 100));
      SendMessage (ins_iprogress, PBM_SETRANGE, 0, MAKELPARAM (0, 100));
      SendMessage (ins_diskfull, PBM_SETRANGE, 0, MAKELPARAM (0, 100));
    }

  SetWindowText (ins_pkgname, "");
  SetWindowText (ins_filename, "");
  SendMessage (ins_pprogress, PBM_SETPOS, (WPARAM) 0, 0);
  SendMessage (ins_iprogress, PBM_SETPOS, (WPARAM) 0, 0);
  SendMessage (ins_diskfull, PBM_SETPOS, (WPARAM) 0, 0);
  ShowWindow (ins_dialog, SW_SHOWNORMAL);
}

static void
progress (int bytes)
{
  int perc;

  if (package_bytes > 100)
    {
      perc = bytes / (package_bytes / 100);
      SendMessage (ins_pprogress, PBM_SETPOS, (WPARAM) perc, 0);
    }

  if (total_bytes > 100)
    {
      perc = (total_bytes_sofar + bytes) / (total_bytes / 100);
      SendMessage (ins_iprogress, PBM_SETPOS, (WPARAM) perc, 0);
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

static int num_installs, num_uninstalls;

/* FIXME: upgrades should be a method too */
static void
uninstall_one (packagemeta * pkgm)
{
  if (pkgm)
    {
      SetWindowText (ins_pkgname, pkgm->name);
      SetWindowText (ins_action, "Uninstalling...");
      log (0, "Uninstalling %s", pkgm->name);
      pkgm->uninstall ();
      num_uninstalls++;
    }
}



/* install one source at a given prefix. */
static int
install_one_source (packagesource & source, char const *prefix,
		    package_type_t type)
{
  int errors = 0;
  SetWindowText (ins_pkgname, source.Base ());
  if (!io_stream::exists (source.Cached ()))
    {
      note (IDS_ERR_OPEN_READ, source.Cached (), "No such file");
      return 1;
    }
  io_stream *lst = 0;
  if (type == package_binary)
    {
      io_stream *tmp =
	io_stream::
	open (concat ("cygfile:///etc/setup/", source.Base (), ".lst.gz", 0),
	      "wb");
      lst = new compress_gz (tmp, "w9");
      if (lst->error ())
	{
	  delete lst;
	  lst = NULL;
	}
    }

  package_bytes = source.size;

  char msg[64];
  strcpy (msg, "Installing");
  SetWindowText (ins_action, msg);
  log (0, "%s%s", msg, source.Cached ());
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
      const char *fn;
      while ((fn = thefile->next_file_name ()))
	{
	  if (lst)
	    lst->write (concat (fn, "\n", 0), strlen (fn) + 1);

	  /* FIXME: concat leaks memory */
	  SetWindowText (ins_filename, concat (prefix, fn, 0));
	  log (LOG_BABBLE, "Installing file %s%s", prefix, fn);
	  if (archive::extract_file (thefile, prefix) != 0)
	    {
	      log (0, "Unable to install file %s%s", prefix, fn);
	      errors++;
	    }

	  progress (tmp->tell ());
	  num_installs++;
	}
      delete thefile;

      total_bytes_sofar += package_bytes;
    }


  progress (0);

  int df = diskfull (get_root_dir ());
  SendMessage (ins_diskfull, PBM_SETPOS, (WPARAM) df, 0);

  if (lst)
    delete lst;

  return errors;
}

/* install a package, install both the binary and source aspects if needed */
static int
install_one (packagemeta & pkg)
{
  int errors = 0;

  if (pkg.desired->srcpicked)
    errors += install_one_source (pkg.desired->src, "cygfile:///usr/src", package_source);
  if (pkg.desired->binpicked)
    errors += install_one_source (pkg.desired->bin, "cygfile:///", package_binary);

  /* FIXME: make a upgrade method and reinstate this */
#if 0
  char msg[64];
  if (!pkg->installed)
    strcpy (msg, "Installing");
  else
    {
      int n = strcmp (pi->version, pkg->installed->version);
      if (n < 0)
	strcpy (msg, "Reverting");
      else if (n == 0)
	strcpy (msg, "Reinstalling");
      else
	strcpy (msg, "Upgrading");
    }

  switch (pkg->action)
    {
    case ACTION_PREV:
      strcat (msg, " previous version...");
      break;
    case ACTION_CURR:
      strcat (msg, "...");
      break;
    case ACTION_TEST:
      strcat (msg, " test version...");
      break;
    default:
      /* FIXME: log this somehow */
      break;
    }
  SetWindowText (ins_action, msg);
  log (0, "%s%s", msg, file);
#endif

  if (!errors)
    if (pkg.desired->binpicked)
      pkg.installed = pkg.desired;

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

void
do_install (HINSTANCE h)
{
  int i;
  int errors = 0;

  num_installs = 0, num_uninstalls = 0;

  next_dialog = IDD_DESKTOP;

  mkdir_p (1, get_root_dir ());

  for (i = 0; standard_dirs[i]; i++)
    {
      char *p = cygpath (standard_dirs[i], 0);
      mkdir_p (1, p);
      free (p);
    }

  /* Create /var/run/utmp */
  io_stream *utmp = io_stream::open ("cygfile:///var/run/utmp", "wb");
  delete utmp;

  dismiss_url_status_dialog ();

  init_dialog ();

  total_bytes = 0;
  total_bytes_sofar = 0;

  int df = diskfull (get_root_dir ());
  SendMessage (ins_diskfull, PBM_SETPOS, (WPARAM) df, 0);

  int istext = (root_text == IDC_ROOT_TEXT) ? 1 : 0;
  int issystem = (root_scope == IDC_ROOT_SYSTEM) ? 1 : 0;

  create_mount ("/", get_root_dir (), istext, issystem);
  create_mount ("/usr/bin", cygpath ("/bin", 0), istext, issystem);
  create_mount ("/usr/lib", cygpath ("/lib", 0), istext, issystem);
  set_cygdrive_flags (istext, issystem);

  packagedb db;
  for (packagemeta * pkg = db.getfirstpackage (); pkg;
           pkg = db.getnextpackage ())
  
    if (pkg->desired && (pkg->desired->srcpicked || pkg->desired->binpicked))
    {
      if (pkg->desired->srcpicked)
	total_bytes += pkg->desired->src.size;
      if (pkg->desired->binpicked)
	total_bytes += pkg->desired->bin.size;
    }
    
  for (packagemeta * pkg = db.getfirstpackage (); pkg;
             pkg = db.getnextpackage ())  
    {
      if (!pkg->desired || pkg->desired != pkg->installed)
	{
	  uninstall_one (pkg);
	}

      if (pkg->desired && (pkg->desired->srcpicked || pkg->desired->binpicked))
	{
	  int e = 0;
	  e += install_one (*pkg);
	  if (e)
	    {
	      errors++;
	    }
	}
    }				// end of big package loop

  ShowWindow (ins_dialog, SW_HIDE);

  int temperr;
  if ((temperr = db.flush ()))
    {
      const char *err = strerror (temperr);
      if (!err)
	err = "(unknown error)";
      fatal (IDS_ERR_OPEN_WRITE, err);
    }

  if (!errors)
    check_for_old_cygwin ();
  if (num_installs == 0 && num_uninstalls == 0)
    {
      exit_msg = IDS_NOTHING_INSTALLED;
      return;
    }
  if (num_installs == 0)
    {
      exit_msg = IDS_UNINSTALL_COMPLETE;
      return;
    }

  if (errors)
    exit_msg = IDS_INSTALL_INCOMPLETE;
  else
    exit_msg = IDS_INSTALL_COMPLETE;
}
