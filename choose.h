/*
 * Copyright (c) 2000, Red Hat, Inc.
 * Copyright (c) 2003 Robert Collins <rbtcollins@hotmail.com>
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

#ifndef SETUP_CHOOSE_H
#define SETUP_CHOOSE_H

#include "proppage.h"
#include "package_meta.h"

#define CATEGORY_EXPANDED  0
#define CATEGORY_COLLAPSED 1

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
  private:
  void getParentRect (HWND parent, HWND child, RECT * r);
  void logOnePackageResult(packagemeta const *aPkg);
  void logResults();
  void keepClicked ();
  void setPrompt(char const *aPrompt);
  template<class C> bool ifChecked(int const &id, void (C::*fn)()) {
     if (IsDlgButtonChecked (GetHWND (), id)) {
       (this->*fn)();
       return true;
     }
    else
      return false;
  }
  template <trusts aTrust> void changeTrust();
};

#endif /* SETUP_CHOOSE_H */
