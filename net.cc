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

/* The purpose of this file is to get the network configuration
   information from the user. */

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include "win32.h"
#include <stdio.h>
#include <stdlib.h>
#include "dialog.h"
#include "resource.h"
#include "state.h"
#include "msg.h"
#include "log.h"

#include "net.h"

#include "threebar.h"
extern ThreeBarProgressPage Progress;

static int rb[] = { IDC_NET_IE5, IDC_NET_DIRECT, IDC_NET_PROXY, 0 };

static void
check_if_enable_next (HWND h)
{
  int e = 0, p = 0, pu = 0;
  if (net_method == IDC_NET_IE5)
    pu = 1;
  if (net_method == IDC_NET_IE5 || net_method == IDC_NET_DIRECT)
    e = 1;
  else if (net_method == IDC_NET_PROXY)
    {
      p = pu = 1;
      if (net_proxy_host && net_proxy_port)
	e = 1;
    }
  EnableWindow (GetDlgItem (h, IDOK), e);
  EnableWindow (GetDlgItem (h, IDC_PROXY_HOST), p);
  EnableWindow (GetDlgItem (h, IDC_PROXY_PORT), p);
}

static void
load_dialog (HWND h)
{
  rbset (h, rb, net_method);
  eset (h, IDC_PROXY_HOST, net_proxy_host);
  if (net_proxy_port == 0)
    net_proxy_port = 80;
  eset (h, IDC_PROXY_PORT, net_proxy_port);
  check_if_enable_next (h);
}

static void
save_dialog (HWND h)
{
  net_method = rbget (h, rb);
  net_proxy_host = eget (h, IDC_PROXY_HOST, net_proxy_host);
  net_proxy_port = eget (h, IDC_PROXY_PORT);
}

static BOOL
dialog_cmd (HWND h, int id, HWND hwndctl, UINT code)
{
  switch (id)
    {

    case IDC_NET_IE5:
    case IDC_NET_DIRECT:
    case IDC_NET_PROXY:
    case IDC_PROXY_HOST:
    case IDC_PROXY_PORT:
      save_dialog (h);
      check_if_enable_next (h);
      break;
    }
  return 0;
}

bool
NetPage::Create ()
{
  return PropertyPage::Create (NULL, dialog_cmd, IDD_NET);
}

void
NetPage::OnInit ()
{
  HWND h = GetHWND ();

  net_method = IDC_NET_DIRECT;
  load_dialog (h);

  // Check to see if any radio buttons are selected. If not, select a default.
  if ((!SendMessage (GetDlgItem (IDC_NET_IE5), BM_GETCHECK, 0, 0) ==
       BST_CHECKED)
      && (!SendMessage (GetDlgItem (IDC_NET_PROXY), BM_GETCHECK, 0, 0)
	  == BST_CHECKED))
    {
      SendMessage (GetDlgItem (IDC_NET_DIRECT), BM_CLICK, 0, 0);
    }
}

long
NetPage::OnNext ()
{
  save_dialog (GetHWND ());

  log (0, "net: %s",
       (net_method == IDC_NET_IE5) ? "IE5" :
       (net_method == IDC_NET_DIRECT) ? "Direct" : "Proxy");

  Progress.SetActivateTask (WM_APP_START_SITE_INFO_DOWNLOAD);
  return IDD_INSTATUS;
}

long
NetPage::OnBack ()
{
  save_dialog (GetHWND ());
  return 0;
}
