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
#include "String++.h"

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
  site_list_type ():url (), displayed_url (0), key (0)
  {
  };
  site_list_type (String const &);
  /* workaround for missing placement new in gcc 2.95 */
  void init (String const &);
  ~site_list_type () {};
  String url;
  String displayed_url;
  String key;
};

/* user chosen sites */
extern list < site_list_type, String, String::casecompare > site_list;
/* potential sites */
extern list < site_list_type, String, String::casecompare > all_site_list;

void save_site_url ();

#endif /* _SITE_H_ */
