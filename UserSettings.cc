/*
 * Copyright (c) 2003, Robert Collins
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

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include "site.h"
#include "win32.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>
#include <algorithm>

#include "LogSingleton.h"
#include "io_stream.h"

#include "port.h"
#include "Exception.h"
#include "UserSettings.h"

using namespace std;

UserSettings &
UserSettings::Instance()
{
  return Instance_;
  if (Instance_.inited != 42)
      Instance_.init ();
  return Instance_;
}

void
UserSettings::init ()
{
  *this = UserSettings();
  inited = 42;
}

UserSettings UserSettings::Instance_;

void
UserSettings::registerSetting(UserSetting &aSetting)
{
  settings.push_back(&aSetting);
}

void
UserSettings::deRegisterSetting(UserSetting &aSetting)
{
  Settings::iterator i = find(settings.begin(), settings.end(), &aSetting);
  if (i == settings.end())
    throw new Exception ("__LINE__ __FILE__", String ("Attempt to deregister non registered setting!"), APPERR_LOGIC_ERROR);
  settings.erase(i);
}
