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

/* The purpose of this file is to ask the user where they want the
   root of the installation to be, and to ask whether the user prefers
   text or binary mounts. */

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
#include "log.h"
#include "root.h"

#include "getopt++/StringOption.h"

using namespace std;

StringOption RootOption ("", 'R', "root", "Root installation directory", false);

static int rb[] = { IDC_ROOT_TEXT, IDC_ROOT_BINARY, 0 };
static int su[] = { IDC_ROOT_SYSTEM, IDC_ROOT_USER, 0 };

static void
check_if_enable_next (HWND h)
{
  EnableWindow (GetDlgItem (h, IDOK), root_text && get_root_dir ().size()
		&& root_scope);
}

static void
load_dialog (HWND h)
{
  rbset (h, rb, root_text);
  rbset (h, su, root_scope);
  eset (h, IDC_ROOT_DIR, get_root_dir ());
  check_if_enable_next (h);
}

static void
save_dialog (HWND h)
{
  root_text = rbget (h, rb);
  root_scope = rbget (h, su);
  set_root_dir (egetString (h, IDC_ROOT_DIR));
}

static int CALLBACK
browse_cb (HWND h, UINT msg, LPARAM lp, LPARAM data)
{
  switch (msg)
    {
    case BFFM_INITIALIZED:
      if (get_root_dir ().size())
	SendMessage (h, BFFM_SETSELECTION, TRUE, (LPARAM) get_root_dir ().cstr_oneuse());
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
  bi.lpszTitle = "Select an installation root directory";
  bi.ulFlags = BIF_RETURNONLYFSDIRS;
  bi.lpfn = browse_cb;
  pidl = SHBrowseForFolder (&bi);
  if (pidl)
    {
      if (SHGetPathFromIDList (pidl, name))
	eset (h, IDC_ROOT_DIR, name);
    }
}

#define isslash(c) ((c) == '\\' || (c) == '/')

static int
directory_is_absolute ()
{
  
  const char *r = get_root_dir ().cstr_oneuse();
  if (isalpha (r[0]) && r[1] == ':' && (r[2] == '\\' || r[2] == '/'))
    {
      return 1;
    }
  return 0;
}

static int
directory_is_rootdir ()
{
  
  for (const char *c = get_root_dir().cstr_oneuse(); *c; c++)
    if (isslash (c[0]) && c[1] && !isslash (c[1]))
      return 0;
  return 1;
}

static int
directory_has_spaces ()
{
  if (get_root_dir ().find(' '))
    return 1;
  return 0;
}

static BOOL
dialog_cmd (HWND h, int id, HWND hwndctl, UINT code)
{
  switch (id)
    {

    case IDC_ROOT_DIR:
    case IDC_ROOT_TEXT:
    case IDC_ROOT_BINARY:
    case IDC_ROOT_SYSTEM:
    case IDC_ROOT_USER:
      save_dialog (h);
      check_if_enable_next (h);
      break;

    case IDC_ROOT_BROWSE:
      browse (h);
      break;
    }
  return 0;
}

bool
RootPage::Create ()
{
  return PropertyPage::Create (NULL, dialog_cmd, IDD_ROOT);
}

void
RootPage::OnInit ()
{
  if (((string)RootOption).size()) 
    set_root_dir((string)RootOption);
  if (!get_root_dir ().size())
    read_mounts ();
  load_dialog (GetHWND ());
}

bool
RootPage::wantsActivation() const
{
  return (source != IDC_SOURCE_DOWNLOAD);
}

long
RootPage::OnNext ()
{
  HWND h = GetHWND ();

  save_dialog (h);

  if (!directory_is_absolute ())
    {
      note (h, IDS_ROOT_ABSOLUTE);
      return -1;
    }
  else if (directory_is_rootdir () && (IDNO == yesno (h, IDS_ROOT_SLASH)))
    return -1;
  else if (directory_has_spaces () && (IDNO == yesno (h, IDS_ROOT_SPACE)))
    return -1;

  log (LOG_PLAIN, String ("root: ") + get_root_dir () + 
       (root_text == IDC_ROOT_TEXT ? " text" : " binary")  +
       (root_scope == IDC_ROOT_USER ? " user" : " system"));

  return 0;
}

long
RootPage::OnBack ()
{
  HWND h = GetHWND ();

  save_dialog (h);
  return 0;
}

long
RootPage::OnUnattended ()
{
  return OnNext();
}
