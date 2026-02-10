/*
 * Copyright (c) 2026 Jon Turney
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
#include "String++.h"
#include <iostream>

void
CliFeedback::phase_init(unsigned int id)
{
  if (id != prev_id)
    {
      std::wstring s = LoadStringW(id);
      std::cout << wstring_to_string(s) << std::endl;

      prev_id = id;
    }
}

void
CliFeedback::phase_progress(int distance, int total) const
{
  int perc = (int)(100.0 * ((double) distance) / (double)total);
  printf ("%d %%  (%d/%d)", perc, distance, total);
}
