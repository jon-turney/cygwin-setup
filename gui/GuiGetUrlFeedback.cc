/*
 * Copyright (c) 2024 Jon Turney
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 */

#include "win32.h"
#include "commctrl.h"
#include "resource.h"
#include "diskfull.h"
#include "mount.h"
#include "threebar.h"
#include "msg.h"
#include "String++.h"

#include "gui/GuiFeedback.h"

extern ThreeBarProgressPage Progress;

void
GuiFeedback::fetch_set_total_length(long long int total_length)
{
  total_download_bytes = total_length;
  total_download_bytes_sofar = 0;
}

void
GuiFeedback::fetch_progress_disable(bool disable)
{
  is_local_install = disable;
}

void
GuiFeedback::fetch_init (const std::string &url, int length)
{
  if (is_local_install)
    return;

  std::string::size_type divide = url.find_last_of('/');
  max_bytes = length;
  Progress.SetText1(IDS_PROGRESS_DOWNLOADING);
  std::wstring fmt = LoadStringW(IDS_PROGRESS_DOWNLOADING_FROM);
  std::wstring s = format(fmt,
                          url.substr(divide + 1).c_str(),
                          url.substr(0, divide).c_str());
  Progress.SetText2(s.c_str());
  Progress.SetText3(IDS_PROGRESS_CONNECTING);
  Progress.SetBar1(0);
  start_tics = GetTickCount ();
}

void
GuiFeedback::fetch_set_length(int length)
{
  max_bytes = length;
}

void
GuiFeedback::fetch_progress (int bytes)
{
  if (is_local_install)
    return;

  static char buf[100];
  double kbps;
  static unsigned int last_tics = 0;
  DWORD tics = GetTickCount ();
  if (tics == start_tics)       // to prevent division by zero
    return;
  if (tics < last_tics + 200)   // to prevent flickering updates
    return;
  last_tics = tics;

  kbps = ((double)bytes) / (double)(tics - start_tics);
  if (max_bytes > 0)
    {
      int perc = (int)(100.0 * ((double)bytes) / (double)max_bytes);
      Progress.SetBar1(bytes, max_bytes);
      sprintf (buf, "%d %%  (%dk/%dk)  %03.1f kB/s",
               perc, bytes / 1000, max_bytes / 1000, kbps);
      if (total_download_bytes > 0)
          Progress.SetBar2(total_download_bytes_sofar + bytes,
                           total_download_bytes);
    }
  else
    sprintf (buf, "%d  %2.1f kB/s", bytes, kbps);

  Progress.SetText3(buf);
}

void
GuiFeedback::fetch_total_progress ()
{
  if (total_download_bytes > 0)
    {
      int df = diskfull (get_root_dir ().c_str());
      Progress.SetBar3(df);
    }
}

void
GuiFeedback::fetch_finish (int total_bytes)
{
  total_download_bytes_sofar += total_bytes;
  Progress.SetText3("");
}

void
GuiFeedback::fetch_fatal (const char *filename, const char *err)
{
  ::fatal (owner_window, IDS_ERR_OPEN_WRITE, filename, err);
}
