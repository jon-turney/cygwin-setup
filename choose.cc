/*
 * Copyright (c) 2000, 2001 Red Hat, Inc.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by DJ Delorie <dj@cygnus.com>
 *
 */

/* The purpose of this file is to let the user choose which packages
   to install, and which versions of the package when more than one
   version is provided.  The "trust" level serves as an indication as
   to which version should be the default choice.  At the moment, all
   we do is compare with previously installed packages to skip any
   that are already installed (by setting the action to ACTION_SAME).
   While the "trust" stuff is supported, it's not really implemented
   yet.  We always prefer the "current" option.  In the future, this
   file might have a user dialog added to let the user choose to not
   install packages, or to install packages that aren't installed by
   default. */

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include "win32.h"
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <ctype.h>
#include <process.h>

#include "dialog.h"
#include "resource.h"
#include "state.h"
#include "ini.h"
#include "concat.h"
#include "msg.h"
#include "log.h"
#include "find.h"
#include "filemanip.h"
#include "io_stream.h"
#include "propsheet.h"
#include "choose.h"
#include "category.h"

#include "package_db.h"
#include "package_meta.h"
#include "package_version.h"

#include "port.h"
#include "threebar.h"
extern ThreeBarProgressPage Progress;

#define alloca __builtin_alloca

#define HMARGIN	10
#define ROW_MARGIN	5
#define ICON_MARGIN	4
#define RTARROW_WIDTH 11
#define SPIN_WIDTH 11
#define NEW_COL_SIZE_SLOP (ICON_MARGIN + SPIN_WIDTH + RTARROW_WIDTH)

#define CHECK_SIZE	11

static int initialized = 0;

static int scroll_ulc_x, scroll_ulc_y;

static HWND lv, nextbutton, choose_inst_text;
static TEXTMETRIC tm;
static int header_height;
static HANDLE sysfont;
static int row_height;
static HANDLE bm_spin, bm_rtarrow, bm_checkyes, bm_checkno, bm_checkna;
static HDC bitmap_dc;
static view *chooser = NULL;
static trusts deftrust = TRUST_UNKNOWN;

static struct _header pkg_headers[] = {
  {"Current", 7, 0, 0},
  {"New", 3, 0, 0},
  {"Src?", 4, 0, 0},
  {"Category", 8, 0, 0},
  {"Package", 7, 0, 0},
  {0, 0, 0, 0}
};

static struct _header cat_headers[] = {
  {"Category", 8, 0, 0},
  {"Current", 7, 0, 0},
  {"New", 3, 0, 0},
  {"Src?", 4, 0, 0},
  {"Package", 7, 0, 0},
  {0, 0, 0, 0}
};

static void set_view_mode (HWND h, views mode);

packageversion *
pkgtrustp (packagemeta const &pkg, trusts const t)
{
  return t == TRUST_PREV ? pkg.prev : t == TRUST_CURR ? pkg.curr : pkg.exp;
}

static int
add_required (packagemeta & pkg, size_t depth = 0)
{
  Dependency *dp;
  packagemeta *required;
  int changed = 0;
  if (!pkg.desired
      || (pkg.desired != pkg.installed && !pkg.desired->binpicked))
    /* uninstall || source only */
    return 0;

  dp = pkg.desired->required;
  packagedb db;
  /* cheap test for too much recursion */
  if (depth > 5)
    return 0;
  while (dp)
    {
      if ((required = db.packages.getbykey (dp->package)) == NULL)
	{
	  dp = dp->next;
	  changed++;
	  continue;
	}
      if (!required->desired)
	{
	  /* it's set to uninstall */
	  required->set_action (pkgtrustp (*required, deftrust));
	}
      else if (required->desired != required->installed
	       && !required->desired->binpicked)
	{
	  /* it's set to change to a different version source only */
	  required->desired->binpicked = 1;
	}
      /* does this requirement have requirements? */
      changed += add_required (*required, depth + 1);
      dp = dp->next;
    }
  return changed;
}

void
topbucket::paint (HDC hdc, int x, int y, int row, int show_cat)
{
  int accum_row = row;
  for (size_t n = 1; n <= bucket.number (); n++)
    {
      bucket[n]->paint (hdc, x, y, accum_row, show_cat);
      accum_row += bucket[n]->itemcount ();
    }
}

void
topbucket::empty (void)
{
  while (bucket.number ())
    {
      pick_line *line = bucket.removebyindex (1);
      delete line;
    }
}

topbucket::~topbucket (void)
{
  empty ();
}

int
topbucket::click (int const myrow, int const ClickedRow, int const x)
{
  int accum_row = myrow;
  for (size_t n = 1; n <= bucket.number (); n++)
    {
      accum_row += bucket[n]->itemcount ();
      if (accum_row > ClickedRow)
	return bucket[n]->click (accum_row - bucket[n]->itemcount (),
				 ClickedRow, x);
    }
  return 0;
}

static void
paint (HWND hwnd)
{
  HDC hdc;
  PAINTSTRUCT ps;
  int x, y;

  hdc = BeginPaint (hwnd, &ps);

  SelectObject (hdc, sysfont);
  SetBkColor (hdc, GetSysColor (COLOR_WINDOW));
  SetTextColor (hdc, GetSysColor (COLOR_WINDOWTEXT));

  RECT cr;
  GetClientRect (hwnd, &cr);

  x = cr.left - scroll_ulc_x;
  y = cr.top - scroll_ulc_y + header_height;

  IntersectClipRect (hdc, cr.left, cr.top + header_height, cr.right,
		     cr.bottom);

  chooser->contents.paint (hdc, x, y, 0, (chooser->get_view_mode () ==
					  VIEW_CATEGORY) ? 0 : 1);

  if (chooser->contents.itemcount () == 0)
    {
      static const char *msg = "Nothing to Install/Update";
      if (source == IDC_SOURCE_DOWNLOAD)
	msg = "Nothing to Download";
      TextOut (hdc, HMARGIN, header_height, msg, strlen (msg));
    }

  EndPaint (hwnd, &ps);
}

void
view::scroll (HWND hwnd, int which, int *var, int code)
{
  SCROLLINFO si;
  si.cbSize = sizeof (si);
  si.fMask = SIF_ALL;
  GetScrollInfo (hwnd, which, &si);

  switch (code)
    {
    case SB_THUMBTRACK:
      si.nPos = si.nTrackPos;
      break;
    case SB_THUMBPOSITION:
      break;
    case SB_BOTTOM:
      si.nPos = si.nMax;
      break;
    case SB_TOP:
      si.nPos = 0;
      break;
    case SB_LINEDOWN:
      si.nPos += row_height;
      break;
    case SB_LINEUP:
      si.nPos -= row_height;
      break;
    case SB_PAGEDOWN:
      si.nPos += si.nPage * 9 / 10;
      break;
    case SB_PAGEUP:
      si.nPos -= si.nPage * 9 / 10;
      break;
    }

  if ((int) si.nPos < 0)
    si.nPos = 0;
  if (si.nPos + si.nPage > (unsigned int) si.nMax)
    si.nPos = si.nMax - si.nPage;

  si.fMask = SIF_POS;
  SetScrollInfo (hwnd, which, &si, TRUE);

  int ox = scroll_ulc_x;
  int oy = scroll_ulc_y;
  *var = si.nPos;

  RECT cr, sr;
  GetClientRect (hwnd, &cr);
  sr = cr;
  sr.top += header_height;
  UpdateWindow (hwnd);
  ScrollWindow (hwnd, ox - scroll_ulc_x, oy - scroll_ulc_y, &sr, &sr);
  /*
     sr.bottom = sr.top;
     sr.top = cr.top;
     ScrollWindow (hwnd, ox - scroll_ulc_x, 0, &sr, &sr);
   */
  if (ox - scroll_ulc_x)
    {
      GetClientRect (listheader, &cr);
      sr = cr;
//  UpdateWindow (htmp);
      MoveWindow (listheader, -scroll_ulc_x, 0,
		  chooser->headers[last_col].x +
		  chooser->headers[last_col].width, header_height, TRUE);
    }
  UpdateWindow (hwnd);
}

static LRESULT CALLBACK
list_vscroll (HWND hwnd, HWND hctl, UINT code, int pos)
{
  chooser->scroll (hwnd, SB_VERT, &scroll_ulc_y, code);
  return 0;
}

static LRESULT CALLBACK
list_hscroll (HWND hwnd, HWND hctl, UINT code, int pos)
{
  chooser->scroll (hwnd, SB_HORZ, &scroll_ulc_x, code);
  return 0;
}

static LRESULT CALLBACK
list_click (HWND hwnd, BOOL dblclk, int x, int y, UINT hitCode)
{
  int row, refresh;

  if (chooser->contents.itemcount () == 0)
    return 0;

  if (y < header_height)
    return 0;
  x += scroll_ulc_x;
  y += scroll_ulc_y - header_height;

  row = (y + ROW_MARGIN / 2) / row_height;

  if (row < 0 || row >= chooser->contents.itemcount ())
    return 0;

  refresh = chooser->click (row, x);

  if (refresh)
    {
      RECT r;
      GetClientRect (lv, &r);
      SCROLLINFO si;
      memset (&si, 0, sizeof (si));
      si.cbSize = sizeof (si);
      si.fMask = SIF_ALL;	/* SIF_RANGE was giving strange behaviour */
      si.nMin = 0;

      si.nMax = chooser->contents.itemcount () * row_height;
      si.nPage = r.bottom - header_height;

      /* if we are under the minimum display count ,
       * set the offset to 0
       */
      if ((unsigned int) si.nMax <= si.nPage)
	scroll_ulc_y = 0;
      si.nPos = scroll_ulc_y;

      SetScrollInfo (lv, SB_VERT, &si, TRUE);

      InvalidateRect (lv, &r, TRUE);

    }
  else
    {
      RECT rect;
      rect.left = chooser->headers[chooser->new_col].x - scroll_ulc_x;
      rect.right = chooser->headers[chooser->src_col + 1].x - scroll_ulc_x;
      rect.top = header_height + row * row_height - scroll_ulc_y;
      rect.bottom = rect.top + row_height;
      InvalidateRect (hwnd, &rect, TRUE);
    }
  return 0;
}

static LRESULT CALLBACK
listview_proc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
    {
    case WM_HSCROLL:
      return HANDLE_WM_HSCROLL (hwnd, wParam, lParam, list_hscroll);
    case WM_VSCROLL:
      return HANDLE_WM_VSCROLL (hwnd, wParam, lParam, list_vscroll);
    case WM_LBUTTONDOWN:
      return HANDLE_WM_LBUTTONDOWN (hwnd, wParam, lParam, list_click);
    case WM_PAINT:
      paint (hwnd);
      return 0;
    case WM_NOTIFY:
      {
	// pnmh = (LPNMHDR) lParam
	LPNMHEADER phdr = (LPNMHEADER) lParam;
	switch (phdr->hdr.code)
	  {
	  case HDN_ITEMCHANGED:
	    if (phdr->hdr.hwndFrom == chooser->ListHeader ())
	      {
		if (phdr->pitem && phdr->pitem->mask & HDI_WIDTH)
		  chooser->headers[phdr->iItem].width = phdr->pitem->cxy;
		for (int i = 1; i <= chooser->last_col; i++)
		  chooser->headers[i].x =
		    chooser->headers[i - 1].x + chooser->headers[i - 1].width;
		RECT r;
		GetClientRect (hwnd, &r);
		SCROLLINFO si;
		si.cbSize = sizeof (si);
		si.fMask = SIF_ALL;
		GetScrollInfo (hwnd, SB_HORZ, &si);
		int oldMax = si.nMax;
		si.nMax =
		  chooser->headers[chooser->last_col].x +
		  chooser->headers[chooser->last_col].width;
		if (si.nTrackPos && oldMax > si.nMax)
		  si.nTrackPos += si.nMax - oldMax;
		si.nPage = r.right;
		SetScrollInfo (hwnd, SB_HORZ, &si, TRUE);
		InvalidateRect (hwnd, &r, TRUE);
		if (si.nTrackPos && oldMax > si.nMax)
		  chooser->scroll (hwnd, SB_HORZ, &scroll_ulc_x,
				   SB_THUMBTRACK);
	      }
	    break;
	  default:
	    break;
	  }
      }
    default:
      return DefWindowProc (hwnd, message, wParam, lParam);
    }
}

static void
register_windows (HINSTANCE hinst)
{
  WNDCLASSEX wcex;
  static int done = 0;

  if (done)
    return;
  done = 1;

  memset (&wcex, 0, sizeof (wcex));
  wcex.cbSize = sizeof (WNDCLASSEX);
  wcex.style = CS_HREDRAW | CS_VREDRAW;
  wcex.lpfnWndProc = listview_proc;
  wcex.hInstance = hinst;
  wcex.hIcon = LoadIcon (0, IDI_APPLICATION);
  wcex.hCursor = LoadCursor (0, IDC_ARROW);
  wcex.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
  wcex.lpszClassName = "listview";

  RegisterClassEx (&wcex);
}

static void
note_width (struct _header *hdrs, HDC dc, const char *string, int addend,
	    int column)
{
  if (!string)
    {
      if (hdrs[column].width < addend)
	hdrs[column].width = addend;
      return;
    }
  SIZE s;
  GetTextExtentPoint32 (dc, string, strlen (string), &s);
  if (hdrs[column].width < s.cx + addend)
    hdrs[column].width = s.cx + addend;
}

static void
set_existence ()
{
  /* FIXME:
     iterate through the package list, and delete packages that are
     * Not installed
     * have no mirror site
     and then do the same for categories with no packages.
   */
}

static void
fill_missing_category ()
{
  packagedb db;
  for (size_t n = 1; n < db.packages.number (); n++)
    {
      packagemeta & pkg = *db.packages[n];
      if (!pkg.Categories.number ())
	pkg.add_category (db.categories.registerbykey ("Misc"));
    }
}

static void
default_trust (HWND h, trusts trust)
{
  deftrust = trust;
  packagedb db;
  for (size_t n = 1; n < db.packages.number (); n++)
    {
      packagemeta & pkg = *db.packages[n];
      if (pkg.installed
	  || pkg.Categories.getbykey (db.categories.registerbykey ("Base"))
	  || pkg.Categories.getbykey (db.categories.registerbykey ("Misc")))
	{
	  pkg.desired = pkgtrustp (pkg, trust);
	  if (pkg.desired)
	    {
	      pkg.desired->binpicked = pkg.desired == pkg.installed ? 0 : 1;
	      pkg.desired->srcpicked = 0;
	    }
	}
      else
	pkg.desired = 0;
    }
  RECT r;
  GetClientRect (h, &r);
  InvalidateRect (h, &r, TRUE);
  if (nextbutton)
    SetFocus (nextbutton);
}

void
pick_pkg_line::paint (HDC hdc, int x, int y, int row, int show_cat)
{
  int r = y + row * row_height;
  int by = r + tm.tmHeight - 11;
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
  if (GetRegionData (oldClip, regionsize, oldClipData) != regionsize)
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
      IntersectClipRect (hdc, x + chooser->headers[chooser->current_col].x,
			 by,
			 x + chooser->headers[chooser->current_col].x +
			 chooser->headers[chooser->current_col].width,
			 by + 11);
      TextOut (hdc,
	       x + chooser->headers[chooser->current_col].x + HMARGIN / 2, r,
	       pkg.installed->Canonical_version (),
	       strlen (pkg.installed->Canonical_version ()));
      SelectObject (bitmap_dc, bm_rtarrow);
      BitBlt (hdc, x + chooser->headers[chooser->new_col].x + HMARGIN / 2,
	      by, 11, 11, bitmap_dc, 0, 0, SRCCOPY);
      SelectClipRgn (hdc, oldClip2);
    }

  const char *s = pkg.action_caption ();
  IntersectClipRect (hdc, x + chooser->headers[chooser->new_col].x,
		     by,
		     x + chooser->headers[chooser->new_col].x +
		     chooser->headers[chooser->new_col].width, by + 11);
  TextOut (hdc,
	   x + chooser->headers[chooser->new_col].x + HMARGIN / 2 +
	   NEW_COL_SIZE_SLOP, r, s, strlen (s));
  SelectObject (bitmap_dc, bm_spin);
  BitBlt (hdc,
	  x + chooser->headers[chooser->new_col].x + ICON_MARGIN / 2 +
	  RTARROW_WIDTH + HMARGIN / 2, by, 11, 11, bitmap_dc, 0, 0, SRCCOPY);
  SelectClipRgn (hdc, oldClip2);

  HANDLE check_bm;
  if ( /* uninstall */ !pkg.desired ||
      /* source only */ (!pkg.desired->binpicked
			 && pkg.desired->srcpicked) ||
      /* when no source mirror available */
      !pkg.desired->src.sites.number ())
    check_bm = bm_checkna;
  else if (pkg.desired->srcpicked)
    check_bm = bm_checkyes;
  else
    check_bm = bm_checkno;

  SelectObject (bitmap_dc, check_bm);
  IntersectClipRect (hdc, x + chooser->headers[chooser->src_col].x, by,
		     x + chooser->headers[chooser->src_col].x +
		     chooser->headers[chooser->src_col].width, by + 11);
  BitBlt (hdc, x + chooser->headers[chooser->src_col].x + HMARGIN / 2, by, 11,
	  11, bitmap_dc, 0, 0, SRCCOPY);
  SelectClipRgn (hdc, oldClip2);

  /* shows "first" category - do we want to show any? */
  if (pkg.Categories.number () && show_cat)
    {
      IntersectClipRect (hdc, x + chooser->headers[chooser->cat_col].x, by,
			 x + chooser->headers[chooser->cat_col].x +
			 chooser->headers[chooser->cat_col].x, by + 11);
      TextOut (hdc, x + chooser->headers[chooser->cat_col].x + HMARGIN / 2, r,
	       pkg.Categories[1]->key.name,
	       strlen (pkg.Categories[1]->key.name));
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
  IntersectClipRect (hdc, x + chooser->headers[chooser->pkg_col].x, by,
		     x + chooser->headers[chooser->pkg_col].x +
		     chooser->headers[chooser->pkg_col].width, by + 11);
  TextOut (hdc, x + chooser->headers[chooser->pkg_col].x + HMARGIN / 2, r, s,
	   strlen (s));
  DeleteObject (oldClip);
  DeleteObject (oldClip2);
  RestoreDC (hdc, oldDC);
}

void
pick_category_line::paint (HDC hdc, int x, int y, int row, int show_cat)
{
  int r = y + row * row_height;
  TextOut (hdc, x + chooser->headers[chooser->cat_col].x + HMARGIN / 2, r,
	   cat.name, strlen (cat.name));
  if (collapsed)
    return;
  int accum_row = row + 1;
  for (size_t n = 1; n <= bucket.number (); n++)
    {
      bucket[n]->paint (hdc, x, y, accum_row, show_cat);
      accum_row += bucket[n]->itemcount ();
    }
}

int
pick_pkg_line::click (int const myrow, int const ClickedRow, int const x)
{
  // assert (myrow == ClickedRow);
  if (pkg.desired && pkg.desired->src.sites.number ()
      && x >= chooser->headers[chooser->src_col].x - HMARGIN / 2
      && x <= chooser->headers[chooser->src_col + 1].x - HMARGIN / 2)
    pkg.desired->srcpicked ^= 1;

  if (x >= chooser->headers[chooser->new_col].x - HMARGIN / 2
      && x <= chooser->headers[chooser->new_col + 1].x - HMARGIN / 2)
    {
      pkg.set_action (pkgtrustp (pkg, deftrust));
      /* Add any packages that are needed by this package */
      return add_required (pkg);
    }
  return 0;
}

int
pick_category_line::click (int const myrow, int const ClickedRow, int const x)
{
  if (myrow == ClickedRow)
    {
      collapsed = !collapsed;
      int accum_row = 0;
      for (size_t n = 1; n <= bucket.number (); n++)
	accum_row += bucket[n]->itemcount ();
      return collapsed ? accum_row : -accum_row;
    }
  else
    {
      int accum_row = myrow + 1;
      for (size_t n = 1; n <= bucket.number (); n++)
	{
	  if (accum_row + bucket[n]->itemcount () > ClickedRow)
	    return bucket[n]->click (accum_row, ClickedRow, x);
	  accum_row += bucket[n]->itemcount ();
	}
      return 0;
    }
}

HWND DoCreateHeader (HWND hwndParent);

view::view (views _mode, HWND lv):listview (lv)
{

  HDC dc = GetDC (listview);
  sysfont = GetStockObject (DEFAULT_GUI_FONT);
  SelectObject (dc, sysfont);
  GetTextMetrics (dc, &tm);

  bitmap_dc = CreateCompatibleDC (dc);

  row_height = (tm.tmHeight + tm.tmExternalLeading + ROW_MARGIN);
  int
    irh =
    tm.
    tmExternalLeading +
    tm.
    tmDescent +
    11 +
    ROW_MARGIN;
  if (row_height < irh)
    row_height = irh;

  RECT rcParent;
  HDLAYOUT hdl;
  WINDOWPOS wp;

  // Ensure that the common control DLL is loaded, and then create
  // the header control.
  INITCOMMONCONTROLSEX controlinfo =
  {
  sizeof (INITCOMMONCONTROLSEX), ICC_LISTVIEW_CLASSES};
  InitCommonControlsEx (&controlinfo);

  if ((listheader = CreateWindowEx (0, WC_HEADER, (LPCTSTR) NULL,
				    WS_CHILD | WS_BORDER | CCS_NORESIZE |
				    // | HDS_BUTTONS
				    HDS_HORZ, 0, 0, 0, 0, listview,
				    (HMENU) IDC_CHOOSE_LISTHEADER, hinstance,
				    (LPVOID) NULL)) == NULL)
    // FIXME: throw an exception 
    exit (10);

  // Retrieve the bounding rectangle of the parent window's
  // client area, and then request size and position values
  // from the header control.
  GetClientRect (listview, &rcParent);

  hdl.prc = &rcParent;
  hdl.pwpos = &wp;
  if (!SendMessage (listheader, HDM_LAYOUT, 0, (LPARAM) & hdl))
    // FIXME: throw an exception
    exit (11);


  // Set the size, position, and visibility of the header control.
  SetWindowPos (listheader, wp.hwndInsertAfter, wp.x, wp.y,
		wp.cx, wp.cy, wp.flags | SWP_SHOWWINDOW);

  header_height = wp.cy;

  view_mode = VIEW_PACKAGE;
  set_headers ();
  init_headers (dc);
  view_mode = VIEW_CATEGORY;
  set_headers ();
  init_headers (dc);

  view_mode = _mode;
  set_headers ();

  ReleaseDC (lv, dc);
}

void
view::set_view_mode (views _mode)
{
  if (_mode == NVIEW)
    view_mode = VIEW_PACKAGE_FULL;
  else
    view_mode = _mode;
  set_headers ();
}

const char *
view::mode_caption ()
{
  switch (view_mode)
    {
    case VIEW_UNKNOWN:
      return "";
    case VIEW_PACKAGE_FULL:
      return "Full";
    case VIEW_PACKAGE:
      return "Partial";
    case VIEW_CATEGORY:
      return "Category";
    default:
      return "";
    }
}

int DoInsertItem (HWND hwndHeader, int iInsertAfter, int nWidth, LPSTR lpsz);

void
view::set_headers ()
{
  switch (view_mode)
    {
    case VIEW_UNKNOWN:
      return;
    case VIEW_PACKAGE_FULL:
    case VIEW_PACKAGE:
      headers = pkg_headers;
      current_col = 0;
      new_col = 1;
      src_col = 2;
      cat_col = 3;
      pkg_col = 4;
      last_col = 4;
      break;
    case VIEW_CATEGORY:
      headers = cat_headers;
      current_col = 1;
      new_col = 2;
      src_col = 3;
      cat_col = 0;
      pkg_col = 4;
      last_col = 4;
      break;
    default:
      return;
    }
  while (int n = SendMessage (listheader, HDM_GETITEMCOUNT, 0, 0))
    {
      SendMessage (listheader, HDM_DELETEITEM, n - 1, 0);
    }
  int i;
  for (i = 0; i <= last_col; i++)
    DoInsertItem (listheader, i, headers[i].width, (char *) headers[i].text);
}


void
view::init_headers (HDC dc)
{
  int i;

  for (i = 0; headers[i].text; i++)
    {
      headers[i].width = 0;
      headers[i].x = 0;
    }

  for (i = 0; headers[i].text; i++)
    note_width (headers, dc, headers[i].text, HMARGIN, i);
  /* src checkbox */
  note_width (headers, dc, 0, HMARGIN + 11, src_col);
  packagedb db;
  for (size_t n = 1; n < db.packages.number (); n++)
    {
      packagemeta & pkg = *db.packages[n];
      if (pkg.installed)
	note_width (headers, dc, pkg.installed->Canonical_version (),
		    HMARGIN, current_col);
      for (size_t n = 1; n <= pkg.versions.number (); n++)
	if (pkg.versions[n] != pkg.installed)
	  note_width (headers, dc,
		      pkg.versions[n]->Canonical_version (),
		      NEW_COL_SIZE_SLOP + HMARGIN, new_col);
      for (size_t n = 1; n <= db.categories.number (); n++)
	note_width (headers, dc, db.categories[n]->name, HMARGIN, cat_col);
      if (!pkg.SDesc ())
	note_width (headers, dc, pkg.name, HMARGIN, pkg_col);
      else
	{
	  static char buf[512];
	  strcpy (buf, pkg.name);
	  strcat (buf, ": ");
	  strcat (buf, pkg.SDesc ());
	  note_width (headers, dc, buf, HMARGIN, pkg_col);
	}
    }
  note_width (headers, dc, "keep", NEW_COL_SIZE_SLOP + HMARGIN, new_col);
  note_width (headers, dc, "uninstall", NEW_COL_SIZE_SLOP + HMARGIN, new_col);

  headers[0].x = 0;
  for (i = 1; i <= last_col; i++)
    headers[i].x = headers[i - 1].x + headers[i - 1].width;
}

void
view::insert_pkg (packagemeta & pkg)
{
  if (view_mode != VIEW_CATEGORY)
    {
      pick_pkg_line & line = *new pick_pkg_line (pkg);
      contents.insert (line);
    }
  else
    {
      for (size_t x = 1; x <= pkg.Categories.number (); x++)
	{
	  Category & cat = pkg.Categories[x]->key;
	  pick_category_line & catline = *new pick_category_line (cat);
	  pick_pkg_line & line = *new pick_pkg_line (pkg);
	  catline.insert (line);
	  contents.insert (catline);
	}
    }
}

void
view::insert_category (Category * cat, bool collapsed)
{

  pick_category_line & catline = *new pick_category_line (*cat, collapsed);
  for (CategoryPackage * catpkg = cat->packages; catpkg;
       catpkg = catpkg->next)
    {

      pick_pkg_line & line = *new pick_pkg_line (*catpkg->pkg);
      catline.insert (line);
    }
  contents.insert (catline);
}

void
view::clear_view (void)
{
  contents.empty ();
}

static views
viewsplusplus (views theview)
{
  switch (theview)
    {
    case VIEW_UNKNOWN:
      return VIEW_PACKAGE_FULL;
    case VIEW_PACKAGE_FULL:
      return VIEW_PACKAGE;
    case VIEW_PACKAGE:
      return VIEW_CATEGORY;
    case VIEW_CATEGORY:
      return NVIEW;
    default:
      return VIEW_UNKNOWN;
    }
}

int
view::click (int row, int x)
{
  return contents.click (0, row, x);
}

static void
set_view_mode (HWND h, views mode)
{
  chooser->set_view_mode (mode);

  chooser->clear_view ();
  packagedb db;
  switch (chooser->get_view_mode ())
    {
    case VIEW_PACKAGE:
      for (size_t n = 1; n < db.packages.number (); n++)
	{
	  packagemeta & pkg = *db.packages[n];
	  if ((!pkg.desired && pkg.installed)
	      || (pkg.desired
		  && (pkg.desired->srcpicked || pkg.desired->binpicked)))
	    chooser->insert_pkg (pkg);
	}
      break;
    case VIEW_PACKAGE_FULL:
      for (size_t n = 1; n < db.packages.number (); n++)
	{
	  packagemeta & pkg = *db.packages[n];
	  chooser->insert_pkg (pkg);
	}
      break;
    case VIEW_CATEGORY:
      /* start collapsed. TODO: make this a chooser flag */
      for (size_t n = 1; n <= db.categories.number (); n++)
	chooser->insert_category (db.categories[n], CATEGORY_COLLAPSED);
      break;
    default:
      break;
    }

  RECT r;
  GetClientRect (h, &r);
  SCROLLINFO si;
  memset (&si, 0, sizeof (si));
  si.cbSize = sizeof (si);
  si.fMask = SIF_ALL;
  si.nMin = 0;
  si.nMax = chooser->headers[chooser->last_col].x + chooser->headers[chooser->last_col].width;	// + HMARGIN;
  si.nPage = r.right;
  SetScrollInfo (h, SB_HORZ, &si, TRUE);

  si.nMax = chooser->contents.itemcount () * row_height;
  si.nPage = r.bottom - header_height;
  SetScrollInfo (h, SB_VERT, &si, TRUE);

  scroll_ulc_x = scroll_ulc_y = 0;

  InvalidateRect (h, &r, TRUE);

  if (nextbutton)
    SetFocus (nextbutton);
}

// DoInsertItem - inserts an item into a header control. 
// Returns the index of the new item. 
// hwndHeader - handle to the header control. 
// iInsertAfter - index of the previous item. 
// nWidth - width of the new item. 
// lpsz - address of the item string. 
int
DoInsertItem (HWND hwndHeader, int iInsertAfter, int nWidth, LPSTR lpsz)
{
  HDITEM hdi;
  int index;

  hdi.mask = HDI_TEXT | HDI_FORMAT | HDI_WIDTH;
  hdi.pszText = lpsz;
  hdi.cxy = nWidth;
  hdi.cchTextMax = lstrlen (hdi.pszText);
  hdi.fmt = HDF_LEFT | HDF_STRING;

  index = SendMessage (hwndHeader, HDM_INSERTITEM,
		       (WPARAM) iInsertAfter, (LPARAM) & hdi);

  return index;

}


static void
create_listview (HWND dlg, RECT * r)
{
  lv = CreateWindowEx (WS_EX_CLIENTEDGE,
		       "listview",
		       "listviewwindow",
		       WS_CHILD | WS_HSCROLL | WS_VSCROLL | WS_VISIBLE,
		       r->left, r->top,
		       r->right - r->left + 1, r->bottom - r->top + 1,
		       dlg,
		       (HMENU) MAKEINTRESOURCE (IDC_CHOOSE_LIST),
		       hinstance, 0);
  ShowWindow (lv, SW_SHOW);
  chooser = new view (VIEW_CATEGORY, lv);

  default_trust (lv, TRUST_CURR);
  set_view_mode (lv, VIEW_CATEGORY);
  if (!SetDlgItemText (dlg, IDC_CHOOSE_VIEWCAPTION, chooser->mode_caption ()))
    log (LOG_BABBLE, "Failed to set View button caption %ld",
	 GetLastError ());
  packagedb db;
  for (size_t n = 1; n < db.packages.number (); n++)
    {
      packagemeta & pkg = *db.packages[n];
      add_required (pkg);
    }
  /* FIXME: do we need to init the desired fields ? */
  static int ta[] = { IDC_CHOOSE_CURR, 0 };
  rbset (dlg, ta, IDC_CHOOSE_CURR);

}

static BOOL
dialog_cmd (HWND h, int id, HWND hwndctl, UINT code)
{
  packagedb db;
  switch (id)
    {
    case IDC_CHOOSE_PREV:
      default_trust (lv, TRUST_PREV);
      for (size_t n = 1; n < db.packages.number (); n++)
	{
	  packagemeta & pkg = *db.packages[n];
	  add_required (pkg);
	}
      set_view_mode (lv, chooser->get_view_mode ());
      break;
    case IDC_CHOOSE_CURR:
      default_trust (lv, TRUST_CURR);
      for (size_t n = 1; n < db.packages.number (); n++)
	{
	  packagemeta & pkg = *db.packages[n];
	  add_required (pkg);
	}
      set_view_mode (lv, chooser->get_view_mode ());
      break;
    case IDC_CHOOSE_EXP:
      default_trust (lv, TRUST_TEST);
      for (size_t n = 1; n < db.packages.number (); n++)
	{
	  packagemeta & pkg = *db.packages[n];
	  add_required (pkg);
	}
      set_view_mode (lv, chooser->get_view_mode ());
      break;
    case IDC_CHOOSE_VIEW:
      set_view_mode (lv, viewsplusplus (chooser->get_view_mode ()));
      if (!SetDlgItemText
	  (h, IDC_CHOOSE_VIEWCAPTION, chooser->mode_caption ()))
	log (LOG_BABBLE, "Failed to set View button caption %ld",
	     GetLastError ());
      break;

    case IDOK:
      if (source == IDC_SOURCE_CWD)
	NEXT (IDD_S_INSTALL);
      else
	NEXT (IDD_S_DOWNLOAD);
      break;

    case IDC_BACK:
      initialized = 0;
      if (source == IDC_SOURCE_CWD)
	NEXT (IDD_LOCAL_DIR);
      else
	NEXT (IDD_SITE);
      break;

    case IDCANCEL:
      NEXT (0);
      break;
    }
  return 0;
}

static void
GetParentRect (HWND parent, HWND child, RECT * r)
{
  POINT p;
  GetWindowRect (child, r);
  p.x = r->left;
  p.y = r->top;
  ScreenToClient (parent, &p);
  r->left = p.x;
  r->top = p.y;
  p.x = r->right;
  p.y = r->bottom;
  ScreenToClient (parent, &p);
  r->right = p.x;
  r->bottom = p.y;
}

static BOOL CALLBACK
dialog_proc (HWND h, UINT message, WPARAM wParam, LPARAM lParam)
{
  HWND frame;
  RECT r;
  switch (message)
    {
    case WM_INITDIALOG:
      nextbutton = GetDlgItem (h, IDOK);
      frame = GetDlgItem (h, IDC_LISTVIEW_POS);
      choose_inst_text = GetDlgItem (h, IDC_CHOOSE_INST_TEXT);
      if (source == IDC_SOURCE_DOWNLOAD)
	SetWindowText (choose_inst_text, "Select packages to download ");
      else
	SetWindowText (choose_inst_text, "Select packages to install ");
      GetParentRect (h, frame, &r);
      r.top += 2;
      r.bottom -= 2;
      create_listview (h, &r);

#if 0
      load_dialog (h);
#endif
      return TRUE;
    case WM_COMMAND:
      return HANDLE_WM_COMMAND (h, wParam, lParam, dialog_cmd);
    }
  return FALSE;
}

/* Find out where to put existing tar file in local directory in
   known package array. */
static void
scan2 (char *path, unsigned int size)
{
  packagemeta *pkg;
  fileparse f;

  if (!parse_filename (path, f))
    return;

  if (f.what[0] != '\0' && f.what[0] != 's')
    return;

  packagedb db;
  pkg = db.packages.getbykey (f.pkg);
  if (pkg == NULL)
    return;

  /* Scan existing package list looking for a match between a known
     package and a tar archive on disk.
     While scanning, keep track of appropriate "holes" in the trust
     table where a tar file could be put if no known entry
     exists.

     We have 4 specific insertion points and one generic point.
     The generic point is in versioned order in the package version array.
     The specific points are
     *installed
     *prev
     *curr
     *exp.

     if the version number matches a version in the db, 
     we simply add this as a mirror source to that version.
     If it matches no version, we add a new version to the db.

     Lastly if the version number does not matche one of installed/prev/current/exp
     AND we had to create a new version entry
     we apply the following heuristic:
     if there is no exp, we link this in exp.
     If there is an exp and this is higher, we link this in exp, and 
     if there is no curr, bump what was in exp to curr. If there was a curr, we leave it be.
     if this is lower than exp, and there is no curr, link as curr. If there is a curr, leave it be.
     If this is lower than curr, and there is no prev, link as prev, if there is a prev, leave it be.

     Whilst this logic is potentially wrong from time to time, it guarantees that
     setup.ini defined stability won't be altered unintentially. An alternative is to
     mark setup.ini defined prev/curr/exp packages as such, when this algorithm, can 
     get smarter.

     So, if setup.ini knows that ash-20010425-1.tar.gz is the current
     version and there is an ash-20010426-1.tar.gz in the current directory,
     the 20010426 version will be placed in the "test" slot, assuming that
     there is no test version listed in setup.ini. */

  int added = 0;
  for (size_t n = 1; n <= pkg->versions.number (); n++)
    {
      if (!strcasecmp (f.ver, pkg->versions[n]->Canonical_version ()))
	{
	  /* FIXME: Add a mirror entry */
	  added = 1;
	}
    }
  if (!added)
    {
      /* FIXME: Add a new version */

      /* And now the hole finder */
#if 0
      if (!pkg->exp)
	pkg->exp = thenewver;
      else if (strcasecmp (f.ver, pkg->versions[n]->Canonicalversion ()) < 0)
	/* try curr */
	if (!pkg->curr)
	  pkg->curr = thenewver;
	else if (strcasecmp (f.ver, pkg->versions[n]->Canonicalversion ()) <
		 0)
	  /* try prev */
	  if (!pkg->prev)
	    pkg->prev = thenewver;
#endif
    }

}

static void
scan_downloaded_files ()
{
  find (".", scan2);
}

void
do_choose (HINSTANCE h, HWND owner)
{
  int rv;

  nextbutton = 0;
  bm_spin = LoadImage (h, MAKEINTRESOURCE (IDB_SPIN), IMAGE_BITMAP, 0, 0, 0);
  bm_rtarrow = LoadImage (h, MAKEINTRESOURCE (IDB_RTARROW), IMAGE_BITMAP,
			  0, 0, 0);

  bm_checkyes = LoadImage (h, MAKEINTRESOURCE (IDB_CHECK_YES), IMAGE_BITMAP,
			   0, 0, 0);
  bm_checkno = LoadImage (h, MAKEINTRESOURCE (IDB_CHECK_NO), IMAGE_BITMAP,
			  0, 0, 0);
  bm_checkna = LoadImage (h, MAKEINTRESOURCE (IDB_CHECK_NA), IMAGE_BITMAP,
			  0, 0, 0);

  register_windows (h);

  if (source == IDC_SOURCE_DOWNLOAD || source == IDC_SOURCE_CWD)
    scan_downloaded_files ();

  set_existence ();
  fill_missing_category ();

  rv = DialogBox (h, MAKEINTRESOURCE (IDD_CHOOSE), owner, dialog_proc);
  if (rv == -1)
    fatal (owner, IDS_DIALOG_FAILED);

  log (LOG_BABBLE, "Chooser results...");
  packagedb db;
  for (size_t n = 1; n < db.packages.number (); n++)
    {
      packagemeta & pkg = *db.packages[n];
//      static const char *infos[] = { "nada", "prev", "curr", "test" };
      const char *trust = ((pkg.desired == pkg.prev) ? "prev"
			   : (pkg.desired == pkg.curr) ? "curr"
			   : (pkg.desired == pkg.exp) ? "test" : "unknown");
      const char *action = pkg.action_caption ();
      const char *installed =
	pkg.installed ? pkg.installed->Canonical_version () : "none";

      log (LOG_BABBLE, "[%s] action=%s trust=%s installed=%s"
	   " src?=%s",
	   pkg.name, action, trust, installed,
	   pkg.desired && pkg.desired->srcpicked ? "yes" : "no");
      if (pkg.Categories.number ())
	{
	  /* List categories the package belongs to */
	  int categories_len = 0;
	  for (size_t n = 1; n <= pkg.Categories.number (); n++)
	    categories_len += strlen (pkg.Categories[n]->key.name) + 2;

	  if (categories_len > 0)
	    {
	      char *categories = (char *) malloc (categories_len);
	      strcpy (categories, pkg.Categories[1]->key.name);
	      for (size_t n = 2; n <= pkg.Categories.number (); n++)
		{
		  strcat (categories, ", ");
		  strcat (categories, pkg.Categories[n]->key.name);
		}
	      log (LOG_BABBLE, "     categories=%s", categories);
	      free (categories);
	    }
	}
      if (pkg.desired && pkg.desired->required)
	{
	  /* List other packages this package depends on */
	  int requires_len = 0;
	  Dependency *dp;
	  for (dp = pkg.desired->required; dp; dp = dp->next)
	    if (dp->package)
	      requires_len += strlen (dp->package) + 2;

	  if (requires_len > 0)
	    {
	      char *requires = (char *) malloc (requires_len);
	      strcpy (requires, pkg.desired->required->package);
	      for (dp = pkg.desired->required->next; dp; dp = dp->next)
		if (dp->package)
		  {
		    strcat (requires, ", ");
		    strcat (requires, dp->package);
		  }
	      log (LOG_BABBLE, "     requires=%s", requires);
	      free (requires);
	    }
	}
#if 0

      /* FIXME: Reinstate this code, but spit out all mirror sites */

      for (int t = 1; t < NTRUST; t++)
	{
	  if (pkg->info[t].install)
	    log (LOG_BABBLE, "     [%s] ver=%s\n"
		 "          inst=%s %d exists=%s\n"
		 "          src=%s %d exists=%s",
		 infos[t],
		 pkg->info[t].version ? : "(none)",
		 pkg->info[t].install ? : "(none)",
		 pkg->info[t].install_size,
		 (pkg->info[t].install_exists) ? "yes" : "no",
		 pkg->info[t].source ? : "(none)",
		 pkg->info[t].source_size,
		 (pkg->info[t].source_exists) ? "yes" : "no");
	}
#endif
    }
}

#define WM_APP_START_CHOOSE        WM_APP+0
#define WM_APP_CHOOSE_IS_FINISHED  WM_APP+1

extern void do_choose (HINSTANCE h, HWND owner);

void
do_choose_thread (void *p)
{
  ChooserPage *cp;

  cp = static_cast < ChooserPage * >(p);

  do_choose (cp->GetInstance (), cp->GetHWND ());

  cp->PostMessage (WM_APP_CHOOSE_IS_FINISHED);

  _endthread ();
}

bool
ChooserPage::Create ()
{
  return PropertyPage::Create (IDD_CHOOSER);
}

void
ChooserPage::OnActivate ()
{
  GetOwner ()->SetButtons (0);
  PostMessage (WM_APP_START_CHOOSE);
}

bool
ChooserPage::OnMessageApp (UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
    {
    case WM_APP_START_CHOOSE:
      {
	// Start the chooser thread.
	_beginthread (do_choose_thread, 0, this);
	break;
      }
    case WM_APP_CHOOSE_IS_FINISHED:
      {
	switch (next_dialog)
	  {
	  case 0:
	    {
	      // Cancel
	      GetOwner ()->PressButton (PSBTN_CANCEL);
	      break;
	    }
	  case IDD_LOCAL_DIR:
	  case IDD_SITE:
	    {
	      // Back
	      GetOwner ()->SetActivePageByID (next_dialog);
	      break;
	    }
	  case IDD_S_DOWNLOAD:
	    {
	      // Next, start download from internet
	      Progress.SetActivateTask (WM_APP_START_DOWNLOAD);
	      GetOwner ()->SetActivePageByID (IDD_INSTATUS);
	      break;
	    }
	  case IDD_S_INSTALL:
	    {
	      // Next, install
	      Progress.SetActivateTask (WM_APP_START_INSTALL);
	      GetOwner ()->SetActivePageByID (IDD_INSTATUS);
	      break;
	    }
	  }
      }
    default:
      return false;
      break;
    }
  return true;
}
