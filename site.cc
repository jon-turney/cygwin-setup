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

#include "site.h"
#include "win32.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>
#include <algorithm>

#include "dialog.h"
#include "resource.h"
#include "state.h"
#include "geturl.h"
#include "msg.h"
#include "LogSingleton.h"
#include "io_stream.h"
#include "site.h"

#include "propsheet.h"

#include "threebar.h"
#include "ControlAdjuster.h"
#include "Exception.h"

extern ThreeBarProgressPage Progress;

/*
  Sizing information.
 */
static ControlAdjuster::ControlInfo SiteControlsInfo[] = {
  {IDC_URL_LIST, 		CP_STRETCH, CP_STRETCH},
  {IDC_EDIT_USER_URL,		CP_STRETCH, CP_BOTTOM},
  {IDC_BUTTON_ADD_URL,		CP_RIGHT,   CP_BOTTOM},
  {IDC_SITE_USERURL,            CP_LEFT,    CP_BOTTOM},
  {0, CP_LEFT, CP_TOP}
};

SitePage::SitePage ()
{
  sizeProcessor.AddControlInfo (SiteControlsInfo);
}

#include "getopt++/StringOption.h"
#include "UserSettings.h"

using namespace std;

SiteList site_list;
SiteList all_site_list;

StringOption SiteOption("", 's', "site", "Download site", false);

/* XXX make into a singleton? */
static SiteSetting ChosenSites;

void 
SiteSetting::load()
{
  string SiteOptionString = SiteOption;
  if (SiteOptionString.size()) 
    registerSavedSite (SiteOptionString.c_str());
  else
    getSavedSites ();
}

void 
SiteSetting::save()
{
  io_stream *f = UserSettings::Instance().settingFileForSave("last-mirror");
  if (f)
    {
      for (SiteList::const_iterator n = site_list.begin ();
      n != site_list.end (); ++n)
        f->write ((n->url + "\n").c_str(), n->url.size() + 1);
      delete f;
    }
}

void
site_list_type::init (String const &newurl)
{
  url = newurl;

  char *dots = new_cstr_char_array (newurl);
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

site_list_type::site_list_type (site_list_type const &rhs)
{
  key = rhs.key;
  url = rhs.url;
  displayed_url = rhs.displayed_url;
}

site_list_type &
site_list_type::operator= (site_list_type const &rhs)
{
  key = rhs.key;
  url = rhs.url;
  displayed_url = rhs.displayed_url;
  return *this;
}

bool
site_list_type::operator == (site_list_type const &rhs) const
{
  return key.casecompare (rhs.key) == 0; 
}

bool
site_list_type::operator < (site_list_type const &rhs) const
{
  return key.casecompare (rhs.key) < 0;
}

static void
save_dialog (HWND h)
{
  // Remove anything that was previously in the selected site list.
  site_list.clear ();

  HWND listbox = GetDlgItem (h, IDC_URL_LIST);
  int sel_count = SendMessage (listbox, LB_GETSELCOUNT, 0, 0);
  if (sel_count > 0)
    {
      int sel_buffer[sel_count];
      SendMessage (listbox, LB_GETSELITEMS, sel_count, (LPARAM) sel_buffer);
      for (int n = 0; n < sel_count; n++)
	{
	  int mirror =
	    SendMessage (listbox, LB_GETITEMDATA, sel_buffer[n], 0);
	  site_list.push_back (all_site_list[mirror]);
	}
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
    if (mirrors.size())
      {
	io_stream *f = UserSettings::Instance().settingFileForSave("mirrors-lst");
	if (f)
	  {
	    f->write(mirrors.c_str(), mirrors.size() + 1);
	    delete f;
	  }
      }
    else
      {
	io_stream *f = UserSettings::Instance().settingFileForLoad("mirrors-lst");
	if (f)
	  {
	    int len;
	    while (len = f->read (mirror_url, 999))
	      {
		mirror_url[len] = '\0';
		mirrors += mirror_url;
	      }
	    delete f;
	    log (LOG_BABBLE) << "Using cached mirror list" << endLog;
	  }
	else
	  {
	    log (LOG_BABBLE) << "Defaulting to empty mirror list" << endLog;
	  }
      }
    theString = new_cstr_char_array (mirrors);
    nl = theString;
  }

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
      if (*bol == '#' || !*bol)
        continue;
      /* Accept only the URL schemes we can understand. */
      if (strncmp(bol, "http://", 7) == 0 ||
	  strncmp(bol, "ftp://", 6) == 0)
	{
	  char *semi = strchr (bol, ';');
	  if (semi)
	    *semi = 0;
	  site_list_type newsite (bol);
	  SiteList::iterator i = find (all_site_list.begin(),
				       all_site_list.end(), newsite);
	  if (i == all_site_list.end())
	    {
	      SiteList result;
	      merge (all_site_list.begin(), all_site_list.end(),
		     &newsite, &newsite + 1,
		     inserter (result, result.begin()));
	      all_site_list = result;
	    }
	  else
	    //TODO: remove and remerge 
	    *i = newsite;
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

void
SiteSetting::registerSavedSite (const char * site)
{
  site_list_type tempSite(site);
  SiteList::iterator i = find (all_site_list.begin(),
			       all_site_list.end(), tempSite);
  if (i == all_site_list.end())
    {
      /* Don't default to certain machines ever since they suffer
	 from bandwidth limitations. */
      if (strnicmp (site, NOSAVE1, NOSAVE1_LEN) == 0
	  || strnicmp (site, NOSAVE2, NOSAVE2_LEN) == 0
	  || strnicmp (site, NOSAVE3, NOSAVE3_LEN) == 0)
	return;
      SiteList result;
      merge (all_site_list.begin(), all_site_list.end(),
	     &tempSite, &tempSite + 1,
	     inserter (result, result.begin()));
      all_site_list = result;
      site_list.push_back (tempSite);
    }
  else
    site_list.push_back (tempSite);
}

void
SiteSetting::getSavedSites ()
{
  io_stream *f = UserSettings::Instance().settingFileForLoad("last-mirror");
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

      registerSavedSite (site);

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

  try
  {
    hinst = (HINSTANCE) (context[0]);
    h = (HWND) (context[1]);
    static bool downloaded = false;
    if (!downloaded && get_site_list (hinst, h))
    {
      // Error: Couldn't download the site info.
      // Go back to the Net setup page.
      MessageBox (h, TEXT ("Can't get list of download sites.\n")
          TEXT("Make sure your network settings are correct and try again."),
          NULL, MB_OK);

      // Tell the progress page that we're done downloading
      Progress.PostMessage (WM_APP_SITE_INFO_DOWNLOAD_COMPLETE, 0, IDD_NET);
    }
    else 
    {
      downloaded = true;
      // Everything worked, go to the site select page
      // Tell the progress page that we're done downloading
      Progress.PostMessage (WM_APP_SITE_INFO_DOWNLOAD_COMPLETE, 0, IDD_SITE);
    }
  }
  TOPLEVEL_CATCH("site");

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

long
SitePage::OnNext ()
{
  HWND h = GetHWND ();

  save_dialog (h);
  ChosenSites.save ();

  // Log all the selected URLs from the list.
  for (SiteList::const_iterator n = site_list.begin ();
       n != site_list.end (); ++n)
    log (LOG_PLAIN) << "site: " << n->url << endLog;

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

long
SitePage::OnUnattended ()
{
  if (SendMessage (GetDlgItem (IDC_URL_LIST), LB_GETSELCOUNT, 0, 0) > 0)
    return OnNext ();
  else
    return -2;
}

void
SitePage::CheckControlsAndDisableAccordingly () const
{
  DWORD ButtonFlags = PSWIZB_BACK;

  // Check that at least one download site is selected.
  if (SendMessage (GetDlgItem (IDC_URL_LIST), LB_GETSELCOUNT, 0, 0) > 0)
    {
      // At least one site selected, enable "Next".
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
  for (SiteList::const_iterator i = all_site_list.begin ();
       i != all_site_list.end (); ++i)
    {
      j = SendMessage (listbox, LB_ADDSTRING, 0,
		       (LPARAM) i->displayed_url.c_str());
      SendMessage (listbox, LB_SETITEMDATA, j, j);
    }

  // Select the selected ones.
  for (SiteList::const_iterator n = site_list.begin ();
       n != site_list.end (); ++n)
    {
      int index = SendMessage (listbox, LB_FINDSTRING, (WPARAM) - 1,
			       (LPARAM) n->displayed_url.c_str());
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
	    site_list_type newsite (other_url);
	    SiteList::iterator i = find (all_site_list.begin(),
					 all_site_list.end(), newsite);
	    if (i == all_site_list.end())
	      {
		all_site_list.push_back (newsite);
		log (LOG_BABBLE) << "Adding site: " << other_url << endLog;
	      }
	    else
	      {
		*i = newsite;
		log (LOG_BABBLE) << "Replacing site: " << other_url << endLog;
	      }

	    // Assume the user wants to use it and select it for him.
	    site_list.push_back (newsite);

	    // Update the list box.
	    PopulateListBox ();
	    // And allow the user to continue
	    CheckControlsAndDisableAccordingly ();
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
