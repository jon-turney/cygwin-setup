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

/* The purpose of this file is to get the list of mirror sites and ask
   the user which mirror site they want to download from. */

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include "win32.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dialog.h"
#include "resource.h"
#include "state.h"
#include "geturl.h"
#include "msg.h"
#include "concat.h"
#include "log.h"
#include "io_stream.h"
#include "site.h"

#include "port.h"

#define NO_IDX (-1)
#define OTHER_IDX (-2)

list < site_list_type, const char *, strcasecmp > site_list;
list < site_list_type, const char *, strcasecmp > all_site_list;
static int mirror_idx = NO_IDX;

void
site_list_type::init (char const *newurl)
{
  url = _strdup (newurl);
  displayed_url = _strdup (newurl);
  char *dot = strchr (displayed_url, '.');
  if (dot)
    {
      dot = strchr (dot, '/');
      if (dot)
	*dot = 0;
    }
  key = (char *) malloc (2 * strlen (newurl) + 3);

  dot = displayed_url;
  dot += strlen (dot);
  char *dp = key;
  while (dot != displayed_url)
    {
      if (*dot == '.' || *dot == '/')
	{
	  char *sp;
	  if (dot[3] == 0)
	    *dp++ = '~';	/* sort .com/.edu/.org together */
	  for (sp = dot + 1; *sp && *sp != '.' && *sp != '/';)
	    *dp++ = *sp++;
	  *dp++ = ' ';
	}
      dot--;
    }
  *dp++ = ' ';
  strcpy (dp, displayed_url);
}

site_list_type::site_list_type (char const *newurl)
{
  init (newurl);
}

static void
check_if_enable_next (HWND h)
{
  EnableWindow (GetDlgItem (h, IDOK),
		SendMessage (GetDlgItem (h, IDC_URL_LIST), LB_GETSELCOUNT, 0,
			     0) > 0 ? 1 : 0);
}

static void
load_dialog (HWND h)
{
  HWND listbox = GetDlgItem (h, IDC_URL_LIST);
  for (size_t n = 1; n <= site_list.number (); n++)
    {
      int index = SendMessage (listbox, LB_FINDSTRING, (WPARAM) - 1,
			       (LPARAM) site_list[n]->displayed_url);
      if (index != LB_ERR)
	SendMessage (listbox, LB_SELITEMRANGE, TRUE, (index << 16) | index);
    }
  check_if_enable_next (h);
}

static void
save_dialog (HWND h)
{
  HWND listbox = GetDlgItem (h, IDC_URL_LIST);
  mirror_idx = 0;
  while (site_list.number () > 0)
    /* we don't delete the object because it's stored in the all_site_list. */
    site_list.removebyindex (1);
  int sel_count = SendMessage (listbox, LB_GETSELCOUNT, 0, 0);
  if (sel_count > 0)
    {
      int sel_buffer[sel_count];
      int sel_count2 = SendMessage (listbox, LB_GETSELITEMS, sel_count,
				    (LPARAM) sel_buffer);
      if (sel_count != sel_count2)
	{
	  NEXT (IDD_SITE);
	}
      for (int n = 0; n < sel_count; n++)
	{
	  int mirror =
	    SendMessage (listbox, LB_GETITEMDATA, sel_buffer[n], 0);
	  if (mirror == OTHER_IDX)
	    mirror_idx = OTHER_IDX;
	  else
	    site_list.registerbyobject (*all_site_list[mirror]);
	}
    }
  else
    {
      NEXT (IDD_SITE);
    }
}

void
save_site_url ()
{
  io_stream *f = io_stream::open ("cygfile:///etc/setup/last-mirror", "wb");
  for (size_t n = 1; n <= site_list.number (); n++)
    {
      if (f)
	{
	  char temp[_MAX_PATH];
	  /* TODO: potential buffer overflow. we need snprintf asap. */
	  // FIXME: write all selected sites
	  sprintf (temp, "%s\n", site_list[n]->url);
	  f->write (temp, strlen (temp));
	}
    }
  delete f;
}

static BOOL
dialog_cmd (HWND h, int id, HWND hwndctl, UINT code)
{
  switch (id)
    {

    case IDC_URL_LIST:
      check_if_enable_next (h);
      break;

    case IDOK:
      save_dialog (h);
      if (mirror_idx == OTHER_IDX)
	NEXT (IDD_OTHER_URL);
      else
	{
	  save_site_url ();
	  NEXT (IDD_S_LOAD_INI);
	}
      break;

    case IDC_BACK:
      save_dialog (h);
      NEXT (IDD_NET);
      break;

    case IDCANCEL:
      NEXT (0);
      break;
    }
  return 0;
}

static BOOL CALLBACK
dialog_proc (HWND h, UINT message, WPARAM wParam, LPARAM lParam)
{
  int j;
  HWND listbox;
  switch (message)
    {
    case WM_INITDIALOG:
      listbox = GetDlgItem (h, IDC_URL_LIST);
      for (size_t i = 1; i <= all_site_list.number (); i++)
	{
	  j =
	    SendMessage (listbox, LB_ADDSTRING, 0,
			 (LPARAM) all_site_list[i]->displayed_url);
	  SendMessage (listbox, LB_SETITEMDATA, j, i);
	}
      j = SendMessage (listbox, LB_ADDSTRING, 0, (LPARAM) "Other URL");
      SendMessage (listbox, LB_SETITEMDATA, j, OTHER_IDX);
      load_dialog (h);
      return FALSE;
    case WM_COMMAND:
      return HANDLE_WM_COMMAND (h, wParam, lParam, dialog_cmd);
    }
  return FALSE;
}

static int
get_site_list (HINSTANCE h)
{
  char mirror_url[1000];
  if (LoadString (h, IDS_MIRROR_LST, mirror_url, sizeof (mirror_url)) <= 0)
    return 1;
  char *mirrors = get_url_to_string (mirror_url);
  dismiss_url_status_dialog ();
  if (!mirrors)
    return 1;

  char *bol, *eol, *nl;

  nl = mirrors;
  while (*nl)
    {
      bol = nl;
      for (eol = bol; *eol && *eol != '\n'; eol++);
      if (*eol)
	nl = eol + 1;
      else
	nl = eol;
      while (eol > bol && eol[-1] == '\r')
	eol--;
      *eol = 0;
      if (bol[0] != '#' && bol[0] > ' ')
	{
	  char *semi = strchr (bol, ';');
	  if (semi)
	    *semi = 0;
	  site_list_type *newsite = new site_list_type (bol);
	  site_list_type & listobj =
	    all_site_list.registerbyobject (*newsite);
	  if (&listobj != newsite)
	    /* That site was already registered */
	    delete newsite;
	}
    }
  delete[]mirrors;

  return 0;
}

/* List of machines that should not be used by default when saved
   in "last-mirror". */
#define NOSAVE1 "ftp://sources.redhat.com/"
#define NOSAVE1_LEN (sizeof ("ftp://sources.redhat.com/") - 1)
#define NOSAVE2 "ftp://sourceware.cygnus.com/"
#define NOSAVE2_LEN (sizeof ("ftp://sourceware.cygnus.com/") - 1)
#define NOSAVE3 "ftp://gcc.gnu.org/"
#define NOSAVE3_LEN (sizeof ("ftp://gcc.gnu.org/") - 1)

static void
get_saved_sites ()
{
  io_stream *f = io_stream::open ("cygfile:///etc/setup/last-mirror", "rt");
  if (!f)
    return;

  char site[1000];
  char *fg_ret;
  while ((fg_ret = f->gets (site, 1000)))
    {

      char *eos = site + strlen (site) - 1;
      while (eos >= site && (*eos == '\n' || *eos == '\r'))
	*eos-- = '\0';

      if (eos < site)
	continue;

      bool found = false;
      for (size_t i = 1; !found && i <= all_site_list.number (); i++)
	if (!strcasecmp (site, all_site_list[i]->url))
	  found = true;

      if (!found)
	{
	  /* Don't default to certain machines ever since they suffer
	     from bandwidth limitations. */
	  if (strnicmp (site, NOSAVE1, NOSAVE1_LEN) == 0
	      || strnicmp (site, NOSAVE2, NOSAVE2_LEN) == 0
	      || strnicmp (site, NOSAVE3, NOSAVE3_LEN) == 0)
	    return;
	  site_list_type *newsite = new site_list_type (site);
	  site_list_type & listobj =
	    all_site_list.registerbyobject (*newsite);
	  if (&listobj != newsite)
	    /* That site was already registered - shouldn't happen, but safety first */
	    delete newsite;
	}
      /* TODO: make a site_type method to create a serach key on-the-fly from a 
         URL
       */
      found = false;
      for (size_t i = 1; !found && i <= all_site_list.number (); i++)
	if (!strcasecmp (site, all_site_list[i]->url))
	  site_list.registerbyobject (*all_site_list[i]);
    }
  delete f;

}

void
do_site (HINSTANCE h)
{
  int rv = 0;

  if (all_site_list.number () == 0)
    if (get_site_list (h))
      {
	NEXT (IDD_NET);
	return;
      }

  get_saved_sites ();

  rv = DialogBox (h, MAKEINTRESOURCE (IDD_SITE), 0, dialog_proc);
  if (rv == -1)
    fatal (IDS_DIALOG_FAILED);

  for (size_t n = 1; n <= site_list.number (); n++)
    log (0, "site: %s", site_list[n]->url);
}
