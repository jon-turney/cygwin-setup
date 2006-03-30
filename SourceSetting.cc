/*
 * Copyright (c) 2003, Robert Collins <rbtcollins@hotmail.com>
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Robert Collins.
 *
 */

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include "SourceSetting.h"
#include "UserSettings.h"
#include "io_stream.h"
#include "state.h"
#include "resource.h"
#include "String++.h"

void
SourceSetting::load()
{
  static int inited = 0;
  if (inited)
    return;
  io_stream *f = UserSettings::Instance().settingFileForLoad("last-action");
  if (f)
    {
      char localdir[1000];
      char *fg_ret = f->gets (localdir, 1000);
      delete f;
      if (fg_ret)
        source = sourceFromString(fg_ret);
    }
  inited = 1;
}

void
SourceSetting::save()
{
  
  io_stream *f = UserSettings::Instance().settingFileForSave("last-action");
  if (f)
    {
      switch (source) {
        case IDC_SOURCE_DOWNLOAD:
            f->write("Download\n",9);
            break;
        case IDC_SOURCE_NETINST:
            f->write("Download,Install\n",17);
            break;
        case IDC_SOURCE_CWD:
            f->write("Install\n",8);
            break;
        default:
            break;
      }
      delete f;
    }
}

int
SourceSetting::sourceFromString(String const & aSource)
{
  if (!casecompare(aSource, "Download"))
    return IDC_SOURCE_DOWNLOAD;
  if (!casecompare(aSource, "Download,Install"))
    return IDC_SOURCE_NETINST;
  if (!casecompare(aSource, "Install"))
    return IDC_SOURCE_CWD;

  /* A sanish default */
  return IDC_SOURCE_NETINST;
}
