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

/* This is the implementation of the SplashPage class.  Since the splash page
 * has little to do, there's not much here. */

#include "setup_version.h"
#include "resource.h"
#include "splash.h"

#define SPLASH_URL "https://cygwin.com"
#define SPLASH_COPYRIGHT L"Copyright © 2000-2025"
#define SPLASH_TRANSLATE_URL "https://cygwin.com/setup/translate"

static ControlAdjuster::ControlInfo SplashControlsInfo[] = {
  { IDC_SPLASH_TEXT,        CP_STRETCH,   CP_STRETCH },
  { IDC_SPLASH_ICON,        CP_LEFT,      CP_TOP },
  { IDC_VERSION,            CP_LEFT,      CP_BOTTOM },
  { IDC_SPLASH_COPYR,       CP_LEFT,      CP_BOTTOM },
  { IDC_SPLASH_URL,         CP_LEFT,      CP_BOTTOM },
  { IDC_SPLASH_TRANSLATE,   CP_LEFT,      CP_BOTTOM },
  {0, CP_LEFT, CP_TOP}
};

SplashPage::SplashPage ()
{
  sizeProcessor.AddControlInfo (SplashControlsInfo);
}

bool
SplashPage::Create ()
{
  return PropertyPage::Create (IDD_SPLASH);
}

void
SplashPage::OnInit ()
{
  std::string ver;
  if (setup_version[0])
    {
      ver = setup_version;
      ver += " (" + machine_name(WindowsProcessMachine()) + ")";
    }

  ::SetWindowText (GetDlgItem (IDC_VERSION), ver.c_str());
  ::SetWindowTextW (GetDlgItem (IDC_SPLASH_COPYR), SPLASH_COPYRIGHT);
  ::SetWindowText (GetDlgItem (IDC_SPLASH_URL), SPLASH_URL);
  makeClickable (IDC_SPLASH_URL, SPLASH_URL);
  makeClickable (IDC_SPLASH_TRANSLATE, SPLASH_TRANSLATE_URL);
}
