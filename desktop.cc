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

#include "win32.h"
#include <shlobj.h>
#include "desktop.h"
#include "propsheet.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "ini.h"
#include "resource.h"
#include "state.h"
#include "dialog.h"
#include "mount.h"
#include "mklink2.h"
#include "package_db.h"
#include "package_meta.h"
#include "filemanip.h"
#include "io_stream.h"
#include "getopt++/BoolOption.h"
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
           const std::string& target,
           const std::string& arg,
	   const std::string& icon)
{
  std::string fname = linkpath + "/" + title + ".lnk";

  if (_access (fname.c_str(), 0) == 0)
    return;			/* already exists */

  LogBabblePrintf ("make_link %s, %s, %s\n",
       fname.c_str(), title.c_str(), target.c_str());

  io_stream::mkpath_p (PATH_TO_FILE, std::string ("file://") + fname, 0);

  std::string exepath;
  std::string argbuf;

  exepath = target;
  argbuf = arg;

  LogBabblePrintf ("make_link_2 (%s, %s, %s, %s)",
       exepath.c_str(), argbuf.c_str(),
       icon.c_str(), fname.c_str());
  make_link_2 (exepath.c_str(), argbuf.c_str(),
	       icon.c_str(), fname.c_str());
}

static void
start_menu (const std::string& title, const std::string& target,
	    const std::string& arg, const std::string& iconpath)
{
  /* Special folders always < MAX_PATH. */
  char path[MAX_PATH];
  LPITEMIDLIST id;
  int issystem = (root_scope == IDC_ROOT_SYSTEM) ? 1 : 0;
  SHGetSpecialFolderLocation (NULL,
			      issystem ? CSIDL_COMMON_PROGRAMS :
			      CSIDL_PROGRAMS, &id);
  SHGetPathFromIDList (id, path);
  strncat (path, "/Cygwin", MAX_PATH);
  LogBabblePrintf ("Program directory for program link: %s", path);
  make_link (path, title, target, arg, iconpath);
}

static void
desktop_icon (const std::string& title, const std::string& target,
	      const std::string& arg, const std::string& iconpath)
{
  /* Special folders always < MAX_PATH. */
  char path[MAX_PATH];
  LPITEMIDLIST id;
  int issystem = (root_scope == IDC_ROOT_SYSTEM) ? 1 : 0;
  SHGetSpecialFolderLocation (NULL,
			      issystem ? CSIDL_COMMON_DESKTOPDIRECTORY :
			      CSIDL_DESKTOPDIRECTORY, &id);
  SHGetPathFromIDList (id, path);
  LogBabblePrintf ("Desktop directory for desktop link: %s", path);
  make_link (path, title, target, arg, iconpath);
}

static void
make_cygwin_bat ()
{
  batname = backslash (cygpath ("/Cygwin.bat"));
  FILE *bat;

  size_t len = batname.size () + 7;
  WCHAR wname[len];
  mklongpath (wname, batname.c_str (), len);

  /* if the batch file exists, don't overwrite it */
  if (GetFileAttributesW (wname) != INVALID_FILE_ATTRIBUTES)
    return;

  bat = nt_wfopen (wname, "wt", 0755);

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
save_icon (std::string &iconpath, const char *resource_name)
{
  HRSRC rsrc = FindResource (NULL, resource_name, "FILE");
  if (rsrc == NULL)
    {
      fatal ("FindResource failed");
    }
  HGLOBAL res = LoadResource (NULL, rsrc);
  char *data = (char *) LockResource (res);
  int len = SizeofResource (NULL, rsrc);

  FILE *f;
  WIN32_FILE_ATTRIBUTE_DATA attr;

  size_t ilen = iconpath.size () + 7;
  WCHAR wname[ilen];
  mklongpath (wname, iconpath.c_str (), ilen);
  if (GetFileAttributesExW (wname, GetFileExInfoStandard, &attr)
      && attr.dwFileAttributes != INVALID_FILE_ATTRIBUTES
      && attr.nFileSizeLow > 7022)	/* Size of old icon */
    return;

  f = nt_wfopen (wname, "wb", 0644);
  if (f)
    {
      fwrite (data, 1, len, f);
      fclose (f);
    }
}

#define TARGET		"/bin/mintty"
#define DEFAULTICON	"/Cygwin.ico"
#define TERMINALICON	"/Cygwin-Terminal.ico"
#define TERMINALTITLE	(is_64bit ? "Cygwin64 Terminal" \
				  : "Cygwin Terminal")
#define STARTMENUDIR	"/Cygwin"

static void
do_desktop_setup ()
{
  std::string target = backslash (cygpath (TARGET));
  std::string defaulticon = backslash (cygpath (DEFAULTICON));
  std::string terminalicon = backslash (cygpath (TERMINALICON));

  save_icon (defaulticon, "CYGWIN.ICON");
  save_icon (terminalicon, "CYGWIN-TERMINAL.ICON");

  make_cygwin_bat ();

  if (root_menu)
    start_menu (TERMINALTITLE, target, "-i " TERMINALICON " -", terminalicon);

  if (root_desktop)
    desktop_icon (TERMINALTITLE, target, "-i " TERMINALICON " -", terminalicon);
}

static int da[] = { IDC_ROOT_DESKTOP, 0 };
static int ma[] = { IDC_ROOT_MENU, 0 };

static void
check_if_enable_next (HWND h)
{
  EnableWindow (GetDlgItem (h, IDOK), 1);
}

static void
set_status (HWND h)
{
  char buf[1000], fmt[1000];
  if (LoadString (hinstance, Logger ().getExitMsg (), fmt, sizeof (fmt)) > 0)
    {
      snprintf (buf, 1000, fmt,
      		backslash (Logger ().getFileName (LOG_BABBLE)).c_str ());
      eset (h, IDC_STATUS, buf);
    }
}

static char *header_string = NULL;
static char *message_string = NULL;
static void
load_dialog (HWND h)
{
  if (source == IDC_SOURCE_DOWNLOAD)
    {
      // Don't need the checkboxes
      EnableWindow (GetDlgItem (h, IDC_ROOT_DESKTOP), FALSE);
      EnableWindow (GetDlgItem (h, IDC_ROOT_MENU), FALSE);
      if (header_string == NULL)
        header_string = eget (h, IDC_STATIC_HEADER_TITLE, header_string);
      if (message_string == NULL) 
        message_string = eget (h, IDC_STATIC_HEADER, message_string);
      eset (h, IDC_STATIC_HEADER_TITLE, "Installation complete");
      eset (h, IDC_STATIC_HEADER, "Shows installation status in download-only mode.");
    }
  else
    {
      EnableWindow (GetDlgItem (h, IDC_ROOT_DESKTOP), TRUE);
      EnableWindow (GetDlgItem (h, IDC_ROOT_MENU), TRUE);
      if (header_string != NULL)
        eset (h, IDC_STATIC_HEADER_TITLE, header_string);
      if (message_string != NULL)
        eset (h, IDC_STATIC_HEADER, message_string);
      rbset (h, da, root_desktop);
      rbset (h, ma, root_menu);
    }
  check_if_enable_next (h);
  set_status (h);
}

static int
check_desktop (const std::string title, const std::string target)
{
  /* Special folders always < MAX_PATH. */
  char path[MAX_PATH];
  LPITEMIDLIST id;
  int issystem = (root_scope == IDC_ROOT_SYSTEM) ? 1 : 0;
  SHGetSpecialFolderLocation (NULL,
			      issystem ? CSIDL_COMMON_DESKTOPDIRECTORY :
			      CSIDL_DESKTOPDIRECTORY, &id);
  SHGetPathFromIDList (id, path);
  LogBabblePrintf ("Desktop directory for desktop link: %s", path);
  std::string fname = std::string(path) + "/" + title + ".lnk";

  if (_access (fname.c_str(), 0) == 0)
    return 0;			/* already exists */

  return IDC_ROOT_DESKTOP;
}

static int
check_startmenu (const std::string title, const std::string target)
{
  /* Special folders always < MAX_PATH. */
  char path[MAX_PATH];
  LPITEMIDLIST id;
  int issystem = (root_scope == IDC_ROOT_SYSTEM) ? 1 : 0;
  SHGetSpecialFolderLocation (NULL,
			      issystem ? CSIDL_COMMON_PROGRAMS :
			      CSIDL_PROGRAMS, &id);
  SHGetPathFromIDList (id, path);
  LogBabblePrintf ("Program directory for program link: %s", path);
  strcat (path, STARTMENUDIR);
  std::string fname = std::string(path) + "/" + title + ".lnk";

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
  SetDlgItemFont(IDC_STATUS_HEADER, "MS Shell Dlg", 8, FW_BOLD);
}

void
DesktopSetupPage::OnActivate ()
{
  if (NoShortcutsOption || source == IDC_SOURCE_DOWNLOAD) 
    {
      root_desktop = root_menu = 0;
    }
  else
    {
      std::string target = backslash (cygpath (TARGET));

      if (NoStartMenuOption) 
	{
	  root_menu = 0;
	}
      else
	{
	  root_menu = check_startmenu (TERMINALTITLE, target);
	}

      if (NoDesktopOption) 
	{
	  root_desktop = 0;
	}
      else
	{
	  root_desktop = check_desktop (TERMINALTITLE, target);
	}
    }

  load_dialog (GetHWND ());
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
  if (source != IDC_SOURCE_DOWNLOAD)
    do_desktop_setup ();

  return true;
}

long 
DesktopSetupPage::OnUnattended ()
{
  Window::PostMessageNow (WM_APP_UNATTENDED_FINISH);
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
