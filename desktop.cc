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

/* The purpose of this file is to manage all the desktop setup, such
   as start menu, batch files, desktop icons, and shortcuts.  Note
   that unlike other do_* functions, this one is called directly from
   install.cc */

#include "win32.h"
#include "shlobj.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "resource.h"
#include "msg.h"
#include "state.h"
#include "concat.h"
#include "mkdir.h"
#include "dialog.h"

#include "port.h"

extern "C" {
  void make_link_2 (char *exepath, char *args, char *icon, char *lname);
};

#define COMMAND9XARGS "/E:4096 /c "
#define COMMAND9XEXE  "\\command.com"

static char *batname;
static char *iconname;

static char *
backslash (char *s)
{
  for (char *t = s; *t; t++)
    if (*t == '/')
      *t = '\\';
  return s;
}

static void
make_link (char *linkpath, char *title, char *target)
{
  char *fname = concat (linkpath, "/", title, ".lnk", 0);

  if (_access (fname, 0) == 0)
    return; /* already exists */

  msg ("make_link %s, %s, %s\n", fname, title, target);

  mkdir_p (0, fname);

  char *cmdline, *exepath, *args;
  OSVERSIONINFO verinfo;
  verinfo.dwOSVersionInfoSize = sizeof (verinfo);

  /* If we are running Win9x, build a command line. */
  GetVersionEx (&verinfo);
  if (verinfo.dwPlatformId == VER_PLATFORM_WIN32_NT)
    {
      cmdline = target;
      exepath = target;
      args = "";
    }
  else
    {
      char *pccmd;
      char windir[MAX_PATH];

      GetWindowsDirectory (windir, sizeof (windir));
      cmdline = concat (windir, COMMAND9XEXE, " ", COMMAND9XARGS,  target, 0);
      exepath = concat (windir, COMMAND9XEXE, 0);
      args = concat (COMMAND9XARGS, target, 0);
    }

  make_link_2 (exepath, args, iconname, fname);
}

static void
start_menu (char *title, char *target)
{
  char path[_MAX_PATH];
  LPITEMIDLIST id;
  SHGetSpecialFolderLocation (NULL, CSIDL_PROGRAMS, &id);
  SHGetPathFromIDList (id, path);
  strcat (path, "/Cygnus Solutions");
  make_link (path, title, target);
}

static void
desktop_icon (char *title, char *target)
{
  char path[_MAX_PATH];
  LPITEMIDLIST id;
  SHGetSpecialFolderLocation (NULL, CSIDL_DESKTOP, &id);
  SHGetPathFromIDList (id, path);
  make_link (path, title, target);
}

static void
make_cygwin_bat ()
{
  batname = concat (root_dir, "/cygwin.bat", 0);

  /* if the batch file exists, don't overwrite it */
  if (_access (batname, 0) == 0)
    return;

  FILE *bat = fopen (batname, "wt");
  if (!bat)
    return;

  fprintf (bat, "@echo off\n\n");

  fprintf (bat, "SET MAKE_MODE=unix\n");

  char *bin = backslash (concat (root_dir, "/bin", 0));
  char *usrbin = backslash (concat (root_dir, "/usr/bin", 0));
  char *usrlocalbin = backslash (concat (root_dir, "/usr/local/bin", 0));

  fprintf (bat, "SET PATH=%s;%s;%s;\"%%PATH%%\"\n", usrlocalbin, usrbin, bin);

  fprintf (bat, "%.2s\n", root_dir);
  fprintf (bat, "chdir %s\n\n", backslash (_strdup (root_dir+2)));

  fprintf (bat, "bash --login -i\n");

  fclose (bat);

  free (bin);
  free (usrbin);
  free (usrlocalbin);
}

static void
save_icon ()
{
  iconname = concat (root_dir, "/cygwin.ico", 0);

  HRSRC rsrc = FindResource (NULL, "CYGWIN.ICON", "FILE");
  if (rsrc == NULL)
    {
      fatal ("FindResource failed");
    }
  HGLOBAL res = LoadResource (NULL, rsrc);
  char *data = (char *) LockResource (res);
  int len = SizeofResource (NULL, rsrc);

  FILE *f = fopen (iconname, "wb");
  if (f)
    {
      fwrite (data, 1, len, f);
      fclose (f);
    }
}

void
do_desktop (HINSTANCE h)
{
  CoInitialize (NULL);

  save_icon ();

  make_cygwin_bat ();

  start_menu ("Cygwin 1.1 Bash Shell", batname);

  desktop_icon ("Cygwin", batname);
}
