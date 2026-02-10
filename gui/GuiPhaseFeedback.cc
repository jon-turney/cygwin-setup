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

#include "threebar.h"

#include "gui/GuiFeedback.h"
#include "resource.h"

extern ThreeBarProgressPage Progress;

void
GuiFeedback::phase_init(unsigned int id)
{
  Progress.SetText1 (id);
  Progress.SetText2 ("");
  Progress.SetText3 ("");
  Progress.SetBar1(0);
  Progress.SetBarLabel1(IDS_PROGRESS_PROGRESS);
}

void
GuiFeedback::phase_progress(int distance, int total) const
{
  Progress.SetBar1(distance, total);
}
