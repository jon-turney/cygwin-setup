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
   files in /etc/setup/* and create the mount points. */

static char *cvsid = "\n%%% $Id$\n";

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
#include "tar.h"
#include "diskfull.h"
#include "msg.h"
#include "mount.h"
#include "log.h"
#include "hash.h"
#include "mount.h"
#include "filemanip.h"

#include "port.h"

char *known_file_types[] = {
  "tar.gz",
  "tar.bz2",
  0
};


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
}

static BOOL CALLBACK
dialog_proc (HWND h, UINT message, WPARAM wParam, LPARAM lParam)
{
  int i, j;
  HWND listbox;
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
  while (GetMessage (&m, 0, 0, 0) > 0) {
    TranslateMessage (&m);
    DispatchMessage (&m);
  }
}

static DWORD start_tics;

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

static void
badrename (char *o, char *n)
{
  char buf[1000];
  char *err = strerror (errno);
  if (!err)
    err = "(unknown error)";
  note (IDS_ERR_RENAME, o, n, err);
}

static char *standard_dirs[] = {
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

void
hash::add_subdirs (char *path)
{
  char *nonp, *pp;
  for (nonp = path; *nonp == '\\' || *nonp == '/'; nonp++);
  for (pp = path + strlen(path) - 1; pp>nonp; pp--)
    if (*pp == '/' || *pp == '\\')
      {
	int i, s=0;
	char c = *pp;
	*pp = 0;
	for (i=0; standard_dirs[i]; i++)
	  if (strcmp (standard_dirs[i]+1, path) == 0)
	    {
	      s = 1;
	      break;
	    }
	if (s == 0)
	  add (path);
	*pp = c;
      }
}

static int
exists (const char *file)
{
  if (_access (file, 0) == 0)
    return 1;
  return 0;
}

static int num_installs, num_uninstalls;

static void
uninstall_one (Package *pkg, bool src)
{
  hash dirs;
  char line[_MAX_PATH];

  gzFile lst = gzopen (cygpath ("/etc/setup/", pkg->name,
				(src ? "-src.lst.gz" : ".lst.gz"), 0), "rb");
  if (lst)
    {
      SetWindowText (ins_pkgname, pkg->name);
      SetWindowText (ins_action, "Uninstalling...");
      if (pkg->action == ACTION_UNINSTALL)
	log (0, "Uninstalling %s", pkg->name);
      else
	log (0, "Uninstalling old %s", pkg->name);

      while (gzgets (lst, line, sizeof (line)))
	{
	  if (line[strlen(line)-1] == '\n')
	    line[strlen(line)-1] = 0;

	  dirs.add_subdirs (line);

	  char *d = cygpath ("/", line, NULL);
	  DWORD dw = GetFileAttributes (d);
	  if (dw != 0xffffffff && !(dw & FILE_ATTRIBUTE_DIRECTORY))
	    {
	      log (LOG_BABBLE, "unlink %s", d);
	      DeleteFile (d);
	    }
	}
      gzclose (lst);

      remove (cygpath ("/etc/setup/", pkg->name, ".lst.gz", 0));

      dirs.reverse_sort ();
      char *subdir = 0;
      while ((subdir = dirs.enumerate (subdir)) != 0)
	{
	  char *d = cygpath ("/", subdir, NULL);
	  if (RemoveDirectory (d))
	    log (LOG_BABBLE, "rmdir %s", d);
	}
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
install_one (Package *pkg, bool isSrc)
{
  int errors = 0;
  char *extra;
  const char *file;
  int file_size;
  Info *pi = pkg->info + pkg->trust;

  if (!isSrc)
    {
      extra = "";
      file_size = pi->install_size;
      file = pi->install;
    }
  else if (pi->source)
    {
      extra = "-src";
      file_size = pi->source_size;
      file = pi->source;
    }
  else
    return 0;

  char name[strlen (pkg->name) + strlen (extra) + 1];
  strcat (strcpy (name, pkg->name), extra);

  char file_buf[MAX_PATH + 1];
  int file_exists = 0;
  int ext = find_tar_ext(file) + 1;
  strncpy (file_buf, file, ext);
  file_buf[ext] = '\0';
  file = (char *) &file_buf;
  char *basef = base (file);
  
  for (int c = 0; !file_exists && known_file_types[c]; c++)
    {
      strcpy ((char *) &file_buf[ext], known_file_types[c]);
      file_exists = exists (file) || exists (basef);
    }
  if (!file_exists)
    {
      note (IDS_ERR_OPEN_READ, file, "No such file");
      return 1;
    }
  else
    SetWindowText (ins_pkgname, basef);

  gzFile lst = gzopen (cygpath ("/etc/setup/", name, ".lst.gz", 0),
		       "wb9");

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
    }

  SetWindowText (ins_action, msg);
  log (0, "%s%s", msg, file);
  tar_open (file);

  char *fn;
  while (fn = tar_next_file ())
    {
      char *dest_file;

      if (lst)
	gzprintf (lst, "%s\n", fn);

      dest_file = cygpath (isSrc ? "/usr/src/" : "/", fn, NULL);

      SetWindowText (ins_filename, dest_file);
      log (LOG_BABBLE, "Installing file %s", dest_file);
      if (tar_read_file (dest_file) != 0)
	{
	  log (0, "Unable to install file %s", dest_file);
	  errors++;
	}

      progress (tar_ftell ());
      free (dest_file); 
      num_installs++;
    }
  tar_close ();

  total_bytes_sofar += file_size;
  progress (0);

  int df = diskfull (get_root_dir ());
  SendMessage (ins_diskfull, PBM_SETPOS, (WPARAM) df, 0);

  if (lst)
    gzclose (lst);

  if (!errors)
    {
      Info *inf = pkg->info + pkg->trust;
      pkg->installed_ix = pkg->trust;
      if (pkg->installed)
	free (pkg->installed);
      pkg->installed = new Info (inf->install, inf->version, inf->install_size, inf->source, inf->source_size);
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
  sprintf (msg, "An old version of cygwin1.dll was found here:\r\n%s\r\nDelete?", buf);
  switch (MessageBox (NULL, msg,
		      "What's that doing there?", MB_YESNO | MB_ICONQUESTION | MB_TASKMODAL))
    {
    case IDYES:
      if (!DeleteFile (buf))
	{
	  sprintf (msg, "Couldn't delete file %s.\r\n"
			"Is the DLL in use by another application?\r\n"
			"You should delete the old version of cygwin1.dll\r\nat your earliest convenience.",
			buf);
	  MessageBox (NULL, buf, "Couldn't delete file", MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);
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

  for (i=0; standard_dirs[i]; i++)
    {
      char *p = cygpath (standard_dirs[i], 0);
      mkdir_p (1, p);
      free (p);
    }

  /* Create /var/run/utmp */
  char *utmp = cygpath ("/var/run/utmp", 0);
  FILE *ufp = fopen (utmp, "wb");
  if (ufp)
    fclose (ufp);
  free (utmp);

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

  for (Package *pkg = package; pkg->name; pkg++)
    {
      Info *pi = pkg->info + pkg->trust;
      if (pkg->action != ACTION_SRC_ONLY)
	total_bytes += pi->install_size;
      if (pkg->srcpicked)
	total_bytes += pi->source_size;
    }

  for (Package *pkg = package; pkg->name; pkg++)
    {
      if (is_uninstall_action (pkg))
	{
	  uninstall_one (pkg, 0);
	  uninstall_one (pkg, 1);
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
    } // end of big package loop

  ShowWindow (ins_dialog, SW_HIDE);

  char *odbn = cygpath ("/etc/setup/installed.db", 0);
  char *ndbn = cygpath ("/etc/setup/installed.db.new", 0);
  char *sdbn = cygpath ("/etc/setup/installed.db.old", 0);

  mkdir_p (0, ndbn);

  FILE *odb = fopen (odbn, "rt");
  FILE *ndb = fopen (ndbn, "wb");

  if (!ndb)
    {
      char *err = strerror (errno);
      if (!err)
	err = "(unknown error)";
      fatal (IDS_ERR_OPEN_WRITE, ndb, err);
    }

  if (odb)
    {
      char line[1000], pkgname[1000];
      int printit;
      while (fgets (line, 1000, odb))
	{
	  sscanf (line, "%s", pkgname);
	  Package *pkg = getpkgbyname (pkgname);
	  if (!pkg || (!is_download_action (pkg) && pkg->action != ACTION_UNINSTALL))
	    fputs (line, ndb);
	}

    }

  for (Package *pkg = package; pkg->name; pkg++)
    if (is_download_action (pkg))
      {
	Info *pi = pkg->info + pkg->installed_ix;
	if (pkg->srcpicked)
	  fprintf (ndb, "%s %s %d %s %d\n", pkg->name,
		   pi->install, pi->install_size,
		   pi->source, pi->source_size);
	else
	  fprintf (ndb, "%s %s %d\n", pkg->name,
		   pi->install, pi->install_size);
      }

  if (odb)
    fclose (odb);
  fclose (ndb);

  remove (sdbn);
  if (odb && rename (odbn, sdbn))
    badrename (odbn, sdbn);

  remove (odbn);
  if (rename (ndbn, odbn))
    badrename (ndbn, odbn);

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
