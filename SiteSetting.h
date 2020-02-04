/*
 * Copyright (c) 2001, Robert Collins.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Robert Collins <rbtcollins@hotmail.com>
 *
 */

#ifndef SETUP_SITESETTING_H
#define SETUP_SITESETTING_H

#include <vector>

class SiteSetting
{
  public:
    SiteSetting ();
    void save ();
    ~SiteSetting ();
  private:
    bool saved;
    void getSavedSites();
    void registerSavedSite(char const *);
    const char *lastMirrorKey();
};

class site_list_type
{
public:
  site_list_type () : url (), displayed_url (), key () {};
  site_list_type (const std::string& , const std::string& ,
                  const std::string& , const std::string&, bool, bool = false,
                  const std::string& = "");
  ~site_list_type () {};
  std::string url;
  // provided by mirrors.lst but not used
  std::string servername;
  std::string area;
  std::string location;
  // did this site come from mirrors.lst?
  bool from_mirrors_lst;
  // marked as "noshow"
  bool noshow;
  // url to redirect to
  std::string redir;

  std::string displayed_url;
  // sort key
  std::string key;
  bool operator == (const site_list_type &) const;
  bool operator != (const site_list_type &) const;
  bool operator < (const site_list_type &) const;
  bool operator <= (const site_list_type &) const;
  bool operator > (const site_list_type &) const;
  bool operator >= (const site_list_type &) const;
};

typedef std::vector <site_list_type> SiteList;

void site_list_insert(SiteList &site_list, site_list_type newsite);

/* user chosen sites */
extern SiteList site_list;
/* potential sites */
extern SiteList all_site_list;

#endif /* SETUP_SITESETTING_H */
