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
uninstall_one (Package * pkg)
{
  packagedb db;
  packagemeta *pkgm = db.getpackagebyname (pkg->name);

  if (pkgm)
    {
      SetWindowText (ins_pkgname, pkg->name);
      SetWindowText (ins_action, "Uninstalling...");
      if (pkg->action == ACTION_UNINSTALL)
	log (0, "Uninstalling %s", pkg->name);
      else
	log (0, "Uninstalling old %s", pkg->name);
      pkgm->uninstall ();

      num_uninstalls++;

      pkg->installed_ix = TRUST_UNKNOWN;

      if (pkg->installed)
	{
	  free (pkg->installed);
	  pkg->installed = NULL;
	}
    }
}


static int
install_one (Package * pkg, bool isSrc)
{
  int errors = 0;
  const char *extra;
  const char *file;
  int file_size;
  archive *thefile = NULL;
  Info *pi = pkg->info + pkg->trust;

  if (!isSrc)
    {
      extra = "";
      file_size = pi->install_size;
      file = concat ("file://", pi->install, 0);
    }
  else if (pi->source)
    {
      extra = "-src";
      file_size = pi->source_size;
      file = concat ("file://", pi->source, 0);
    }
  else
    return 0;

  char name[strlen (pkg->name) + strlen (extra) + 1];
  strcat (strcpy (name, pkg->name), extra);

  /* FIXME: this may file now */
  char *basef = base (file);
  SetWindowText (ins_pkgname, basef);

  if (!io_stream::exists (file))
    file = basef;
  if (!io_stream::exists (file))
    {
      note (IDS_ERR_OPEN_READ, file, "No such file");
      return 1;
    }

  io_stream *tmp =
    io_stream::open (concat ("cygfile:///etc/setup/", name, ".lst.gz", 0),
		     "wb");
  io_stream *lst = new compress_gz (tmp, "w9");
  if (lst->error ())
    {
      delete lst;
      lst = NULL;
    }

  package_bytes = file_size;

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
  tmp = io_stream::open (file, "rb");
  if (tmp)
    {
      io_stream *tmp2 = compress::decompress (tmp);
      if (tmp2)
	thefile = archive::extract (tmp2);
      else
	thefile = archive::extract (tmp);
      /* FIXME: potential leak of either *tmp or *tmp2 */
      if (thefile)
	{
	  const char *fn;
	  while ((fn = thefile->next_file_name ()))
	    {
	      char *dest_file;

	      if (lst)
		lst->write (concat (fn, "\n", 0), strlen (fn) + 1);

	      dest_file =
		concat ("cygfile://", isSrc ? "/usr/src/" : "/", NULL);

	      /* FIXME: concat leaks memory */
	      SetWindowText (ins_filename, concat (dest_file, fn, 0));
	      log (LOG_BABBLE, "Installing file %s%s", dest_file, fn);
	      if (archive::extract_file (thefile, dest_file) != 0)
		{
		  log (0, "Unable to install file %s%s", dest_file, fn);
		  errors++;
		}

	      progress (tmp->tell ());
	      free (dest_file);
	      num_installs++;
	    }
	  delete thefile;

	  total_bytes_sofar += file_size;
	}
    }
  progress (0);

  int df = diskfull (get_root_dir ());
  SendMessage (ins_diskfull, PBM_SETPOS, (WPARAM) df, 0);

  if (lst)
    delete lst;

  if (!errors)
    {
      Info *inf = pkg->info + pkg->trust;
      pkg->installed_ix = pkg->trust;
      if (pkg->installed)
	free (pkg->installed);
      pkg->installed =
	new Info (inf->install, inf->version, inf->install_size, inf->source,
		  inf->source_size);
    }

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

  for (Package * pkg = package; pkg->name; pkg++)
    {
      Info *pi = pkg->info + pkg->trust;
      if (pkg->action != ACTION_SRC_ONLY)
	total_bytes += pi->install_size;
      if (pkg->srcpicked)
	total_bytes += pi->source_size;
    }

  for (Package * pkg = package; pkg->name; pkg++)
    {
      if (is_uninstall_action (pkg))
	{
	  uninstall_one (pkg);
	}

      if (is_download_action (pkg))
	{
	  int e = 0;
	  if (pkg->action != ACTION_SRC_ONLY)
	    e += install_one (pkg, FALSE);
	  if (pkg->srcpicked)
	    e += install_one (pkg, TRUE);
	  if (e)
	    {
	      pkg->action = ACTION_ERROR;
	      errors++;
	    }
	}
    }				// end of big package loop

  ShowWindow (ins_dialog, SW_HIDE);

  packagedb db;
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
