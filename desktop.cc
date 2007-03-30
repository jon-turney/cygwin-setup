/*
 * Copyright (c) 2000, 2001 Red Hat, Inc.
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

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include "win32.h"
#include <shlobj.h>
#include "desktop.h"
#include "propsheet.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "resource.h"
#include "msg.h"
#include "state.h"
#include "dialog.h"
#include "mount.h"
#include "mklink2.h"
#include "package_db.h"
#include "package_meta.h"
#include "package_version.h"
#include "filemanip.h"
#include "io_stream.h"
#include "getopt++/BoolOption.h"
#include "PackageSpecification.h"
#include "LogFile.h"

static BoolOption NoShortcutsOption (false, 'n', "no-shortcuts", "Disable creation of desktop and start menu shortcuts");
static BoolOption NoStartMenuOption (false, 'N', "no-startmenu", "Disable creation of start menu shortcut");
static BoolOption NoDesktopOption (false, 'd', "no-desktop", "Disable creation of desktop shortcut");

/* Lines starting with '@' are conditionals - include 'N' for NT,
   '5' for Win95, '8' for Win98, '*' for all, like this:
	echo foo
	@N8
	echo NT or 98
	@*
   */

static std::string batname;
static std::string iconname;

static ControlAdjuster::ControlInfo DesktopControlsInfo[] = {
  {IDC_DESKTOP_SEPARATOR, 	CP_STRETCH, CP_BOTTOM},
  {IDC_STATUS, 			CP_LEFT, CP_BOTTOM},
  {IDC_STATUS_HEADER, 		CP_LEFT, CP_BOTTOM},
  {0, CP_LEFT, CP_TOP}
};

DesktopSetupPage::DesktopSetupPage ()
{
  sizeProcessor.AddControlInfo (DesktopControlsInfo);
}

static void
make_link (const std::string& linkpath,
           const std::string& title,
           const std::string& target)
{
  std::string fname = linkpath + "/" + title + ".lnk";

  if (_access (fname.c_str(), 0) == 0)
    return;			/* already exists */

  msg ("make_link %s, %s, %s\n",
       fname.c_str(), title.c_str(), target.c_str());

  io_stream::mkpath_p (PATH_TO_FILE, std::string ("file://") + fname);

  std::string exepath;
  std::string argbuf;

  if (IsWindowsNT ())
    {
      exepath = target;
      argbuf = " ";
    }
  else
    {
      char windir[MAX_PATH];

      GetWindowsDirectory (windir, sizeof (windir));
      exepath = std::string(windir) + "\\command.com";
      argbuf = "/E:4096 /c " + target;
    }

  msg ("make_link_2 (%s, %s, %s, %s)",
       exepath.c_str(), argbuf.c_str(),
       iconname.c_str(), fname.c_str());
  make_link_2 (exepath.c_str(), argbuf.c_str(),
	       iconname.c_str(), fname.c_str());
}

static void
start_menu (const std::string& title, const std::string& target)
{
  LPITEMIDLIST id;
  int issystem = (root_scope == IDC_ROOT_SYSTEM) ? 1 : 0;
  SHGetSpecialFolderLocation (NULL,
			      issystem ? CSIDL_COMMON_PROGRAMS :
			      CSIDL_PROGRAMS, &id);
  char _path[MAX_PATH];
  SHGetPathFromIDList (id, _path);
  std::string path(_path);
  // Win95 does not use common programs unless multiple users for Win95 is enabled
  msg ("Program directory for program link: %s", path.c_str());
  if (path.empty())
    {
      SHGetSpecialFolderLocation (NULL, CSIDL_PROGRAMS, &id);
      SHGetPathFromIDList (id, _path);
      path = _path;
      msg ("Program directory for program link changed to: %s",
	   path.c_str());
    }
// end of Win95 addition
  path += "/Cygwin";
  make_link (path, title, target);
}

static void
desktop_icon (const std::string& title, const std::string& target)
{
  char path[MAX_PATH];
  LPITEMIDLIST id;
  int issystem = (root_scope == IDC_ROOT_SYSTEM) ? 1 : 0;
  //SHGetSpecialFolderLocation (NULL, issystem ? CSIDL_DESKTOP : CSIDL_COMMON_DESKTOPDIRECTORY, &id);
  SHGetSpecialFolderLocation (NULL,
			      issystem ? CSIDL_COMMON_DESKTOPDIRECTORY :
			      CSIDL_DESKTOPDIRECTORY, &id);
  SHGetPathFromIDList (id, path);
// following lines added because it appears Win95 does not use common programs
// unless it comes into play when multiple users for Win95 is enabled
  msg ("Desktop directory for desktop link: %s", path);
  if (strlen (path) == 0)
    {
      SHGetSpecialFolderLocation (NULL, CSIDL_DESKTOPDIRECTORY, &id);
      SHGetPathFromIDList (id, path);
      msg ("Desktop directory for deskop link changed to: %s", path);
    }
// end of Win95 addition
  make_link (path, title, target);
}

static void
make_cygwin_bat ()
{
  batname = backslash (cygpath ("/Cygwin.bat"));

  /* if the batch file exists, don't overwrite it */
  if (_access (batname.c_str(), 0) == 0)
    return;

  FILE *bat = fopen (batname.c_str(), "wt");
  if (!bat)
    return;

  fprintf (bat, "@echo off\n\n");

  fprintf (bat, "%.2s\n", get_root_dir ().c_str());
  fprintf (bat, "chdir %s\n\n",
	   replace(backslash(get_root_dir() + "/bin"), "%", "%%").c_str());

  fprintf (bat, "bash --login -i\n");

  fclose (bat);
}

static void
save_icon ()
{
  iconname = backslash (cygpath ("/Cygwin.ico"));

  HRSRC rsrc = FindResource (NULL, "CYGWIN.ICON", "FILE");
  if (rsrc == NULL)
    {
      fatal ("FindResource failed");
    }
  HGLOBAL res = LoadResource (NULL, rsrc);
  char *data = (char *) LockResource (res);
  int len = SizeofResource (NULL, rsrc);

  FILE *f = fopen (iconname.c_str(), "wb");
  if (f)
    {
      fwrite (data, 1, len, f);
      fclose (f);
    }
}

static void
do_desktop_setup ()
{
  save_icon ();

  make_cygwin_bat ();

  if (root_menu)
    {
      start_menu ("Cygwin Bash Shell", batname);
    }

  if (root_desktop)
    {
      desktop_icon ("Cygwin", batname);
    }
}

static int da[] = { IDC_ROOT_DESKTOP, 0 };
static int ma[] = { IDC_ROOT_MENU, 0 };

static void
check_if_enable_next (HWND h)
{
  EnableWindow (GetDlgItem (h, IDOK), 1);
}

extern LogFile * theLog;

static void
set_status (HWND h)
{
  char buf[1000], fmt[1000];
  if (LoadString (hinstance, exit_msg, fmt, sizeof (fmt)) > 0)
    {
      snprintf (buf, 1000, fmt, backslash(theLog->getFileName(LOG_BABBLE)).c_str());
      ::SetWindowText (GetDlgItem (h, IDC_STATUS), buf);
    }
}

static void
load_dialog (HWND h)
{
  rbset (h, da, root_desktop);
  rbset (h, ma, root_menu);
  check_if_enable_next (h);
  set_status (h);
}

static int
check_desktop (const std::string title, const std::string target)
{
  char path[MAX_PATH];
  LPITEMIDLIST id;
  int issystem = (root_scope == IDC_ROOT_SYSTEM) ? 1 : 0;
  SHGetSpecialFolderLocation (NULL,
			      issystem ? CSIDL_COMMON_DESKTOPDIRECTORY :
			      CSIDL_DESKTOPDIRECTORY, &id);
  SHGetPathFromIDList (id, path);
  // following lines added because it appears Win95 does not use common programs
  // unless it comes into play when multiple users for Win95 is enabled
  msg ("Desktop directory for desktop link: %s", path);
  if (strlen (path) == 0)
    {
      SHGetSpecialFolderLocation (NULL, CSIDL_DESKTOPDIRECTORY, &id);
      SHGetPathFromIDList (id, path);
      msg ("Desktop directory for deskop link changed to: %s", path);
    }
  // end of Win95 addition
  std::string fname = std::string(path) + "/" + title + ".lnk";

  if (_access (fname.c_str(), 0) == 0)
    return 0;			/* already exists */

  fname = std::string(path) + "/" + title + ".pif";	/* check for a pif as well */

  if (_access (fname.c_str(), 0) == 0)
    return 0;			/* already exists */

  return IDC_ROOT_DESKTOP;
}

static int
check_startmenu (const std::string title, const std::string target)
{
  char path[MAX_PATH];
  LPITEMIDLIST id;
  int issystem = (root_scope == IDC_ROOT_SYSTEM) ? 1 : 0;
  SHGetSpecialFolderLocation (NULL,
			      issystem ? CSIDL_COMMON_PROGRAMS :
			      CSIDL_PROGRAMS, &id);
  SHGetPathFromIDList (id, path);
  // following lines added because it appears Win95 does not use common programs
  // unless it comes into play when multiple users for Win95 is enabled
  msg ("Program directory for program link: %s", path);
  if (strlen (path) == 0)
    {
      SHGetSpecialFolderLocation (NULL, CSIDL_PROGRAMS, &id);
      SHGetPathFromIDList (id, path);
      msg ("Program directory for program link changed to: %s", path);
    }
  // end of Win95 addition
  strcat (path, "/Cygwin");
  std::string fname = std::string(path) + "/" + title + ".lnk";

  if (_access (fname.c_str(), 0) == 0)
    return 0;			/* already exists */

  fname = std::string(path) + "/" + title + ".pif";	/* check for a pif as well */

  if (_access (fname.c_str(), 0) == 0)
    return 0;			/* already exists */

  return IDC_ROOT_MENU;
}

static void
save_dialog (HWND h)
{
  root_desktop = rbget (h, da);
  root_menu = rbget (h, ma);
}

static BOOL
dialog_cmd (HWND h, int id, HWND hwndctl, UINT code)
{
  switch (id)
    {

    case IDC_ROOT_DESKTOP:
    case IDC_ROOT_MENU:
      save_dialog (h);
      check_if_enable_next (h);
      break;
    }
  return 0;
}

bool
DesktopSetupPage::Create ()
{
  return PropertyPage::Create (NULL, dialog_cmd, IDD_DESKTOP);
}

void
DesktopSetupPage::OnInit ()
{
  // FIXME: This CoInitialize() feels like it could be moved to startup in main.cc.
  CoInitialize (NULL);

  if (NoShortcutsOption) 
    {
      root_desktop = root_menu = 0;
    }
  else
    {
      if (NoStartMenuOption) 
	{
	  root_menu = 0;
	}
      else
	{
	  root_menu =
	    check_startmenu ("Cygwin Bash Shell",
			     backslash (cygpath ("/cygwin.bat")));
	}

      if (NoDesktopOption) 
	{
	  root_desktop = 0;
	}
      else
	{
	  root_desktop =
	    check_desktop ("Cygwin", backslash (cygpath ("/cygwin.bat")));
	}
    }

  load_dialog (GetHWND ());

  SetDlgItemFont(IDC_STATUS_HEADER, "MS Shell Dlg", 8, FW_BOLD);
}

long
DesktopSetupPage::OnBack ()
{
  HWND h = GetHWND ();
  save_dialog (h);
  return IDD_CHOOSE;
}

bool
DesktopSetupPage::OnFinish ()
{
  HWND h = GetHWND ();
  save_dialog (h);
  do_desktop_setup ();

  return true;
}

long 
DesktopSetupPage::OnUnattended ()
{
  Window::PostMessage (WM_APP_UNATTENDED_FINISH);
  // GetOwner ()->PressButton(PSBTN_FINISH);
  return -1;
}

bool
DesktopSetupPage::OnMessageApp (UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
    {
    case WM_APP_UNATTENDED_FINISH:
      {
	GetOwner ()->PressButton(PSBTN_FINISH);
	break;
      }
    default:
      {
	// Not handled
	return false;
      }
    }

  return true;
}
