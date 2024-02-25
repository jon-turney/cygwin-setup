/*
 * Copyright (c) 2000, 2001, Red Hat, Inc.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 */

/* Query user for auth information required */

#include "netio.h"
#include "GuiGetNetAuth.h"

#include "LogFile.h"

#include "resource.h"
#include "dialog.h"

static char **user, **passwd;
static int loading = 0;

static void
check_if_enable_ok (HWND h)
{
  int e = 0;
  if (*user)
    e = 1;
  EnableWindow (GetDlgItem (h, IDOK), e);
}

static void
load_dialog (HWND h)
{
  loading = 1;
  eset (h, IDC_NET_USER, *user);
  eset (h, IDC_NET_PASSWD, *passwd);
  check_if_enable_ok (h);
  loading = 0;
}

static void
save_dialog (HWND h)
{
  *user = eget (h, IDC_NET_USER, *user);
  *passwd = eget (h, IDC_NET_PASSWD, *passwd);
  if (! *passwd) {
    *passwd = new char[1];
    (*passwd)[0] = '\0';
  }
}

static BOOL
auth_cmd (HWND h, int id, HWND hwndctl, UINT code)
{
  switch (id)
    {

    case IDC_NET_USER:
    case IDC_NET_PASSWD:
      if (code == EN_CHANGE && !loading)
	{
	  save_dialog (h);
	  check_if_enable_ok (h);
	}
      break;

    case IDOK:
      save_dialog (h);
      EndDialog (h, 0);
      break;

    case IDCANCEL:
      EndDialog (h, 1);
      Logger ().exit (1);
      break;
    }
  return 0;
}

static INT_PTR CALLBACK
auth_proc (HWND h, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
    {
    case WM_INITDIALOG:
      load_dialog (h);
      return FALSE;
    case WM_COMMAND:
      auth_cmd (h, LOWORD(wParam), (HWND)lParam, HIWORD(wParam));
      return 0;
    }
  return FALSE;
}

static int
auth_common (int id, HWND owner)
{
  return DialogBox (NULL, MAKEINTRESOURCE (id), owner, auth_proc);
}

int
GuiGetNetAuth::get_auth ()
{
  user = &NetIO::net_user;
  passwd = &NetIO::net_passwd;
  return auth_common (IDD_NET_AUTH, owner);
}

int
GuiGetNetAuth::get_proxy_auth ()
{
  user = &NetIO::net_proxy_user;
  passwd = &NetIO::net_proxy_passwd;
  return auth_common (IDD_PROXY_AUTH, owner);
}

int
GuiGetNetAuth::get_ftp_auth ()
{
  if (NetIO::net_ftp_user)
    {
      delete[] NetIO::net_ftp_user;
      NetIO::net_ftp_user = NULL;
    }
  if (NetIO::net_ftp_passwd)
    {
      delete[] NetIO::net_ftp_passwd;
      NetIO::net_ftp_passwd = NULL;
    }
  user = &NetIO::net_ftp_user;
  passwd = &NetIO::net_ftp_passwd;
  return auth_common (IDD_FTP_AUTH, owner);
}
