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

#include "PickPackageLine.h"
#include "PickView.h"
#include "package_version.h"

void
PickPackageLine::paint (HDC hdc, int x, int y, int row, int show_cat)
{
  int r = y + row * theView.row_height;
  int by = r + theView.tm.tmHeight - 11;
  int oldDC = SaveDC (hdc);
  if (!oldDC)
    return;
  HRGN oldClip = CreateRectRgn (0, 0, 0, 0);
  if (GetRandomRgn (hdc, oldClip, SYSRGN) == -1)
    {
      RestoreDC (hdc, oldDC);
      return;
    }
  unsigned int regionsize = GetRegionData (oldClip, 0, 0);
  LPRGNDATA oldClipData = (LPRGNDATA) malloc (regionsize);
  if (GetRegionData (oldClip, regionsize, oldClipData) > regionsize)
    {
      RestoreDC (hdc, oldDC);
      DeleteObject (oldClip);
      return;
    }
  for (unsigned int n = 0; n < oldClipData->rdh.nCount; n++)
    for (unsigned int t = 0; t < 2; t++)
      ScreenToClient (WindowFromDC (hdc),
		      &((POINT *) oldClipData->Buffer)[t + n * 2]);

  HRGN oldClip2 = ExtCreateRegion (NULL, regionsize, oldClipData);
  SelectClipRgn (hdc, oldClip2);
  if (pkg.installed)
    {
      IntersectClipRect (hdc, x + theView.headers[theView.current_col].x,
			 by,
			 x + theView.headers[theView.current_col].x +
			 theView.headers[theView.current_col].width, by + 11);
      TextOut (hdc,
	       x + theView.headers[theView.current_col].x + HMARGIN / 2, r,
	       pkg.installed->Canonical_version (),
	       strlen (pkg.installed->Canonical_version ()));
      SelectObject (theView.bitmap_dc, theView.bm_rtarrow);
      BitBlt (hdc, x + theView.headers[theView.new_col].x + HMARGIN / 2,
	      by, 11, 11, theView.bitmap_dc, 0, 0, SRCCOPY);
      SelectClipRgn (hdc, oldClip2);
    }

  const char *s = pkg.action_caption ();
  IntersectClipRect (hdc, x + theView.headers[theView.new_col].x,
		     by,
		     x + theView.headers[theView.new_col].x +
		     theView.headers[theView.new_col].width, by + 11);
  TextOut (hdc,
	   x + theView.headers[theView.new_col].x + HMARGIN / 2 +
	   NEW_COL_SIZE_SLOP, r, s, strlen (s));
  SelectObject (theView.bitmap_dc, theView.bm_spin);
  BitBlt (hdc,
	  x + theView.headers[theView.new_col].x + ICON_MARGIN / 2 +
	  RTARROW_WIDTH + HMARGIN / 2, by, 11, 11, theView.bitmap_dc, 0, 0,
	  SRCCOPY);
  SelectClipRgn (hdc, oldClip2);

  HANDLE check_bm;
  if ( /* uninstall */ !pkg.desired ||
      /* source only */ (!pkg.desired->binpicked
			 && pkg.desired->srcpicked) ||
      /* when no source mirror available */
      !pkg.desired->src.sites.number ())
    check_bm = theView.bm_checkna;
  else if (pkg.desired->srcpicked)
    check_bm = theView.bm_checkyes;
  else
    check_bm = theView.bm_checkno;

  SelectObject (theView.bitmap_dc, check_bm);
  IntersectClipRect (hdc, x + theView.headers[theView.src_col].x, by,
		     x + theView.headers[theView.src_col].x +
		     theView.headers[theView.src_col].width, by + 11);
  BitBlt (hdc, x + theView.headers[theView.src_col].x + HMARGIN / 2, by, 11,
	  11, theView.bitmap_dc, 0, 0, SRCCOPY);
  SelectClipRgn (hdc, oldClip2);

  /* shows "first" category - do we want to show any? */
  if (pkg.Categories.number () && show_cat)
    {
      int index = 1;
      if (!strcasecmp (pkg.Categories[1]->key.name, "All"))
	index = 2;
      IntersectClipRect (hdc, x + theView.headers[theView.cat_col].x, by,
			 x + theView.headers[theView.cat_col].x +
			 theView.headers[theView.cat_col].x, by + 11);
      TextOut (hdc, x + theView.headers[theView.cat_col].x + HMARGIN / 2, r,
	       pkg.Categories[index]->key.name,
	       strlen (pkg.Categories[index]->key.name));
      SelectClipRgn (hdc, oldClip2);
    }

  if (!pkg.SDesc ())
    s = pkg.name;
  else
    {
      static char buf[512];
      strcpy (buf, pkg.name);
      strcat (buf, ": ");
      strcat (buf, pkg.SDesc ());
      s = buf;
    }
  IntersectClipRect (hdc, x + theView.headers[theView.pkg_col].x, by,
		     x + theView.headers[theView.pkg_col].x +
		     theView.headers[theView.pkg_col].width, by + 11);
  TextOut (hdc, x + theView.headers[theView.pkg_col].x + HMARGIN / 2, r, s,
	   strlen (s));
  DeleteObject (oldClip);
  DeleteObject (oldClip2);
  RestoreDC (hdc, oldDC);
}

int
PickPackageLine::click (int const myrow, int const ClickedRow, int const x)
{
  // assert (myrow == ClickedRow);
  if (pkg.desired && pkg.desired->src.sites.number ()
      && x >= theView.headers[theView.src_col].x - HMARGIN / 2
      && x <= theView.headers[theView.src_col + 1].x - HMARGIN / 2)
    pkg.desired->srcpicked ^= 1;

  if (x >= theView.headers[theView.new_col].x - HMARGIN / 2
      && x <= theView.headers[theView.new_col + 1].x - HMARGIN / 2)
    {
      pkg.set_action (pkg.trustp(theView.deftrust));
      /* Add any packages that are needed by this package */
      return pkg.set_requirements ();
    }
  return 0;
}

int PickPackageLine::set_action (packagemeta::_actions action)
{
  pkg.set_action (action , pkg.trustp(theView.deftrust));
  return pkg.set_requirements(theView.deftrust) + 1;
}
