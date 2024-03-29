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

#include <string>
#include <algorithm>

#include "site.h"
#include "win32.h"
#include <stdio.h>
#include <stdlib.h>
#include <process.h>

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
#include "String++.h"

#define MIRROR_LIST_URL "https://cygwin.com/mirrors.lst"

extern ThreeBarProgressPage Progress;

/*
  What to do if dropped mirrors are selected.
*/
enum
{
  CACHE_REJECT,		// Go back to re-select mirrors.
  CACHE_ACCEPT_WARN,	// Go on. Warn again next time.
  CACHE_ACCEPT_NOWARN	// Go on. Don't warn again.
};

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

#include "getopt++/StringArrayOption.h"
#include "getopt++/BoolOption.h"
#include "UserSettings.h"

bool cache_is_usable;
bool cache_needs_writing;
std::string cache_warn_urls;

/* Selected sites */
SiteList site_list;

/* Fresh mirrors + selected sites */
SiteList all_site_list;

/* Previously fresh + cached before */
SiteList cached_site_list;

/* Stale selected sites to warn about and add to cache */
SiteList dropped_site_list;

StringArrayOption SiteOption('s', "site", IDS_HELPTEXT_SITE);
BoolOption OnlySiteOption(false, 'O', "only-site", IDS_HELPTEXT_ONLY_SITE);
extern BoolOption UnsupportedOption;

SiteSetting::SiteSetting (): saved (false)
{
  std::vector<std::string> SiteOptionStrings = SiteOption;
  if (SiteOptionStrings.size())
    {
      for (std::vector<std::string>::const_iterator n = SiteOptionStrings.begin ();
	   n != SiteOptionStrings.end (); ++n)
	registerSavedSite (n->c_str ());
    }
  else
    getSavedSites ();
}

const char *
SiteSetting::lastMirrorKey ()
{
  if (UnsupportedOption)
    return "last-mirror-unsupported";

  return "last-mirror";
}

void 
SiteSetting::save()
{
  io_stream *f = UserSettings::instance().open (lastMirrorKey ());
  if (f)
    {
      for (SiteList::const_iterator n = site_list.begin ();
	   n != site_list.end (); ++n)
	*f << n->url;
      delete f;
    }
  saved = true;
}

SiteSetting::~SiteSetting ()
{
  if (!saved)
    save ();
}

site_list_type::site_list_type (const std::string &_url,
				const std::string &_servername,
				const std::string &_area,
				const std::string &_location,
				bool _from_mirrors_lst,
                                bool _noshow = false)
{
  url = _url;
  servername = _servername;
  area = _area;
  location = _location;
  from_mirrors_lst = _from_mirrors_lst;
  noshow = _noshow;

  /* Canonicalize URL to ensure it ends with a '/' */
  if (url.at(url.length()-1) != '/')
    url.append("/");

  /* displayed_url is protocol and site name part of url */
  std::string::size_type path_offset = url.find ("/", url.find ("//") + 2);
  displayed_url = url.substr(0, path_offset);

  /* the sorting key is hostname components in reverse order (to sort by country code)
     plus the url (to ensure uniqueness) */
  key = std::string();
  std::string::size_type last_idx = displayed_url.length () - 1;
  std::string::size_type idx = url.find_last_of("./", last_idx);
  if (last_idx - idx == 3)
  {
    /* Sort non-country TLDs (.com, .net, ...) together. */
    key += " ";
  }
  do
  {
    key += url.substr(idx + 1, last_idx - idx);
    key += " ";
    last_idx = idx - 1;
    idx = url.find_last_of("./", last_idx);
    if (idx == std::string::npos)
      idx = 0;
  } while (idx > 0);
  key += url;
}

bool
site_list_type::operator == (site_list_type const &rhs) const
{
  return stricmp (key.c_str(), rhs.key.c_str()) == 0; 
}

bool
site_list_type::operator < (site_list_type const &rhs) const
{
  return stricmp (key.c_str(), rhs.key.c_str()) < 0; 
}

/*
  A SiteList is maintained as an in-order std::vector of site_list_type, by
  replacing it with a new object with the new item inserted in the correct
  place.

  Yes, we could just use an ordered container, instead.
*/
static void
site_list_insert(SiteList &site_list, site_list_type newsite)
{
  SiteList::iterator i = find (site_list.begin(), site_list.end(), newsite);
  if (i == site_list.end())
    {
      SiteList result;
      merge (site_list.begin(), site_list.end(),
             &newsite, &newsite + 1,
             inserter (result, result.begin()));
      site_list = result;
    }
  else
    *i = newsite;
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

// This is called only for lists of mirrors that came (now or in a
// previous setup run) from mirrors.lst.
void
load_site_list (SiteList& theSites, char *theString)
{
  char *bol, *eol, *nl;
  
  nl = theString;
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
          strncmp(bol, "https://", 8) == 0 ||
          strncmp(bol, "ftp://", 6) == 0 ||
          strncmp(bol, "ftps://", 7) == 0)
        {
          int i;
          char *semi[4];

          /* split into up to 4 semicolon-delimited parts */
          for (i = 0; i < 4; i++)
            semi[i] = 0;

          char *p = bol;
          for (i = 0; i < 4; i++)
            {
              semi[i] = strchr (p, ';');
              if (!semi[i])
                break;

              *semi[i] = 0;
              p = ++semi[i];
            }

          /* Ignore malformed lines */
          if (!semi[0] || !semi[1] || !semi[2])
            continue;

          bool noshow = semi[3] && (strncmp(semi[3], "noshow", 6) == 0);

          site_list_type newsite (bol, semi[0], semi[1], semi[2], true, noshow);
          site_list_insert (theSites, newsite);
        }
        else
        {
          Log (LOG_BABBLE) << "Discarding line '" << bol << "' due to unknown protocol" << endLog;
        }
    }
}

static void
migrate_selected_site_list()
{
  const std::string http = "http://";

  for (SiteList::iterator i = site_list.begin();
       i != site_list.end();
       ++i)
    {
      /* If the saved selected site URL starts with "http://", and the same URL,
         but starting with "https://" appears in the mirror list, migrate to
         "https://" */
      if (strnicmp(i->url.c_str(), http.c_str(), strlen(http.c_str())) == 0)
        {
          std::string migrated_site = "https://";
          migrated_site.append(i->url.substr(http.length()));

          site_list_type migrate(migrated_site, "", "", "", false);
          SiteList::iterator j = find (all_site_list.begin(),
                                       all_site_list.end(), migrate);
          if (j != all_site_list.end())
            {
              Log (LOG_PLAIN) << "Migrated " << i->url << " to " << migrated_site  << endLog;
              *i = migrate;
            }
        }
    }
}

static int
get_site_list (HINSTANCE h, HWND owner)
{
  char *theMirrorString, *theCachedString;

  if (UnsupportedOption)
    return 0;

  const char *cached_mirrors = OnlySiteOption ? NULL : UserSettings::instance().get ("mirrors-lst");
  if (cached_mirrors)
    {
      Log (LOG_BABBLE) << "Loaded cached mirror list" << endLog;
      cache_is_usable = true;
    }
  else
    {
      Log (LOG_BABBLE) << "Cached mirror list unavailable" << endLog;
      cache_is_usable = false;
      cached_mirrors = "";
    }

  std::string mirrors = OnlySiteOption ? std::string ("") : get_url_to_string (MIRROR_LIST_URL, owner);
  if (mirrors.size())
    cache_needs_writing = true;
  else
    {
      if (!cached_mirrors[0])
        {
          if (!OnlySiteOption)
            note(owner, IDS_NO_MIRROR_LST);
          Log (LOG_BABBLE) << "Defaulting to empty mirror list" << endLog;
        }
      else
	{
	  mirrors = cached_mirrors;
	  Log (LOG_BABBLE) << "Using cached mirror list" << endLog;
	}
      cache_is_usable = false;
      cache_needs_writing = false;
    }
  theMirrorString = new_cstr_char_array (mirrors);
  theCachedString = new_cstr_char_array (cached_mirrors);

  load_site_list (all_site_list, theMirrorString);
  load_site_list (cached_site_list, theCachedString);

  delete[] theMirrorString;
  delete[] theCachedString;

  migrate_selected_site_list();

  return 0;
}

/* List of machines that should not be used by default when saved
   in "last-mirror". */
#define NOSAVE1 "ftp://sourceware.org/"
#define NOSAVE1_LEN (sizeof (NOSAVE2) - 1)
#define NOSAVE2 "ftp://sources.redhat.com/"
#define NOSAVE2_LEN (sizeof (NOSAVE1) - 1)
#define NOSAVE3 "ftp://gcc.gnu.org/"
#define NOSAVE3_LEN (sizeof (NOSAVE3) - 1)

void
SiteSetting::registerSavedSite (const char * site)
{
  site_list_type tempSite(site, "", "", "", false);

  /* Don't default to certain machines if they suffer from bandwidth
     limitations. */
  if (strnicmp (site, NOSAVE1, NOSAVE1_LEN) == 0
      || strnicmp (site, NOSAVE2, NOSAVE2_LEN) == 0
      || strnicmp (site, NOSAVE3, NOSAVE3_LEN) == 0)
    return;

  site_list_insert (all_site_list, tempSite);
  site_list.push_back (tempSite);
}

void
SiteSetting::getSavedSites ()
{
  const char *buf = UserSettings::instance().get (lastMirrorKey ());
  if (!buf)
    return;
  char *fg_ret = strdup (buf);
  for (char *site = strtok (fg_ret, "\n"); site; site = strtok (NULL, "\n"))
    registerSavedSite (site);
  free (fg_ret);
}

static DWORD WINAPI
do_download_site_info_thread (void *p)
{
  HANDLE *context;
  HINSTANCE hinst;
  HWND h;
  context = (HANDLE *) p;

  SetThreadUILanguage(langid);

  try
  {
    hinst = (HINSTANCE) (context[0]);
    h = (HWND) (context[1]);
    static bool downloaded = false;
    if (!downloaded && get_site_list (hinst, h))
    {
      // Error: Couldn't download the site info.
      // Go back to the Net setup page.
      mbox (h, IDS_GET_SITELIST_ERROR, MB_OK);

      // Tell the progress page that we're done downloading
      Progress.PostMessageNow (WM_APP_SITE_INFO_DOWNLOAD_COMPLETE, 0, IDD_NET);
    }
    else 
    {
      downloaded = true;
      // Everything worked, go to the site select page
      // Tell the progress page that we're done downloading
      Progress.PostMessageNow (WM_APP_SITE_INFO_DOWNLOAD_COMPLETE, 0, IDD_SITE);
    }
  }
  TOPLEVEL_CATCH((HWND) context[1], "site");

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

static INT_PTR CALLBACK
drop_proc (HWND h, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
    {
      case WM_INITDIALOG:
        eset(h, IDC_DROP_MIRRORS, cache_warn_urls);
	/* Should this be set by default? */
	// CheckDlgButton (h, IDC_DROP_NOWARN, BST_CHECKED);
	SetFocus (GetDlgItem(h, IDC_DROP_NOWARN));
	return FALSE;
	break;
      case WM_COMMAND:
	switch (LOWORD (wParam))
	  {
	    case IDYES:
	      if (IsDlgButtonChecked (h, IDC_DROP_NOWARN) == BST_CHECKED)
	        EndDialog (h, CACHE_ACCEPT_NOWARN);
	      else
	        EndDialog (h, CACHE_ACCEPT_WARN);
	      break;

	    case IDNO:
	      EndDialog (h, CACHE_REJECT);
	      break;

	    default:
	      return 0;
	  }
	return TRUE;
	break;
      default:
	return FALSE;
    }
}

int check_dropped_mirrors (HWND h)
{
  cache_warn_urls = "";
  dropped_site_list.clear ();

  for (SiteList::const_iterator n = site_list.begin ();
       n != site_list.end (); ++n)
    {
      SiteList::iterator i = find (all_site_list.begin(), all_site_list.end(),
				   *n);
      if (i == all_site_list.end() || !i->from_mirrors_lst)
	{
	  SiteList::iterator j = find (cached_site_list.begin(),
				       cached_site_list.end(), *n);
	  if (j != cached_site_list.end())
	    {
	      Log (LOG_PLAIN) << "Dropped selected mirror: " << n->url
		  << endLog;
	      dropped_site_list.push_back (*j);
	      if (cache_warn_urls.size())
		cache_warn_urls += "\r\n";
	      cache_warn_urls += i->url;
	    }
	}
    }
  if (cache_warn_urls.size())
    {
      if (unattended_mode)
	return CACHE_ACCEPT_WARN;
      return DialogBox (hinstance, MAKEINTRESOURCE (IDD_DROPPED), h,
			drop_proc);
    }
  return CACHE_ACCEPT_NOWARN;
}

void write_cache_list (io_stream *f, const SiteList& theSites)
{
  for (SiteList::const_iterator n = theSites.begin ();
       n != theSites.end (); ++n)
    if (n->from_mirrors_lst)
      *f << (n->url + ";" + n->servername + ";" + n->area + ";"
	     + n->location);
}

void save_cache_file (int cache_action)
{
  io_stream *f = UserSettings::instance().open ("mirrors-lst");
  if (f)
    {
      write_cache_list (f, all_site_list);
      if (cache_action == CACHE_ACCEPT_WARN)
	{
	  Log (LOG_PLAIN) << "Adding dropped mirrors to cache to warn again."
	      << endLog;
	  *f << "# Following mirrors re-added by setup.exe to warn again about dropped urls.";
	  write_cache_list (f, dropped_site_list);
	}
      delete f;
    }
}

bool SitePage::Create ()
{
  return PropertyPage::Create (IDD_SITE);
}

void
SitePage::OnInit ()
{
    AddTooltip (IDC_EDIT_USER_URL, IDS_USER_URL_TOOLTIP);
}

long
SitePage::OnNext ()
{
  HWND h = GetHWND ();
  int cache_action = CACHE_ACCEPT_NOWARN;

  save_dialog (h);

  if (cache_is_usable && !(cache_action = check_dropped_mirrors (h)))
    return -1;

  if (cache_needs_writing)
    save_cache_file (cache_action);

  // Log all the selected URLs from the list.
  for (SiteList::const_iterator n = site_list.begin ();
       n != site_list.end (); ++n)
    Log (LOG_PLAIN) << "site: " << n->url << endLog;

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
  std::vector <int> sel_indicies;
  HWND listbox = GetDlgItem (IDC_URL_LIST);

  // Populate the list box with the URLs.
  SendMessage (listbox, LB_RESETCONTENT, 0, 0);
  for (SiteList::const_iterator i = all_site_list.begin ();
       i != all_site_list.end (); ++i)
    {
      // If selected, always show
      SiteList::iterator f = find (site_list.begin(), site_list.end(), *i);
      if (f == site_list.end())
        {
          // Otherwise, hide redundant legacy URLs:
          if (i->noshow)
            continue;
        }

      int j = SendMessage (listbox, LB_ADDSTRING, 0,
                           (LPARAM) i->displayed_url.c_str());
      // Set the ListBox item data to the index into all_site_list
      SendMessage (listbox, LB_SETITEMDATA, j, (i - all_site_list.begin()));

      // For every selected item, remember the index
      if (f != site_list.end())
        {
          sel_indicies.push_back(j);
        }
    }

  // Select the selected ones.
  for (std::vector <int>::const_iterator n = sel_indicies.begin ();
       n != sel_indicies.end (); ++n)
    {
      int index = *n;
      // Highlight the selected item
      SendMessage (listbox, LB_SELITEMRANGE, TRUE, (index << 16) | index);
      // Make sure it's fully visible
      SendMessage (listbox, LB_SETCARETINDEX, index, FALSE);
    }
}

bool SitePage::OnMessageCmd (int id, HWND hwndctl, UINT code)
{
  switch (id)
    {
    case IDC_EDIT_USER_URL:
      {
	// Set the default pushbutton to ADD if the user is entering text.
	if (code == EN_CHANGE)
	  SendMessage (GetHWND (), DM_SETDEFID, (WPARAM) IDC_BUTTON_ADD_URL, 0);
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
	    std::string other_url = egetString (GetHWND (), IDC_EDIT_USER_URL);
	    if (other_url.size())
	    {
	    site_list_type newsite (other_url, "", "", "", false);
	    SiteList::iterator i = find (all_site_list.begin(),
					 all_site_list.end(), newsite);
	    if (i == all_site_list.end())
	      {
		all_site_list.push_back (newsite);
		Log (LOG_BABBLE) << "Adding site: " << other_url << endLog;
		site_list.push_back (newsite);
	      }
	    else
	      site_list.push_back (*i);

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
