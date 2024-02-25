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

#include "cli/CliFeedback.h"
#include "resource.h"
#include "String++.h"
#include <iostream>

void
CliFeedback::hash_init(const char *hashalg, const std::string &shortname)
{
  std::wstring fmt = LoadStringW(IDS_PROGRESS_CHECKING_HASH);
  std::wstring s = format(fmt, hashalg, shortname.c_str());
  std::cout << wstring_to_string(s) << std::endl;
}

void
CliFeedback::hash_progress(int bytes, int total_bytes)
{
  std::cout << bytes << "/" << total_bytes << "\r";
}
