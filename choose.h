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
 * Written by Robert Collins <rbtcollins@hotmail.com>
 *
 */

#ifndef _CHOOSE_H_
#define _CHOOSE_H_

#include "proppage.h"

#define CATEGORY_EXPANDED  0
#define CATEGORY_COLLAPSED 1

#ifdef __cplusplus

class ChooserPage:public PropertyPage
{
public:
  ChooserPage ()
  {
  };
  virtual ~ ChooserPage ()
  {
  };

  virtual bool OnMessageCmd (int id, HWND hwndctl, UINT code);

  bool Create ();
  virtual void OnInit ();
  virtual long OnNext ();
  virtual long OnBack ();
  virtual void OnActivate ();
  virtual long OnUnattended ()
  {
    return OnNext ();
  }; 
};

#endif /* __cplusplus */
#endif /* _CHOOSE_H_ */
