#ifndef CINSTALL_DESKTOP_H
#define CINSTALL_DESKTOP_H

/*
 * Copyright (c) 2001, Gary R. Van Sickle.
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

// This is the header for the DesktopSetupPage class.  Allows selection
// of "create desktop icon" and "add to start menu".

#include "proppage.h"

class DesktopSetupPage:public PropertyPage
{
public:
  DesktopSetupPage ()
  {
  };
  virtual ~ DesktopSetupPage ()
  {
  };

  bool Create ();

  virtual void OnInit ();
  virtual bool OnFinish ();
  virtual long OnBack ();
};

#endif // CINSTALL_DESKTOP_H
