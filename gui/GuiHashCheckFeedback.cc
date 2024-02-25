/*
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

#include "gui/GuiFeedback.h"
#include "resource.h"
#include "threebar.h"
#include "String++.h"

extern ThreeBarProgressPage Progress;

void
GuiFeedback::hash_init(const char *hashalg, const std::string &shortname)
{
  std::wstring fmt = LoadStringW(IDS_PROGRESS_CHECKING_HASH);
  std::wstring s = format(fmt, hashalg, shortname.c_str());
  Progress.SetText1(s.c_str());
  Progress.SetText4(IDS_PROGRESS_PROGRESS);
  Progress.SetBar1(0);
}

void
GuiFeedback::hash_progress(int bytes, int total_bytes)
{
  Progress.SetBar1(bytes, total_bytes);
}
