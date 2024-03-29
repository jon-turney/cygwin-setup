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
 * Written by Andrej Borsenkow <Andrej.Borsenkow@mow.siemens.ru>
 * based on work and suggestions of DJ Delorie
 *
 */

/* The purpose of this file is to ask the user where they want the
 * locally downloaded package files to be cached
 */

#include "localdir.h"

#include "LogSingleton.h"
#include "LogFile.h"
#include "win32.h"
#include "filemanip.h"

#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "ini.h"
#include "dialog.h"
#include "resource.h"
#include "state.h"
#include "msg.h"
#include "mount.h"
#include "io_stream.h"
#include "mkdir.h"

#include "UserSettings.h"

#include "getopt++/StringOption.h"

#include "threebar.h"
extern ThreeBarProgressPage Progress;

static StringOption LocalDirOption ("", 'l', "local-package-dir", IDS_HELPTEXT_LOCAL_PACKAGE_DIR, false);

static ControlAdjuster::ControlInfo LocaldirControlsInfo[] = {
  { IDC_LOCALDIR_GRP,       CP_STRETCH,   CP_TOP },
  { IDC_LOCAL_DIR,          CP_STRETCH,   CP_TOP },
  { IDC_LOCAL_DIR_BROWSE,   CP_RIGHT,     CP_TOP },
  {0, CP_LEFT, CP_TOP}
};

LocalDirSetting::LocalDirSetting ()
{
  const char *fg_ret;
  if (std::string (LocalDirOption).size ())
    local_dir = std::string (LocalDirOption);
  else if ((fg_ret = UserSettings::instance().get ("last-cache")))
    local_dir = std::string (fg_ret);
}

void
LocalDirSetting::save ()
{
  UserSettings::instance().set ("last-cache", local_dir);
  if (source == IDC_SOURCE_DOWNLOAD || !get_root_dir ().size())
    {
      const char *sep = isdirsep (local_dir[local_dir.size () - 1]) ? "" : "\\";
      Logger ().clearFiles();
      Logger ().setFile (LOG_BABBLE, local_dir + sep + "setup.log.full", false);
      Logger ().setFile (0, local_dir + sep + "setup.log", true);
    }
  else
    {
      Logger ().clearFiles();
      mkdir_p (1, cygpath ("/var/log").c_str (), 01777);
      Logger ().setFile (LOG_BABBLE, cygpath ("/var/log/setup.log.full"),
      			 false);
      Logger ().setFile (0, cygpath ("/var/log/setup.log"), true);
    }
}

static void
check_if_enable_next (HWND h)
{
  EnableWindow (GetDlgItem (h, IDOK), local_dir.size() != 0);
}

static void
load_dialog (HWND h)
{
  if (source != IDC_SOURCE_LOCALDIR)
    {
      ShowWindow (GetDlgItem(h, IDC_LOCAL_DIR_DOWNLOAD_DESC), SW_SHOW);
      ShowWindow (GetDlgItem(h, IDC_LOCAL_DIR_INSTALL_DESC), SW_HIDE);
    }
  else
    {
      ShowWindow (GetDlgItem(h, IDC_LOCAL_DIR_DOWNLOAD_DESC), SW_HIDE);
      ShowWindow (GetDlgItem(h, IDC_LOCAL_DIR_INSTALL_DESC), SW_SHOW);
    }
  eset (h, IDC_LOCAL_DIR, local_dir);
  check_if_enable_next (h);
}

static void
save_dialog (HWND h)
{
  local_dir = egetString (h, IDC_LOCAL_DIR);
}

// returns non-zero if refused or error, 0 if accepted and created ok.
static int
offer_to_create (HWND h, const char *dirname)
{
  if (!dirname || !*dirname)
    return -1;

  if (!unattended_mode)
    {
      DWORD ret;
      ret = mbox (h, IDS_MAYBE_MKDIR, MB_ICONSTOP | MB_YESNO, dirname);
      if (ret == IDNO)
	return -1;
    }

  int rv = mkdir_p (true, dirname, 0);

  if (rv)
    note (h, IDS_CANT_MKDIR, dirname);

  return rv;
}

static int CALLBACK
browse_cb (HWND h, UINT msg, LPARAM lp, LPARAM data)
{
  /* SHGetPathFromIDList doesn't handle path length > MAX_PATH. */
  static CHAR dirname[MAX_PATH];
  switch (msg)
    {
    case BFFM_INITIALIZED:
      if (local_dir.size())
	SendMessage (h, BFFM_SETSELECTION, TRUE, (LPARAM) local_dir.c_str());
      break;
    case BFFM_SELCHANGED:
      {
        // Make a note of each new dir we successfully select, so that
        // we know where to create the new directory if an invalid name
        // is entered in the text box.
        LPITEMIDLIST pidl = reinterpret_cast<LPITEMIDLIST>(lp);
        SHGetPathFromIDList (pidl, dirname);
        break;
      }
    case BFFM_VALIDATEFAILED:
      // See if user wants to create a dir in the last successfully-selected.
      CHAR tempname[MAX_PATH];
      snprintf (tempname, sizeof tempname, "%s\\%s", dirname, reinterpret_cast<LPTSTR>(lp));
      if (!offer_to_create (h, tempname))
	{
	  SendMessage (h, BFFM_SETSELECTION, TRUE, reinterpret_cast<LPARAM>(tempname));
	  return -1;
	}
      // Reset to original directory instead.
      SendMessage (h, BFFM_SETSELECTION, TRUE, reinterpret_cast<LPARAM>(dirname));
      return -1;
    }
  return 0;
}

static void
browse (HWND h)
{
  std::wstring title = LoadStringW((source != IDC_SOURCE_LOCALDIR) ?
                                   IDS_LOCALDIR_BROWSE_DOWNLOAD_TITLE :
                                   IDS_LOCALDIR_BROWSE_PACKAGE_TITLE);

  wchar_t wname[MAX_PATH];
  BROWSEINFOW bi;
  memset (&bi, 0, sizeof (bi));
  bi.hwndOwner = h;
  bi.pszDisplayName = wname;
  bi.lpszTitle = title.c_str();
  bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE
	      | ((source != IDC_SOURCE_LOCALDIR) ? (BIF_EDITBOX | BIF_VALIDATE)
						 : 0);
  bi.lpfn = browse_cb;

  /* SHGetPathFromIDList doesn't handle path length > MAX_PATH. */
  LPITEMIDLIST pidl;
  pidl = SHBrowseForFolderW (&bi);
  if (pidl)
    {
      CHAR name[MAX_PATH];
      if (SHGetPathFromIDList (pidl, name))
	eset (h, IDC_LOCAL_DIR, name);
    }
}

static BOOL
dialog_cmd (HWND h, int id, HWND hwndctl, UINT code)
{
  switch (id)
    {

    case IDC_LOCAL_DIR:
      save_dialog (h);
      check_if_enable_next (h);
      break;

    case IDC_LOCAL_DIR_BROWSE:
      browse (h);
      break;
    }
  return 0;
}

LocalDirPage::LocalDirPage ()
{
  sizeProcessor.AddControlInfo (LocaldirControlsInfo);
}

bool
LocalDirPage::Create ()
{
  return PropertyPage::Create (NULL, dialog_cmd, IDD_LOCAL_DIR);
}

void
LocalDirPage::OnActivate ()
{
  load_dialog (GetHWND ());
}

long
LocalDirPage::OnNext ()
{
  HWND h = GetHWND ();

  save_dialog (h);
  while (local_dir[local_dir.size() - 1] == '\\' || local_dir[local_dir.size() - 1] == '/')
    local_dir.erase (local_dir.size() - 1, 1);
  LocalDirSetting::save ();
  Log (LOG_PLAIN) << "Selected local directory: " << local_dir << endLog;
  
  bool tryLocalDir = true;
  while (tryLocalDir)
    {
      tryLocalDir = false;
      /* FIXME: As for almost any other string, we should store local_dir
         as UNICODE string to avoid the whole conversion mess.
	 We call GetFileAttributesW because our own version implies
	 FILE_OPEN_FOR_BACKUP_INTENT.  We don't try to change CWD for
	 the same reason. */
      size_t len = local_dir.size () + 7;
      WCHAR wlocal[len];
      mklongpath (wlocal, local_dir.c_str (), len);
      DWORD attr = GetFileAttributesW (wlocal);
      if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
	{
	  if (source == IDC_SOURCE_LOCALDIR)
	    {
	      do_from_local_dir (GetInstance (), GetHWND (), local_dir);
	      Progress.SetActivateTask (WM_APP_START_SETUP_INI_DOWNLOAD);
	      return IDD_INSTATUS;
	    }
	}
      else if (attr == INVALID_FILE_ATTRIBUTES
	       && (GetLastError () == ERROR_FILE_NOT_FOUND
		   || GetLastError () == ERROR_PATH_NOT_FOUND))
	{
	  if (source == IDC_SOURCE_LOCALDIR)
	   {
	   if (!unattended_mode)
	    {
	      // Check the user really wants only to uninstall.
	      int ret = mbox(h, IDS_NO_LOCALDIR,
                             MB_ICONEXCLAMATION | MB_OKCANCEL,
                             local_dir.c_str ());
	      if (ret == IDCANCEL)
                return -1;
	    }
	   Progress.SetActivateTask (WM_APP_START_SETUP_INI_DOWNLOAD);
	   return IDD_INSTATUS;
	   }
	  else if (offer_to_create (GetHWND (), local_dir.c_str ()))
	    return -1;
	  tryLocalDir = true;
	}
      else
	{
	  DWORD err = GetLastError ();
          char* buf;
          char msg[1000];
	  if (FormatMessage (FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
                             0, err, LANG_NEUTRAL, (LPSTR)&buf, 0, 0) != 0)
	    {
              snprintf (msg, sizeof (msg), "%s", buf);
	      LocalFree (buf);
            }
          else
            snprintf (msg, sizeof (msg), "%s", "(unknown error)");
	  int ret = mbox (h, IDS_ERR_CHDIR,
                          MB_ICONEXCLAMATION | MB_ABORTRETRYIGNORE,
                          local_dir.c_str(), msg, err);
	  if (ret == IDABORT)
	    return -1;
	  else
	    tryLocalDir = (ret == IDRETRY);
	}
    }

  if (source == IDC_SOURCE_LOCALDIR)
    return IDD_INSTATUS;

  return 0; // IDD_NET
}

long
LocalDirPage::OnBack ()
{
  save_dialog (GetHWND ());
  return 0;
}
