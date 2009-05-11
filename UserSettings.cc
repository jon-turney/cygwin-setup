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
#include "Exception.h"
#include "UserSettings.h"
#include "io_stream.h"

/* for local_dir */
#include "state.h"
/* for deciding the location of saved files.. */
#include "mount.h"

using namespace std;

UserSettings &
UserSettings::Instance()
{
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
    throw new Exception (TOSTRING(__LINE__) " " __FILE__,
                         "Attempt to deregister non registered setting!",
                         APPERR_LOGIC_ERROR);
  settings.erase(i);
}

void
UserSettings::loadAllSettings()
{
  for_each(settings.begin(), settings.end(), mem_fun(&UserSetting::load)); 
}

void
UserSettings::saveAllSettings()
{
  for_each(settings.begin(), settings.end(), mem_fun(&UserSetting::save)); 
}

io_stream *
UserSettings::settingFileForLoad(const std::string& relativeName) const
{
  io_stream *result = io_stream::open("file://" + local_dir + "\\"
  						+ relativeName, "rt");
  if (!result)
    result = io_stream::open("cygfile:///etc/setup/" + relativeName, "rt");
  return result;
}

io_stream *
UserSettings::settingFileForSave(const std::string& relativeName) const
{
  // TODO: this doesn't belong here.
  io_stream::mkpath_p(PATH_TO_DIR, "file://" + local_dir, 0755);

  io_stream *result;
  // If cygwin's root is known.
  if (get_root_dir ().size())
    {
      result = io_stream::open("cygfile:///etc/setup/" + relativeName, "wb");
      io_stream::remove("file://" + local_dir + "\\" + relativeName);
    }
  else
    result = io_stream::open("file://" + local_dir + "\\" + relativeName, "wb");
  return result;
}
