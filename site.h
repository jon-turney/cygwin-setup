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

#ifndef _SITE_H_
#define _SITE_H_

/* required to parse this file */
#include <string.h>
#include <stdlib.h>
#include "list.h"

#include "proppage.h"

class SitePage:public PropertyPage
{
public:
  SitePage ()
  {
  };
  virtual ~ SitePage ()
  {
  };

  bool Create ();

  virtual void OnInit ();
  virtual void OnActivate ();
  virtual long OnNext ();
  virtual long OnBack ();

  virtual bool OnMessageCmd (int id, HWND hwndctl, UINT code);

  void PopulateListBox();
  void CheckControlsAndDisableAccordingly () const;
};

void do_download_site_info (HINSTANCE h, HWND owner);

class site_list_type
{
public:
  site_list_type ():url (0), displayed_url (0), key (0)
  {
  };
  site_list_type (char const *);
  /* workaround for missing placement new in gcc 2.95 */
  void init (char const *);
  ~site_list_type ()
  {
    if (url)
      free (url);
    if (displayed_url)
      free (displayed_url);
    if (key)
      free (key);
  };
  char *url;
  char *displayed_url;
  char *key;
};

/* user chosen sites */
extern list < site_list_type, const char *, strcasecmp > site_list;
/* potential sites */
extern list < site_list_type, const char *, strcasecmp > all_site_list;

void save_site_url ();

#endif /* _SITE_H_ */
