/*
 * Copyright (c) 2001, 2002, 2003 Gary R. Van Sickle.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Gary R. Van Sickle <g.r.vansickle@worldnet.att.net>
 *
 */

#ifndef SETUP_PROPPAGE_H
#define SETUP_PROPPAGE_H

// This is the header for the PropertyPage class.  It works closely with the
// PropSheet class to implement a single page of the property sheet.


#include "win32.h"
#include <prsht.h>

#include "window.h"

class PropSheet;

class PropertyPage:public Window
{
  static bool DoOnceForSheet;
  PROPSHEETPAGE psp;
  DLGPROC proc;
    BOOL (*cmdproc) (HWND h, int id, HWND hwndctl, UINT code);

  // The sheet that owns this page.
  PropSheet *OurSheet;

  // For setting the back/finish buttons properly.
  bool IsFirst, IsLast;

  static BOOL CALLBACK FirstDialogProcReflector (HWND hwnd, UINT message,
						 WPARAM wParam,
						 LPARAM lParam);
  static BOOL CALLBACK DialogProcReflector (HWND hwnd, UINT message,
					    WPARAM wParam, LPARAM lParam);
  void setTitleFont ();

protected:
    virtual BOOL CALLBACK DialogProc (UINT message, WPARAM wParam,
				      LPARAM lParam);

public:
    PropertyPage ();
    virtual ~ PropertyPage ();

  PROPSHEETPAGE *GetPROPSHEETPAGEPtr ()
  {
    return &psp;
  };

  // FIXME: These should be private and friended to PropSheet.
  void YouAreBeingAddedToASheet (PropSheet * ps)
  {
    OurSheet = ps;
  };
  void YouAreFirst ()
  {
    IsFirst = true;
    IsLast = false;
  };
  void YouAreLast ()
  {
    IsFirst = false;
    IsLast = true;
  };
  void YouAreMiddle ()
  {
    IsFirst = false;
    IsLast = false;
  };

  virtual bool Create (int TemplateID);
  virtual bool Create (DLGPROC dlgproc, int TemplateID);
  virtual bool Create (DLGPROC dlgproc,
		       BOOL (*cmdproc) (HWND h, int id, HWND hwndctl,
					UINT code), int TemplateID);

  virtual void OnInit ()
  {
  };
  virtual void OnActivate ()
  {
  };
  virtual bool wantsActivation () const
  {
    return true;
  };
  virtual void OnDeactivate ()
  {
  };

  // Overload these to perform special processing when the user hits
  // "Next" or "Back". Return:
  // 0 == Go to next/previous page in sequence.
  // -1 == Prevent wizard from changing page.
  // Resource ID == go to a specific page specified by the resource ID.
  virtual long OnNext ()
  {
    return 0;
  };
  virtual long OnBack ()
  {
    return 0;
  };

  virtual bool OnFinish ()
  {
    return true;
  };
  virtual long OnUnattended ()
  {
    return -2;
  };

  PropSheet *GetOwner () const
  {
    return OurSheet;
  };
};

#endif /* SETUP_PROPPAGE_H */
