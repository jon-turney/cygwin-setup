/*
 * Copyright (c) 2017 Jon Turney
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

#include <package_depends.h>
#include <LogSingleton.h>

void
dumpPackageDepends (PackageDepends const &currentList,
                    std::ostream &logger)
{
  Log (LOG_BABBLE) << "( ";
  PackageDepends::const_iterator i = currentList.begin();
  while (true)
    {
      if (i == currentList.end()) break;
      Log (LOG_BABBLE) << **i << " ";
      ++i;
    }
  Log (LOG_BABBLE) << ")";
}
