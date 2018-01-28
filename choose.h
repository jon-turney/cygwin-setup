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

#define DEFAULT_TIMER_ID   5   //value doesn't matter, as long as it's unique
#define SEARCH_TIMER_DELAY 500 //in milliseconds

extern bool hasManualSelections;

class ChooserPage:public PropertyPage
{
public:
  ChooserPage ();
  ~ChooserPage ();

  virtual bool OnMessageCmd (int id, HWND hwndctl, UINT code);
  virtual INT_PTR CALLBACK OnMouseWheel (UINT message, WPARAM wParam,
					 LPARAM lParam);
  virtual INT_PTR CALLBACK OnTimerMessage (UINT message, WPARAM wParam,
										   LPARAM lparam);

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
  void changeTrust(int button, bool test, bool initial);
  void logOnePackageResult(packagemeta const *aPkg);
  void logResults();
  void setPrompt(char const *aPrompt);
  void PlaceDialog (bool);
  void applyCommandLinePackageSelection();
  void initialUpdateState();

  PickView *chooser;
  static HWND ins_dialog;
  bool cmd_show_set;
  bool saved_geom;
  bool saw_geom_change;
  WINDOWPLACEMENT window_placement;
  WINDOWPLACEMENT pre_chooser_placement;
  UINT_PTR timer_id;
  union writer
  {
    WINDOWPLACEMENT wp;
    UINT wpi[sizeof (WINDOWPLACEMENT) / sizeof (UINT)];
  };
  int update_mode_id;
  bool activated;
};

#endif /* SETUP_CHOOSE_H */
