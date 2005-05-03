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
PickPackageLine::DrawCheck (int const checked, HDC hdc, int const column, HRGN const clip, int const x, int const by)
{
  HANDLE check_bm;
  if (checked == 0)
    check_bm = theView.bm_checkna;
  else if (checked == 1)
    check_bm = theView.bm_checkyes;
  else if (checked == 2)
    check_bm = theView.bm_checkno;
  else
    return;
  
  SelectObject (theView.bitmap_dc, check_bm);
  IntersectClipRect (hdc, x + theView.headers[column].x, by,
		    x + theView.headers[column].x +
		    theView.headers[column].width, by + 11);
  BitBlt (hdc, x + theView.headers[column].x + HMARGIN / 2, by, 11,
	  11, theView.bitmap_dc, 0, 0, SRCCOPY);
  SelectClipRgn (hdc, clip);
}

void
PickPackageLine::paint (HDC hdc, int x, int y, int row, int show_cat)
{
  int r = y + row * theView.row_height;
  int rb = r + theView.tm.tmHeight;
  int by = rb - 11; // top of box images
  int oldDC = SaveDC (hdc);
  if (!oldDC)
    return;
  HRGN oldClip = CreateRectRgn (0, 0, 0, 0);
  if (GetRandomRgn (hdc, oldClip, SYSRGN) == -1)
    {
      RestoreDC (hdc, oldDC);
      return;
    }
  HRGN oldClip2;
  if (IsWindowsNT()) {
				  
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

  oldClip2 = ExtCreateRegion (NULL, regionsize, oldClipData);
			      }
  else 
    oldClip2 = oldClip;  
  
  SelectClipRgn (hdc, oldClip2);
  if (pkg.installed)
    {
      IntersectClipRect (hdc, x + theView.headers[theView.current_col].x,
			 r,
			 x + theView.headers[theView.current_col].x +
			 theView.headers[theView.current_col].width, rb);
      TextOut (hdc,
	       x + theView.headers[theView.current_col].x + HMARGIN / 2, r,
	       pkg.installed.Canonical_version ().cstr_oneuse(),
	       pkg.installed.Canonical_version ().size());
      SelectObject (theView.bitmap_dc, theView.bm_rtarrow);
      BitBlt (hdc, x + theView.headers[theView.new_col].x + HMARGIN / 2,
	      by, 11, 11, theView.bitmap_dc, 0, 0, SRCCOPY);
      SelectClipRgn (hdc, oldClip2);
    }

  String s = pkg.action_caption ();
  IntersectClipRect (hdc, x + theView.headers[theView.new_col].x,
		     r,
		     x + theView.headers[theView.new_col].x +
		     theView.headers[theView.new_col].width, rb);
  TextOut (hdc,
	   x + theView.headers[theView.new_col].x + HMARGIN / 2 +
	   NEW_COL_SIZE_SLOP, r, s.cstr_oneuse(), s.size());
  SelectObject (theView.bitmap_dc, theView.bm_spin);
  BitBlt (hdc,
	  x + theView.headers[theView.new_col].x + ICON_MARGIN / 2 +
	  RTARROW_WIDTH + HMARGIN / 2, by, 11, 11, theView.bitmap_dc, 0, 0,
	  SRCCOPY);
  SelectClipRgn (hdc, oldClip2);

  int checked;

  if (/* uninstall or skip */ !pkg.desired ||
      /* current version */ pkg.desired == pkg.installed ||
      /* no source */ !pkg.desired.accessible())
    checked = 0;
  else if (pkg.desired.picked())
    checked = 1;
  else
    checked = 2;
      
  DrawCheck (checked, hdc, theView.bintick_col, oldClip2, x, by);
  
  if ( /* uninstall */ !pkg.desired ||
      /* source only */ (!pkg.desired.picked()
			 && pkg.desired.sourcePackage().picked() && pkg.desired == pkg.installed) ||
      /* when no source mirror available */
      !pkg.desired.sourcePackage().accessible())
    checked = 0;
  else if (pkg.desired.sourcePackage().picked())
    checked = 1;
  else
    checked = 2;

  DrawCheck (checked, hdc, theView.srctick_col, oldClip2, x, by);

  /* shows "first" category - do we want to show any? */
  if (pkg.categories.size () && show_cat)
    {
      String catName = pkg.getReadableCategoryList();
      IntersectClipRect (hdc, x + theView.headers[theView.cat_col].x, r,
			 x + theView.headers[theView.cat_col].x +
			 theView.headers[theView.cat_col].width - HMARGIN / 2, rb);
      TextOut (hdc, x + theView.headers[theView.cat_col].x + HMARGIN / 2, r,
	       catName.cstr_oneuse(),
	       catName.size());
      SelectClipRgn (hdc, oldClip2);
    }

  s = pkg.name;
  if (pkg.SDesc ().size())
    s += String(": ") + pkg.SDesc ();
  IntersectClipRect (hdc, x + theView.headers[theView.pkg_col].x, r,
		     x + theView.headers[theView.pkg_col].x +
		     theView.headers[theView.pkg_col].width, rb);
  TextOut (hdc, x + theView.headers[theView.pkg_col].x + HMARGIN / 2, r, s.cstr_oneuse(),
	   s.size());
  DeleteObject (oldClip);
  DeleteObject (oldClip2);
  RestoreDC (hdc, oldDC);
}

int
PickPackageLine::click (int const myrow, int const ClickedRow, int const x)
{
  // assert (myrow == ClickedRow);
  if (pkg.desired.accessible ()
      && x >= theView.headers[theView.bintick_col].x - HMARGIN / 2
      && x <= theView.headers[theView.bintick_col + 1].x - HMARGIN / 2)
    pkg.desired.pick(!pkg.desired.picked());
  if (pkg.desired.sourcePackage ().accessible ()
      && x >= theView.headers[theView.srctick_col].x - HMARGIN / 2
      && x <= theView.headers[theView.srctick_col + 1].x - HMARGIN / 2)
    pkg.desired.sourcePackage().pick(!pkg.desired.sourcePackage().picked());

  if (x >= theView.headers[theView.new_col].x - HMARGIN / 2
      && x <= theView.headers[theView.new_col + 1].x - HMARGIN / 2)
    pkg.set_action (pkg.trustp(theView.deftrust));
  
  packagemeta::PrepareForVisit();
  /* Add any packages that are needed by this package */
  /* TODO: This hardcoded TRUST_CURR does not seem right. */
  return pkg.set_requirements (TRUST_CURR);
}

int PickPackageLine::set_action (packagemeta::_actions action)
{
  pkg.set_action (action , pkg.trustp(theView.deftrust));
  return pkg.set_requirements(theView.deftrust) + 1;
}
