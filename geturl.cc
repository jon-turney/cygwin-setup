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
 * Written by DJ Delorie <dj@cygnus.com>
 *
 */

/* The purpose of this file is to act as a pretty interface to
   netio.cc.  We add a progress dialog and some convenience functions
   (like collect to string or file */

static char *cvsid = "\n%%% $Id$\n";

#include "win32.h"
#include "commctrl.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "dialog.h"
#include "geturl.h"
#include "resource.h"
#include "netio.h"
#include "msg.h"
#include "log.h"
#include "state.h"
#include "diskfull.h"
#include "mount.h"

static HWND gw_dialog = 0;
static HWND gw_url = 0;
static HWND gw_rate = 0;
static HWND gw_progress = 0;
static HWND gw_pprogress = 0;
static HWND gw_iprogress = 0;
static HWND gw_progress_text = 0;
static HWND gw_pprogress_text = 0;
static HWND gw_iprogress_text = 0;
static HANDLE init_event;
static int max_bytes = 0;
static int is_local_install = 0;

int total_download_bytes = 0;
int total_download_bytes_sofar = 0;

static BOOL
dialog_cmd (HWND h, int id, HWND hwndctl, UINT code)
{
  switch (id)
    {
    case IDCANCEL:
      exit_setup (0);
    }
}

static BOOL CALLBACK
dialog_proc (HWND h, UINT message, WPARAM wParam, LPARAM lParam)
{
  int i, j;
  HWND listbox;
  switch (message)
    {
    case WM_INITDIALOG:
      gw_dialog = h;
      gw_url = GetDlgItem (h, IDC_DLS_URL);
      gw_rate = GetDlgItem (h, IDC_DLS_RATE);
      gw_progress = GetDlgItem (h, IDC_DLS_PROGRESS);
      gw_pprogress = GetDlgItem (h, IDC_DLS_PPROGRESS);
      gw_iprogress = GetDlgItem (h, IDC_DLS_IPROGRESS);
      gw_progress_text = GetDlgItem (h, IDC_DLS_PROGRESS_TEXT);
      gw_pprogress_text = GetDlgItem (h, IDC_DLS_PPROGRESS_TEXT);
      gw_iprogress_text = GetDlgItem (h, IDC_DLS_IPROGRESS_TEXT);
      SetEvent (init_event);
      return TRUE;
    case WM_COMMAND:
      return HANDLE_WM_COMMAND (h, wParam, lParam, dialog_cmd);
    }
  return FALSE;
}

static WINAPI DWORD
dialog (void *)
{
  int rv = 0;
  MSG m;
  HWND local_gw_dialog = CreateDialog (hinstance, MAKEINTRESOURCE (IDD_DLSTATUS),
				   0, dialog_proc);
  ShowWindow (local_gw_dialog, SW_SHOWNORMAL);
  UpdateWindow (local_gw_dialog);
  while (GetMessage (&m, 0, 0, 0) > 0) {
    TranslateMessage (&m);
    DispatchMessage (&m);
  }
}

static DWORD start_tics;

static void
init_dialog (char *url, int length)
{
  if (is_local_install)
    return;
  if (gw_dialog == 0)
    {
      DWORD tid;
      HANDLE thread;
      init_event = CreateEvent (0, 0, 0, 0);
      thread = CreateThread (0, 0, dialog, 0, 0, &tid);
      WaitForSingleObject (init_event, 1000);
      CloseHandle (init_event);
      SendMessage (gw_progress, PBM_SETRANGE, 0, MAKELPARAM (0, 100));
      SendMessage (gw_pprogress, PBM_SETRANGE, 0, MAKELPARAM (0, 100));
      SendMessage (gw_iprogress, PBM_SETRANGE, 0, MAKELPARAM (0, 100));
    }
  char *sl=url, *cp;
  for (cp=url; *cp; cp++)
    if (*cp == '/' || *cp == '\\' || *cp == ':')
      sl = cp+1;
  max_bytes = length;
  SetWindowText (gw_url, sl);
  SetWindowText (gw_rate, "Connecting...");
  SendMessage (gw_progress, PBM_SETPOS, (WPARAM) 0, 0);
  ShowWindow (gw_progress, (length > 0) ? SW_SHOW : SW_HIDE);
  if (length > 0 )
    SetWindowText (gw_progress_text, "Package");
  else
    SetWindowText (gw_progress_text, "       ");
  ShowWindow (gw_pprogress, (total_download_bytes > 0) ? SW_SHOW : SW_HIDE);
  if (total_download_bytes > 0)
    {
      SetWindowText (gw_pprogress_text, "Total");
      SetWindowText (gw_iprogress_text, "Disk");
    }
  else
    {
      SetWindowText (gw_pprogress_text, "     ");
      SetWindowText (gw_iprogress_text, "    ");
    }
  ShowWindow (gw_iprogress, (total_download_bytes > 0) ? SW_SHOW : SW_HIDE);
  ShowWindow (gw_dialog, SW_SHOWNORMAL);
  start_tics = GetTickCount ();
}


static void
progress (int bytes)
{
  if (is_local_install)
    return;
  static char buf[100];
  int kbps;
  static unsigned int last_tics = 0;
  DWORD tics = GetTickCount ();
  if (tics == start_tics) // to prevent division by zero
    return;
  if (tics < last_tics + 200) // to prevent flickering updates
    return;
  last_tics = tics;

  kbps = bytes / (tics - start_tics);
  ShowWindow (gw_progress, (max_bytes > 0) ? SW_SHOW : SW_HIDE);
  ShowWindow (gw_pprogress, (total_download_bytes > 0) ? SW_SHOW : SW_HIDE);
  ShowWindow (gw_iprogress, (total_download_bytes > 0) ? SW_SHOW : SW_HIDE);
  if (max_bytes > 100)
    {
      int perc = bytes / (max_bytes / 100);
      SendMessage (gw_progress, PBM_SETPOS, (WPARAM) perc, 0);
      sprintf (buf, "%3d %%  (%dk/%dk)  %d kb/s\n",
	       perc, bytes/1000, max_bytes/1000, kbps);
      if (total_download_bytes > 0)
        {
          int totalperc = (total_download_bytes_sofar + bytes) / (
                    total_download_bytes / 100);
          SendMessage (gw_pprogress, PBM_SETPOS, (WPARAM) totalperc, 0);
        }
    }
  else
    sprintf (buf, "%d  %d kb/s\n", bytes, kbps);

  SetWindowText (gw_rate, buf);
}

struct GUBuf {
  GUBuf *next;
  int count;
  char buf[2000];
};

char *
get_url_to_string (char *_url)
{
  log (LOG_BABBLE, "get_url_to_string %s", _url);
  is_local_install = (source == IDC_SOURCE_CWD);
  init_dialog (_url, 0);
  NetIO *n = NetIO::open (_url);
  if (!n || !n->ok ())
    {
      delete n;
      log (LOG_BABBLE, "get_url_to_string failed!");
      return 0;
    }

  if (n->file_size)
    max_bytes = n->file_size;

  GUBuf *bufs = 0;
  GUBuf **nextp = &bufs;
  int total_bytes = 1; /* for the NUL */
  progress (0);
  while (1)
    {
      GUBuf *b = new GUBuf;
      *nextp = b;
      b->next = 0;
      nextp = &(b->next);

      b->count = n->read (b->buf, sizeof (b->buf));
      if (b->count <= 0)
	break;
      total_bytes += b->count;
      progress (total_bytes);
    }

  char *rv = (char *) malloc (total_bytes);
  if (NULL == rv)
    {
      log (LOG_BABBLE, "get_url_to_string(): malloc failed for rv!");
      return 0;
    }
  char *rvp = rv;
  while (bufs && bufs->count > 0)
    {
      GUBuf *tmp = bufs->next;
      memcpy (rvp, bufs->buf, bufs->count);
      rvp += bufs->count;
      delete bufs;
      bufs = tmp;
    }
  *rvp = 0;
  return rv;
}

int
get_url_to_file (char *_url, char *_filename, int expected_length,
		 BOOL allow_ftp_auth)
{
  log (LOG_BABBLE, "get_url_to_file %s %s", _url, _filename);
  if (total_download_bytes > 0)
    {
      int df = diskfull (get_root_dir ());
      SendMessage (gw_iprogress, PBM_SETPOS, (WPARAM) df, 0);
    }
  init_dialog (_url, expected_length);

  remove (_filename); /* but ignore errors */

  NetIO *n = NetIO::open (_url, allow_ftp_auth);
  if (!n || !n->ok ())
    {
      delete n;
      log (LOG_BABBLE, "get_url_to_file failed!");
      return 1;
    }

  FILE *f = fopen (_filename, "wb");
  if (!f)
    {
      char *err = strerror (errno);
      if (!err)
	err = "(unknown error)";
      fatal (IDS_ERR_OPEN_WRITE, _filename, err);
    }

  if (n->file_size)
    max_bytes = n->file_size;

  int total_bytes = 0;
  progress (0);
  while (1)
    {
      char buf[8192];
      int count;
      count = n->read (buf, sizeof (buf));
      if (count <= 0)
	break;
      fwrite (buf, 1, count, f);
      total_bytes += count;
      progress (total_bytes);
    }

  total_download_bytes_sofar += total_bytes;

  fclose (f);

  if (total_download_bytes > 0)
    {
      int df = diskfull (get_root_dir ());
      SendMessage (gw_iprogress, PBM_SETPOS, (WPARAM) df, 0);
    }

  return 0;
}

void
dismiss_url_status_dialog ()
{
  if (!is_local_install)
    ShowWindow (gw_dialog, SW_HIDE);
}
