/*
 * Copyright (c) 2002 Robert Collins.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Robert Collins <robertc@hotmail.com>
 *
 */

#include "PickCategoryLine.h"
#include "PickView.h"

void
PickCategoryLine::empty (void)
{
  while (bucket.size ())
    {
      PickLine *line = *bucket.begin ();
      delete line;
      bucket.erase (bucket.begin ());
    }
}

void
PickCategoryLine::paint (HDC hdc, int x, int y, int row, int show_cat)
{
  int r = y + row * theView.row_height;
  if (show_label)
    {
      int by = r + theView.tm.tmHeight - 11;
      String temp=(String("+ ") +cat.first);
      TextOut (hdc,
	       x + theView.headers[theView.cat_col].x + HMARGIN / 2 +
	       depth * 8, r, temp.cstr_oneuse(), temp.size());
      if (!labellength)
	{
	  SIZE s;
	  GetTextExtentPoint32 (hdc, temp.cstr_oneuse(), temp.size(), &s);
	  labellength = s.cx;
	}
      SelectObject (theView.bitmap_dc, theView.bm_spin);
      BitBlt (hdc,
	      x + theView.headers[theView.cat_col].x +
	      labellength + depth * 8 +
	      ICON_MARGIN +
	      HMARGIN / 2, by, 11, 11, theView.bitmap_dc, 0, 0, SRCCOPY);
      TextOut (hdc,
	       x + theView.headers[theView.cat_col].x +
	       labellength + depth * 8 +
	       ICON_MARGIN + SPIN_WIDTH +
	       HMARGIN, r, current_default.caption (), strlen (current_default.caption ()));
    }
  if (collapsed)
    return;
  int accum_row = row + (show_label ? 1 : 0);
  for (size_t n = 0; n < bucket.size (); ++n)
    {
      bucket[n]->paint (hdc, x, y, accum_row, show_cat);
      accum_row += bucket[n]->itemcount ();
    }
}

int
PickCategoryLine::click (int const myrow, int const ClickedRow, int const x)
{
  if (myrow == ClickedRow && show_label)
    {
      if ((size_t) x >= theView.headers[theView.cat_col].x +
	  labellength + depth * 8 + ICON_MARGIN + HMARGIN / 2)
	{
	  ++current_default;
	  packagemeta::PrepareForVisit();
	  
	  return set_action (current_default);
	}
      else
	{
	  collapsed = !collapsed;
	  int accum_row = 0;
	  for (size_t n = 0; n < bucket.size (); ++n)
	    accum_row += bucket[n]->itemcount ();
	  return collapsed ? accum_row : -accum_row;
	}
    }
  else
    {
      int accum_row = myrow + (show_label ? 1 : 0);
      for (size_t n = 0; n < bucket.size (); ++n)
	{
	  if (accum_row + bucket[n]->itemcount () > ClickedRow)
	    return bucket[n]->click (accum_row, ClickedRow, x);
	  accum_row += bucket[n]->itemcount ();
	}
      return 0;
    }
}

int 
PickCategoryLine::set_action (packagemeta::_actions action)
{
  current_default = action;
  int accum_diff = 0;
  for (size_t n = 0; n < bucket.size (); n++)
      accum_diff += bucket[n]->set_action (current_default);
  return accum_diff;
}
