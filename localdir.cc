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
#include "concat.h"
#include "log.h"
#include "mkdir.h"
#include "io_stream.h"

#include "localdir.h"

#include "threebar.h"
extern ThreeBarProgressPage Progress;

void
save_local_dir ()
{
  mkdir_p (1, local_dir);

  io_stream *f;
  if (get_root_dir ())
    {
      f = io_stream::open ("cygfile:///etc/setup/last-cache", "wb");
      io_stream::remove ("file://last-cache");
    }
  else
    f = io_stream::open ("file://last-cache", "wb");
  if (f)
    {
      char temp[1000];
      sprintf (temp, "%s", local_dir);
      f->write (temp, strlen (temp));
      delete f;
    }
}

static void
check_if_enable_next (HWND h)
{
  EnableWindow (GetDlgItem (h, IDOK), local_dir != 0);
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
  local_dir = eget (h, IDC_LOCAL_DIR, local_dir);
}


static int CALLBACK
browse_cb (HWND h, UINT msg, LPARAM lp, LPARAM data)
{
  switch (msg)
    {
    case BFFM_INITIALIZED:
      if (local_dir)
	SendMessage (h, BFFM_SETSELECTION, TRUE, (LPARAM) local_dir);
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
LocalDirPage::OnInit ()
{
  static int inited = 0;
  if (!inited)
    {
      io_stream *f =
	io_stream::open ("cygfile:///etc/setup/last-cache", "rt");
      if (!f)
	f = io_stream::open ("file://last-cache", "rt");
      if (f)
	{
	  char localdir[1000];
	  char *fg_ret = f->gets (localdir, 1000);
	  delete f;
	  if (fg_ret)
	    {
	      delete [] local_dir;
	      local_dir = new char [strlen(localdir) +1];
	      local_dir[strlen(localdir)] = '\0';
	      strcpy (local_dir, localdir);
	    }
	}
      inited = 1;
    }
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
  save_local_dir ();
  log (0, "Selected local directory: %s", local_dir);
  if (SetCurrentDirectoryA (local_dir))
    {
      if (source == IDC_SOURCE_CWD)
	{
	  do_fromcwd (GetInstance (), GetHWND ());
	  if (next_dialog == IDD_S_LOAD_INI)
	    {
	      Progress.SetActivateTask (WM_APP_START_SETUP_INI_DOWNLOAD);
	      return IDD_INSTATUS;
	    }
	  return next_dialog;
	}
    }
  else
    note (h, IDS_ERR_CHDIR, local_dir);

  return 0;
}

long
LocalDirPage::OnBack ()
{
  save_dialog (GetHWND ());
  if (source == IDC_SOURCE_DOWNLOAD)
    {
      // Downloading only, skip the unix root page
      return IDD_SOURCE;
    }
  return 0;
}
