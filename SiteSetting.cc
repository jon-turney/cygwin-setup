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

#include "io_stream.h"
#include "SiteSetting.h"
#include "UserSettings.h"

#include "getopt++/StringArrayOption.h"
#include "getopt++/BoolOption.h"
#include "resource.h"

#include <algorithm>
#include <string>
#include <vector>
#include <string.h>

StringArrayOption SiteOption('s', "site", IDS_HELPTEXT_SITE);
extern BoolOption UnsupportedOption;

/* Selected sites */
SiteList site_list;

/* Fresh mirrors + selected sites */
SiteList all_site_list;

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

site_list_type::site_list_type (const std::string &_url,
                                const std::string &_servername,
                                const std::string &_area,
                                const std::string &_location,
                                bool _from_mirrors_lst,
                                bool _noshow, /* default: false */
                                const std::string &_redir /* default: "" */)
{
  url = _url;
  servername = _servername;
  area = _area;
  location = _location;
  from_mirrors_lst = _from_mirrors_lst;
  noshow = _noshow;
  redir = _redir;

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
void
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
