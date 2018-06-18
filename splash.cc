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
#include "ini.h"

static ControlAdjuster::ControlInfo SplashControlsInfo[] = {
  { IDC_SPLASH_TEXT,        CP_STRETCH,   CP_STRETCH },
  { IDC_SPLASH_ICON,        CP_LEFT,      CP_BOTTOM },
  { IDC_VERSION,            CP_LEFT,      CP_BOTTOM },
  { IDC_SPLASH_COPYR,       CP_LEFT,      CP_BOTTOM },
  { IDC_SPLASH_URL,         CP_LEFT,      CP_BOTTOM },
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
  std::string ver = "Setup version ";
  ver += (setup_version[0] ? setup_version : "[unknown]");
  ver += is_64bit ? " (64 bit)" : " (32 bit)";
  SetDlgItemFont(IDC_VERSION, "Arial", 10, FW_BOLD);
  ::SetWindowText (GetDlgItem (IDC_VERSION), ver.c_str());
  makeClickable (IDC_SPLASH_URL, "https://cygwin.com");
}
