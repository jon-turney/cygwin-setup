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
#include <process.h>

#include "dialog.h"
#include "resource.h"
#include "state.h"
#include "geturl.h"
#include "msg.h"
#include "log.h"
#include "io_stream.h"
#include "site.h"

#include "port.h"

#include "site.h"
#include "propsheet.h"

#include "threebar.h"
extern ThreeBarProgressPage Progress;

list < site_list_type, String, String::casecompare > site_list;
list < site_list_type, String, String::casecompare > all_site_list;

void
site_list_type::init (String const &newurl)
{
  url = newurl;

  char *dots = newurl.cstr();
  char *dot = strchr (dots, '.');
  if (dot)
      {
         dot = strchr (dot, '/');
        if (dot)
	        *dot = 0;
   }
  displayed_url = String (dots);

  
  dot = dots + strlen (dots);
  char *dpsave, *dp = new char[2 * newurl.size() + 3];
  dpsave = dp;
  while (dot != dots)
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
      --dot;
    }
  *dp++ = ' ';
  strcpy (dp, dots);
  delete[] dots;
  key = String (dp);
  delete[] dpsave;
}

site_list_type::site_list_type (String const &newurl)
{
  init (newurl);
}

static void
save_dialog (HWND h)
{
  // Remove anything that was previously in the selected site list.
  while (site_list.number () > 0)
    {
      // we don't delete the object because it's stored in the all_site_list.
      site_list.removebyindex (1);
    }

  HWND listbox = GetDlgItem (h, IDC_URL_LIST);
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
	  site_list.registerbyobject (*all_site_list[mirror]);
	}
    }
}

void
save_site_url ()
{
  io_stream *f = io_stream::open ("cygfile:///etc/setup/last-mirror", "wb");
  if (f)
    {
      for (size_t n = 1; n <= site_list.number (); n++)
        f->write ((site_list[n]->url + "\n").cstr_oneuse(), site_list[n]->url.size() + 1);
      delete f;
    }
}

static int
get_site_list (HINSTANCE h, HWND owner)
{
  char mirror_url[1000];

  if (LoadString (h, IDS_MIRROR_LST, mirror_url, sizeof (mirror_url)) <= 0)
    return 1;
  char *bol, *eol, *nl, *theString;
  {
  String mirrors = get_url_to_string (mirror_url, owner);
  if (!mirrors.size())
    return 1;

  nl = theString = mirrors.cstr();}

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
  delete[] theString;

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

      String tempKey = site_list_type (String(site)).key;
      if  (!all_site_list.getbykey (tempKey))
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
      // Was it an allowed URL? 
      site_list_type *tempSite;
      if ((tempSite = all_site_list.getbykey (tempKey)))
	site_list.registerbyobject (*tempSite);
    }
  delete f;

}

static DWORD WINAPI
do_download_site_info_thread (void *p)
{
  HANDLE *context;
  HINSTANCE hinst;
  HWND h;
  context = (HANDLE *) p;

  hinst = (HINSTANCE) (context[0]);
  h = (HWND) (context[1]);

  if (all_site_list.number () == 0
      && get_site_list (hinst, h))
	{
	  // Error: Couldn't download the site info.  Go back to the Net setup page.
	  MessageBox (h, TEXT ("Can't get list of download sites.\n\
Make sure your network settings are corect and try again."), NULL, MB_OK);

	  // Tell the progress page that we're done downloading
	  Progress.PostMessage (WM_APP_SITE_INFO_DOWNLOAD_COMPLETE, 0,
				IDD_NET);

	}
  else
  // Everything worked, go to the site select page
  // Tell the progress page that we're done downloading
  Progress.PostMessage (WM_APP_SITE_INFO_DOWNLOAD_COMPLETE, 0, IDD_SITE);

  ExitThread(0);
}

static HANDLE context[2];

void
do_download_site_info (HINSTANCE hinst, HWND owner)
{

  context[0] = hinst;
  context[1] = owner;

  DWORD threadID;
  CreateThread (NULL, 0, do_download_site_info_thread, context, 0, &threadID);
}

bool SitePage::Create ()
{
  return PropertyPage::Create (IDD_SITE);
}

void
SitePage::OnInit ()
{
  get_saved_sites ();
}

long
SitePage::OnNext ()
{
  HWND h = GetHWND ();

  save_dialog (h);
  save_site_url ();

  // Log all the selected URLs from the list.    
  for (size_t n = 1; n <= site_list.number (); n++)
    log (LOG_PLAIN, String ("site: ") + site_list[n]->url);

  Progress.SetActivateTask (WM_APP_START_SETUP_INI_DOWNLOAD);
  return IDD_INSTATUS;

  return 0;
}

long
SitePage::OnBack ()
{
  HWND h = GetHWND ();

  save_dialog (h);

  // Go back to the net connection type page
  return 0;
}

void
SitePage::OnActivate ()
{
  // Fill the list box with all known sites.
  PopulateListBox ();

  // Load the user URL box with nothing - it is in the list already.
  eset (GetHWND (), IDC_EDIT_USER_URL, "");

  // Get the enabled/disabled states of the controls set accordingly.
  CheckControlsAndDisableAccordingly ();
}

void
SitePage::CheckControlsAndDisableAccordingly () const
{
  DWORD ButtonFlags = PSWIZB_BACK;

  // Check that at least one download site is selected.
  if (SendMessage (GetDlgItem (IDC_URL_LIST), LB_GETSELCOUNT, 0, 0) > 0)
    {
      // At least one official site selected, enable "Next".
      ButtonFlags |= PSWIZB_NEXT;
    }
  GetOwner ()->SetButtons (ButtonFlags);
}

void
SitePage::PopulateListBox ()
{
  int j;
  HWND listbox = GetDlgItem (IDC_URL_LIST);

  // Populate the list box with the URLs.
  SendMessage (listbox, LB_RESETCONTENT, 0, 0);
  for (size_t i = 1; i <= all_site_list.number (); i++)
    {
      j = SendMessage (listbox, LB_ADDSTRING, 0,
		       (LPARAM) all_site_list[i]->displayed_url.cstr_oneuse());
      SendMessage (listbox, LB_SETITEMDATA, j, i);
    }

  // Select the selected ones.
  for (size_t n = 1; n <= site_list.number (); n++)
    {
      int index = SendMessage (listbox, LB_FINDSTRING, (WPARAM) - 1,
			       (LPARAM) site_list[n]->displayed_url.cstr_oneuse());
      if (index != LB_ERR)
	{
	  // Highlight the selected item
	  SendMessage (listbox, LB_SELITEMRANGE, TRUE, (index << 16) | index);
	  // Make sure it's fully visible
	  SendMessage (listbox, LB_SETCARETINDEX, index, FALSE);
	}
    }
}

bool SitePage::OnMessageCmd (int id, HWND hwndctl, UINT code)
{
  switch (id)
    {
    case IDC_EDIT_USER_URL:
      {
	// FIXME: Make Enter here cause an ADD, not a NEXT.
	break;
      }
    case IDC_URL_LIST:
      {
	if (code == LBN_SELCHANGE)
	  {
	    CheckControlsAndDisableAccordingly ();
	    save_dialog (GetHWND ());
	  }
	break;
      }
    case IDC_BUTTON_ADD_URL:
      {
	if (code == BN_CLICKED)
	  {
	    // User pushed the Add button.
	    String other_url = egetString (GetHWND (), IDC_EDIT_USER_URL);
	    if (other_url.size())
	    {
	    site_list_type *
	      newsite =
	      new
	      site_list_type (other_url);
	    site_list_type & listobj =
	      all_site_list.registerbyobject (*newsite);
	    if (&listobj != newsite)
	      {
		// That site was already registered
		delete
		  newsite;
	      }
	    else
	      {
		// Log the adding of this new URL.
		log (LOG_BABBLE, String ("Adding site: ") + other_url);
	      }

	    // Assume the user wants to use it and select it for him.
	    site_list.registerbyobject (listobj);

	    // Update the list box.
	    PopulateListBox ();
	    eset (GetHWND (), IDC_EDIT_USER_URL, "");
	    }
	  }
	break;
      }
    default:
      // Wasn't recognized or handled.
      return false;
    }

  // Was handled since we never got to default above.
  return true;
}
