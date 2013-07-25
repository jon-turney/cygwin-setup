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
#include "PickView.h"

extern bool hasManualSelections;

class ChooserPage:public PropertyPage
{
public:
  ChooserPage ();
  ~ChooserPage ();

  virtual bool OnMessageCmd (int id, HWND hwndctl, UINT code);
  virtual INT_PTR CALLBACK OnMouseWheel (UINT message, WPARAM wParam,
					 LPARAM lParam);

  bool Create ();
  virtual void OnInit ();
  virtual long OnNext ();
  virtual long OnBack ();
  virtual void OnActivate ();
  virtual long OnUnattended ();

  static void SetHwndDialog (HWND h)
  {
    ins_dialog = h;
  }
private:
  void createListview ();
  RECT getDefaultListViewSize();
  void getParentRect (HWND parent, HWND child, RECT * r);
  void keepClicked();
  void changeTrust(trusts aTrust);
  void logOnePackageResult(packagemeta const *aPkg);
  void logResults();
  void setPrompt(char const *aPrompt);
  PickView *chooser;

  static HWND ins_dialog;
  bool cmd_show_set;
  bool saved_geom;
  bool saw_geom_change;
  WINDOWPLACEMENT window_placement;
  WINDOWPLACEMENT pre_chooser_placement;
  union writer
  {
    WINDOWPLACEMENT wp;
    UINT wpi[sizeof (WINDOWPLACEMENT) / sizeof (UINT)];
  };
  void PlaceDialog (bool);
};

#endif /* SETUP_CHOOSE_H */
