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

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include "win32.h"
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "dialog.h"
#include "resource.h"
#include "state.h"
#include "msg.h"
#include "mount.h"
#include "LogFile.h"
#include "log.h"
#include "io_stream.h"

#include "localdir.h"
#include "UserSettings.h"

#include "getopt++/StringOption.h"

#include "threebar.h"
extern ThreeBarProgressPage Progress;
extern LogFile * theLog;

static StringOption LocalDirOption ("", 'l', "local-package-dir", "Local package directory", false);

static LocalDirSetting localDir;

void 
LocalDirSetting::load(){
  static int inited = 0;
  if (inited)
    return;
  io_stream *f = UserSettings::Instance().settingFileForLoad("last-cache");
  if (f)
    {
      char localdir[1000];
      char *fg_ret = f->gets (localdir, 1000);
      delete f;
      if (fg_ret)
        local_dir = String (localdir);
    }
  if (((std::string)LocalDirOption).size())
    local_dir = ((std::string)LocalDirOption);
  inited = 1;
}

void 
LocalDirSetting::save()
{
  io_stream *f = UserSettings::Instance().settingFileForSave("last-cache");
  if (f)
    {
      f->write (local_dir.cstr_oneuse(), local_dir.size());
      delete f;
    }
  if (source == IDC_SOURCE_DOWNLOAD || !get_root_dir ().size())
    {
      theLog->clearFiles();
      theLog->setFile (LOG_BABBLE, local_dir + "/setup.log.full", false);
      theLog->setFile (0, local_dir + "/setup.log", true);
    }
  else
    {
      theLog->clearFiles();
      theLog->setFile (LOG_BABBLE, cygpath ("/var/log/setup.log.full"), false);
      theLog->setFile (0, cygpath ("/var/log/setup.log"), true);
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
  eset (h, IDC_LOCAL_DIR, local_dir);
  check_if_enable_next (h);
}

static void
save_dialog (HWND h)
{
  local_dir = egetString (h, IDC_LOCAL_DIR);
}


static int CALLBACK
browse_cb (HWND h, UINT msg, LPARAM lp, LPARAM data)
{
  switch (msg)
    {
    case BFFM_INITIALIZED:
      if (local_dir.size())
	SendMessage (h, BFFM_SETSELECTION, TRUE, (LPARAM) local_dir.cstr_oneuse());
      break;
    }
  return 0;
}

static void
browse (HWND h)
{
  BROWSEINFO bi;
  CHAR name[MAX_PATH];
  LPITEMIDLIST pidl;
  memset (&bi, 0, sizeof (bi));
  bi.hwndOwner = h;
  bi.pszDisplayName = name;
  bi.lpszTitle = "Select download directory";
  bi.ulFlags = BIF_RETURNONLYFSDIRS;
  bi.lpfn = browse_cb;
  pidl = SHBrowseForFolder (&bi);
  if (pidl)
    {
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
  localDir.save ();
  log (LOG_PLAIN, String ("Selected local directory: ") + local_dir);
  if (SetCurrentDirectoryA (local_dir.cstr_oneuse()))
    {
      if (source == IDC_SOURCE_CWD)
	{
	  if (do_fromcwd (GetInstance (), GetHWND ()))
	    {
	      Progress.SetActivateTask (WM_APP_START_SETUP_INI_DOWNLOAD);
	      return IDD_INSTATUS;
	    }
	  return IDD_CHOOSE;
	}
    }
  else
    note (h, IDS_ERR_CHDIR, local_dir.cstr_oneuse());

  return 0;
}

long
LocalDirPage::OnBack ()
{
  save_dialog (GetHWND ());
  return 0;
}
