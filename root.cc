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

static int rb[] = { IDC_ROOT_TEXT, IDC_ROOT_BINARY, 0 };
static int su[] = { IDC_ROOT_SYSTEM, IDC_ROOT_USER, 0 };

static void
check_if_enable_next (HWND h)
{
  EnableWindow (GetDlgItem (h, IDOK), root_text && root_dir && root_scope);
}

static void
load_dialog (HWND h)
{
  rbset (h, rb, root_text);
  rbset (h, su, root_scope);
  eset (h, IDC_ROOT_DIR, root_dir);
  check_if_enable_next (h);
}

static void
save_dialog (HWND h)
{
  root_text = rbget (h, rb);
  root_scope = rbget (h, su);
  root_dir = eget (h, IDC_ROOT_DIR, root_dir);
}

static void
read_mount_table ()
{
  int istext;
  int issystem;
  root_dir = find_root_mount (&istext, &issystem);
  if (root_dir)
    {
      if (istext)
	root_text = IDC_ROOT_TEXT;
      else
	root_text = IDC_ROOT_BINARY;
      if (issystem)
	root_scope = IDC_ROOT_SYSTEM;
      else
	root_scope = IDC_ROOT_USER;
    }
  else
    {
      char windir[_MAX_PATH];
      GetWindowsDirectory (windir, sizeof (windir));
      windir[2] = 0;
      root_dir = concat (windir, "\\cygwin", 0);
      root_text = IDC_ROOT_BINARY;
      root_scope = IDC_ROOT_USER;
    }
}

static int CALLBACK
browse_cb (HWND h, UINT msg, LPARAM lp, LPARAM data)
{
  switch (msg)
    {
    case BFFM_INITIALIZED:
      if (root_dir)
	SendMessage (h, BFFM_SETSELECTION, TRUE, (LPARAM)root_dir);
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
  if (isalpha (root_dir[0])
      && root_dir[1] == ':'
      && (root_dir[2] == '\\' || root_dir[2] == '/'))
    return 1;
  return 0;
}

static int
directory_is_rootdir ()
{
  char *c;
  for (c = root_dir; *c; c++)
    if (isslash(c[0]) && c[1] && !isslash(c[1]))
      return 0;
  return 1;
}

static int
directory_has_spaces ()
{
  if (strchr (root_dir, ' '))
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

    case IDOK:
      save_dialog(h);

      if (! directory_is_absolute ())
	{
	  note (IDS_ROOT_ABSOLUTE);
	  break;
	}

      if (directory_is_rootdir ())
	if (IDNO == yesno (IDS_ROOT_SLASH))
	  break;

      if (directory_has_spaces ())
	if (IDNO == yesno (IDS_ROOT_SPACE))
	  break;

      switch (source)
	{
	case IDC_SOURCE_NETINST:
	  NEXT(IDD_NET);
	  break;
	case IDC_SOURCE_CWD:
	  NEXT(IDD_S_FROM_CWD);
	  break;
	default:
	  msg("source is default? %d\n", source);
	  NEXT(0);
	}
      break;

    case IDC_BACK:
      save_dialog(h);
      NEXT(IDD_SOURCE);
      break;

    case IDCANCEL:
      NEXT(0);
      break;
    }
}

static BOOL CALLBACK
dialog_proc (HWND h, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
    {
    case WM_INITDIALOG:
      load_dialog(h);
      return FALSE;
    case WM_COMMAND:
      return HANDLE_WM_COMMAND(h, wParam, lParam, dialog_cmd);
    }
  return FALSE;
}

void
do_root (HINSTANCE h)
{
  int rv = 0;
  if (!root_dir)
    read_mount_table();
  rv = DialogBox (h, MAKEINTRESOURCE (IDD_ROOT), 0, dialog_proc);
  if (rv == -1)
    fatal (IDS_DIALOG_FAILED);
}

