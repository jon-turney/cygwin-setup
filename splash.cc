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

// This is the implementation of the SplashPage class.  Since the splash page
// has little to do, there's not much here.

#include <stdio.h>
#include "version.h"
#include "resource.h"
#include "cistring.h"
#include "splash.h"

bool
SplashPage::Create ()
{
  return PropertyPage::Create (IDD_SPLASH);
}

void
SplashPage::OnInit ()
{
  cistring ver;

  ver.Format (IDS_VERSION_INFO, version[0] ? version : "[unknown]");

  SetWindowText (GetDlgItem (IDC_VERSION), ver.c_str ());

  // Set the font for the IDC_STATIC_WELCOME_TITLE
  SetDlgItemFont(IDC_STATIC_WELCOME_TITLE, "Ariel", 12, FW_BOLD);
}
