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
#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <ctype.h>

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
#include "choose.h"
#include "category.h"
#include "category_list.h"

#include "package_db.h"
#include "package_meta.h"
#include "package_version.h"

#include "port.h"

#define alloca __builtin_alloca

#define HMARGIN	10
#define ROW_MARGIN	5
#define ICON_MARGIN	4
#define NEW_COL_SIZE_SLOP (ICON_MARGIN + 11)

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

static int add_required (packagemeta & pkg);
static void set_view_mode (HWND h, views mode);

#define pkgtrustp(pkg,t) (packageversion *)(t==TRUST_PREV ? pkg->prev : t == TRUST_CURR ? pkg->curr : pkg->exp)

/* Set the next action given a current action.  */
static void
set_action (packagemeta * pkg, bool preinc)
{
/* actions are the following:

   for install modes (from net/local)
   for each version:
   install this version
   install the source for this version
   and a boolean flag - force install to allow reinstallation, or bypassing requirements
   globally:
   install the source for the current version.
   
   to uninstall a package, the desired version is set to NULL;

   for mirroring modes (download only)
   for each version
   download this version
   download source for this version

   these are represented by the following:
   the desired pointer in the packagemetadata indicated which version we are operating on.
   if we are operating on the installed version, reinstall is a valid option.
   for the selected version, forceinstall means Do an install no matter what, and
   srcpicked means download the source.

   The default action for any installed package is to install the 'curr version' 
   if it is not already installed.

   The default action for any non-installed package is to do nothing. 
   
   To achieve a no-op, set desired==installed, and if (installed) set forceinstall=0 and
   srcpicked = 0;

   Iteration through versions should follow the following rules:
   selected radio button (prev/curr/test) (show as reinstall if that is the
   current version) ->source only ->oldest version....skip version of radio button...
   newest version->uninstall->no-op->selected radio button.

   If any state cannot be set (ie because (say) no prev entry exists for a package
   simply progress to the next option.
   
   */

  /* We were set to uninstall the package */
  if (!pkg->desired && pkg->installed)
  {
    pkg->desired = pkg->installed;
    pkg->desired->binpicked = 0;
    pkg->desired->srcpicked = 0;
  } else if (pkg->desired == pkg->installed
      && (!pkg->installed
	  || !(pkg->installed->binpicked || pkg->installed->srcpicked)))
    {
      pkg->desired = NULL;
      /* No-op */
      pkg->desired = pkgtrustp (pkg, deftrust);
      if (pkg->desired)
      {
	pkg->desired->binpicked = 1;
        return;
      }
    }
  /* are we currently on the radio button selection */
  if (pkg->desired == pkgtrustp (pkg, deftrust) && pkg->installed)
	{
	  /* source only this file */
	  pkg->desired = pkg->installed;
	  pkg->desired->binpicked = 0;
	  pkg->desired->srcpicked = 1;
	  return;
	}
  /* are we currently on source only */
  else if (pkg->desired == pkg->installed && pkg->installed
	   && pkg->installed->srcpicked)
    {
      /* move onto the loop through versions */
      pkg->desired = pkg->versions[0];
      pkg->desired->binpicked = 1;
      pkg->desired->srcpicked = 0;
      return;
    }
  else
    {
      /* preserve the src tick box */
      int source = pkg->desired->srcpicked;
      /* bump the version selected, skipping the radio button trust along the way */
      size_t n;
      for (n = 0; n < pkg->versioncount && pkg->desired != pkg->versions[n];
	   n++);
      /* n points at pkg->desired */
      n++;
      if (n < pkg->versioncount)
	{
	  if (pkgtrustp (pkg, deftrust) == pkg->versions[n])
	    n++;
	  if (n < pkg->versioncount)
	    {
	      pkg->desired = pkg->versions[n];
	      pkg->desired->srcpicked = source;
	      return;
	    }
	}
      /* went past the end - uninstall the package */
      pkg->desired = NULL;
    }
}

static int
add_required (packagemeta & pkg)
{
  Dependency *dp;
  packagemeta *required;
  int changed = 0;
  if (!pkg.desired || (pkg.desired != pkg.installed && !pkg.desired->binpicked))
    /* uninstall || source only */
    return 0;

  dp = pkg.desired->required;
  packagedb db;
  while (dp)
    {
      if ((required = db.getpackagebyname (dp->package)) == NULL)
	{
	  dp = dp->next;
	  changed++;
	  continue;
	}
      if (!required->desired)
      {
	/* it's set to uninstall */
	set_action (required, 0);
      }
	else if (required->desired != required->installed && !required->desired->binpicked)
      {
	/* it's set to change to a different version source only */
	required->desired->binpicked = 1;
      }
	/* does this requirement have requirements? */
      changed += add_required (*required);
      dp = dp->next;
    }
  return changed;
}

/* Return an appropriate caption given the current action. */
char const *
choose_caption (packagemeta * pkg)
{
  if (!pkg->desired && pkg->installed)
    return "Uninstall";
  else if (!pkg->desired)
    return "Skip";
  else
    if (pkg->desired == pkg->installed && pkg->desired->binpicked)
      return source == IDC_SOURCE_DOWNLOAD ? "Retrieve" : "Reinstall";
    else
      if (pkg->desired == pkg->installed && pkg->desired->srcpicked)
	/* FIXME: Redo source should come up if the tarball is already present locally */
	return "Source";
      else if (pkg->desired == pkg->installed) /* and neither src nor bin */
	return "Keep";
      else
	return pkg->desired->Canonical_version ();
}

static void
paint (HWND hwnd)
{
  HDC hdc;
  PAINTSTRUCT ps;
  int x, y, i, ii;

  hdc = BeginPaint (hwnd, &ps);

  SelectObject (hdc, sysfont);
  SetBkColor (hdc, GetSysColor (COLOR_WINDOW));
  SetTextColor (hdc, GetSysColor (COLOR_WINDOWTEXT));

  RECT cr;
  GetClientRect (hwnd, &cr);

  POINT p;

  x = cr.left - scroll_ulc_x;
  y = cr.top - scroll_ulc_y + header_height;


  for (i = 0; i <= chooser->last_col; i++)
    {
      TextOut (hdc, x + chooser->headers[i].x, 3, chooser->headers[i].text,
	       chooser->headers[i].slen);
      MoveToEx (hdc, x + chooser->headers[i].x, header_height - 3, &p);
      LineTo (hdc, x + chooser->headers[i].x + chooser->headers[i].width,
	      header_height - 3);
    }

  IntersectClipRect (hdc, cr.left, cr.top + header_height, cr.right,
		     cr.bottom);

  for (ii = 0; ii < chooser->nlines; ii++)
    chooser->lines[ii].paint (hdc, x, y, ii,
			      (chooser->get_view_mode () ==
			       VIEW_CATEGORY) ? 0 : 1);

  if (chooser->nlines == 0)
    {
      static const char *msg = "Nothing to Install/Update";
      if (source == IDC_SOURCE_DOWNLOAD)
	msg = "Nothing to Download";
      TextOut (hdc, HMARGIN, header_height, msg, strlen (msg));
    }

  EndPaint (hwnd, &ps);
}

static void
scroll_common (HWND hwnd, int which, int *var, int code)
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
  ScrollWindow (hwnd, ox - scroll_ulc_x, oy - scroll_ulc_y, &sr, &sr);
  sr.bottom = sr.top;
  sr.top = cr.top;
  ScrollWindow (hwnd, ox - scroll_ulc_x, 0, &sr, &sr);
}

static LRESULT CALLBACK
list_vscroll (HWND hwnd, HWND hctl, UINT code, int pos)
{
  scroll_common (hwnd, SB_VERT, &scroll_ulc_y, code);
  return 0;
}

static LRESULT CALLBACK
list_hscroll (HWND hwnd, HWND hctl, UINT code, int pos)
{
  scroll_common (hwnd, SB_HORZ, &scroll_ulc_x, code);
  return 0;
}

static LRESULT CALLBACK
list_click (HWND hwnd, BOOL dblclk, int x, int y, UINT hitCode)
{
  int row, refresh;

  if (chooser->nlines == 0)
    return 0;

  if (y < header_height)
    return 0;
  x += scroll_ulc_x;
  y += scroll_ulc_y - header_height;

  row = (y + ROW_MARGIN / 2) / row_height;

  if (row < 0 || row >= chooser->nlines)
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

      si.nMax = chooser->nlines * row_height;
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
    return;
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
  for (packagemeta *pkg = db.getfirstpackage(); pkg; pkg = db.getnextpackage())
    if (!pkg->Categories().categories ())
      pkg->add_category (db.categories.register_category ("Misc"));
}

static void
default_trust (HWND h, trusts trust)
{
  deftrust = trust;
  packagedb db;
  for (packagemeta * pkg = db.getfirstpackage(); pkg; pkg = db.getnextpackage ())
      {
	if (pkg->installed || pkg->Categories ().getcategorybyname ("Base") 
	    || pkg->Categories ().getcategorybyname ("Misc"))
	pkg->desired = pkgtrustp (pkg, trust);
	else
	  pkg->desired = 0;
      }
  RECT r;
  GetClientRect (h, &r);
  InvalidateRect (h, &r, TRUE);
  if (nextbutton)
    SetFocus (nextbutton);
}

void
pick_line::set_line (packagemeta * _pkg)
{
  pkg = _pkg;
  cat = NULL;
}

void
pick_line::set_line (Category * _cat)
{
  cat = _cat;
  pkg = NULL;
}

void
pick_line::paint (HDC hdc, int x, int y, int row, int show_cat)
{
  int r = y + row * row_height;
  int by = r + tm.tmHeight - 11;
  if (pkg)
    {
      if (pkg->installed)
	{
	  TextOut (hdc, x + chooser->headers[chooser->current_col].x, r,
		   pkg->installed->Canonical_version (), strlen (pkg->installed->Canonical_version()));
	  SelectObject (bitmap_dc, bm_rtarrow);
	  BitBlt (hdc, x + chooser->headers[chooser->current_col].x
		  + chooser->headers[0].width + ICON_MARGIN / 2
		  + HMARGIN / 2, by, 11, 11, bitmap_dc, 0, 0, SRCCOPY);
	}

      const char *s = choose_caption (pkg);
      TextOut (hdc, x + chooser->headers[chooser->new_col].x
	       + NEW_COL_SIZE_SLOP, r, s, strlen (s));
      SelectObject (bitmap_dc, bm_spin);
      BitBlt (hdc, x + chooser->headers[chooser->new_col].x, by, 11, 11,
	      bitmap_dc, 0, 0, SRCCOPY);

      HANDLE check_bm;
      if (/* uninstall */!pkg->desired || 
	  /* source only */ (!pkg->desired->binpicked 
	    && pkg->desired->srcpicked) ||
	    /* when no source mirror available */ !pkg->desired->srcmirrorcount())
	check_bm = bm_checkna;
      else if (pkg->desired->srcpicked)
	check_bm = bm_checkyes;
      else
	check_bm = bm_checkno;

      SelectObject (bitmap_dc, check_bm);
      BitBlt (hdc, x + chooser->headers[chooser->src_col].x, by, 11, 11,
	      bitmap_dc, 0, 0, SRCCOPY);

      /* shows "first" category - do we want to show any? */
      if (pkg->Categories().getfirstcategory() && show_cat)
	TextOut (hdc, x + chooser->headers[chooser->cat_col].x, r,
		 pkg->Categories().getfirstcategory()->name, strlen (pkg->Categories().getfirstcategory()->name));

      if (!pkg->SDesc ())
	s = pkg->name;
      else
	{
	  static char buf[512];
	  strcpy (buf, pkg->name);
	  strcat (buf, ": ");
	  strcat (buf, pkg->SDesc ());
	  s = buf;
	}
      TextOut (hdc, x + chooser->headers[chooser->pkg_col].x, r, s,
	       strlen (s));
    }
  else if (cat)
    TextOut (hdc, x + chooser->headers[chooser->cat_col].x, r, cat->name,
	     strlen (cat->name));
}

int
pick_line::click (int x)
{
  if (pkg)
    {
      if (pkg->desired && pkg->desired->srcmirrorcount ()
	  && x >= chooser->headers[chooser->src_col].x - HMARGIN / 2
	  && x <= chooser->headers[chooser->src_col + 1].x - HMARGIN / 2)
	pkg->desired->srcpicked ^= 1;

      if (x >= chooser->headers[chooser->new_col].x - (HMARGIN / 2)
	  && x <= chooser->headers[chooser->new_col + 1].x - HMARGIN / 2)
	{
	  set_action (pkg, 1);
	  /* Add any packages that are needed by this package */
	  return add_required (*pkg);
	}
    }
  else if (cat)
    {
      /* handle the catalog being clicked ... does this belong up a level.. ? */
    }

  return 0;
}

_view::_view (views _mode, HDC dc)
{
  lines = NULL;
  nlines = 0;
  view_mode = VIEW_PACKAGE;
  set_headers ();
  init_headers (dc);
  view_mode = VIEW_CATEGORY;
  set_headers ();
  init_headers (dc);

  view_mode = _mode;
  set_headers ();
}

void
_view::set_view_mode (views _mode)
{
  if (_mode == NVIEW)
    view_mode = VIEW_PACKAGE_FULL;
  else
    view_mode = _mode;
  set_headers ();
}

const char *
_view::mode_caption ()
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

void
_view::set_headers (void)
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
    default:
      return;
    }
}

void
_view::init_headers (HDC dc)
{
  int i;

  for (i = 0; headers[i].text; i++)
    headers[i].width = 0;

  for (i = 0; headers[i].text; i++)
    note_width (headers, dc, headers[i].text, 0, i);
  packagedb db;
  for (packagemeta * pkg = db.getfirstpackage(); pkg; pkg = db.getnextpackage())
    {
      if (pkg->installed)
	{
	  note_width (headers, dc, pkg->installed->Canonical_version (), 0, current_col);
	  note_width (headers, dc, pkg->installed->Canonical_version (), NEW_COL_SIZE_SLOP,
		      new_col);
	}
      for (size_t n = 0; n< pkg->versioncount; n++)
	note_width (headers, dc, pkg->versions[n]->Canonical_version (), NEW_COL_SIZE_SLOP, new_col);
      for (Category * cat = db.categories.getfirstcategory(); cat; cat = cat->next)
	note_width (headers, dc, cat->name, 0, cat_col);
      if (!pkg->SDesc())
	note_width (headers, dc, pkg->name, 0, pkg_col);
      else
	{
	  static char buf[512];
	  strcpy (buf, pkg->name);
	  strcat (buf, ": ");
	  strcat (buf, pkg->SDesc ());
	  note_width (headers, dc, buf, 0, pkg_col);
	}
    }
  note_width (headers, dc, "keep", NEW_COL_SIZE_SLOP, new_col);
  note_width (headers, dc, "uninstall", NEW_COL_SIZE_SLOP, new_col);

  headers[0].x = HMARGIN / 2;
  for (i = 1; i <= last_col; i++)
    headers[i].x = headers[i - 1].x + headers[i - 1].width + ((i == new_col) ?
							      NEW_COL_SIZE_SLOP
							      : 0) + HMARGIN;
}

void
_view::insert_pkg (packagemeta & pkg)
{
  pick_line line;
  line.set_line (&pkg);
  packagedb db;
  if (view_mode != VIEW_CATEGORY)
    {
      if (lines == NULL)
	{
	  lines =
	    (pick_line *) calloc (db.npackages() + db.categories.categories (),
				  sizeof (pick_line));
	  nlines = 0;
	  insert_at (0, line);
	}
      else
	insert_under (0, line);
    }
  else
    {
//      assert (lines); /* protect against a coding change in future */
      for (Category * cat = pkg.Categories().getfirstcategory(); cat; cat = cat->next)
	{
	  /* insert the package under this category in the list. If this category is not
	     visible, add it */
	  int n = 0;
	  while (n < nlines)
	    {
	      /* this should be a generic call to list_sort_cmp */
	      if (lines[n].get_category ()
		  && cat->name == lines[n].get_category ()->name)
		{
		  insert_under (n, line);
		  n = nlines;
		}
	      n++;
	    }
	  if (n == nlines)
	    {
	      /* the category wasn't visible - insert at the end */
	      insert_category (cat, CATEGORY_COLLAPSED);
	      insert_pkg (pkg);
	    }
	}
    }
}

void
_view::insert_category (Category * cat, int collapsed)
{
  pick_line line;
  line.set_line (cat);
  if (lines == NULL)
    {
      packagedb db;
      lines =
	(pick_line *) malloc ((db.npackages() + db.categories.categories ()) *
			      sizeof (pick_line));
      memset (lines, '\0',
	      (db.npackages() + db.categories.categories ()) * sizeof (pick_line));
      nlines = 0;
      insert_at (0, line);
      if (!collapsed)
	for (CategoryPackage * catpkg = cat->packages; catpkg;
	     catpkg = catpkg->next)
	  insert_pkg (catpkg->pkg);
    }
  else
    {
      int n = 0;
      while (n < nlines)
	{
	  /* this should be a generic call to list_sort_cmp */
	  if (lines[n].get_category ()
	      && strcasecmp (cat->name, lines[n].get_category ()->name) < 0)
	    {
	      insert_at (n, line);
	      if (!collapsed)
		for (CategoryPackage * catpkg = cat->packages; catpkg;
		     catpkg = catpkg->next)
		  insert_pkg (catpkg->pkg);
	      n = nlines;
	    }
	  else if (lines[n].get_category () == cat)
	    n = nlines;
	  n++;

	}
      if (n == nlines)
	{
	  /* insert at the end */
	  insert_at (n, line);
	  if (!collapsed)
	    for (CategoryPackage * catpkg = cat->packages; catpkg;
		 catpkg = catpkg->next)
	      insert_pkg (catpkg->pkg);
	}
    }
}

/* insert a new line at line n */
void
_view::insert_at (int n, pick_line line)
{
  if (n < 0 || n > nlines)
    return;
  memmove (&lines[n + 1], &lines[n], (nlines - n) * sizeof (pick_line));
  lines[n] = line;
  nlines++;
}

/* insert a new line in the chooser, at the next depth in from linen */
void
_view::insert_under (int linen, pick_line line)
{
  int n;
  /* special case - empty view */
  if (nlines == 0)
    {
      insert_at (0, line);
      return;
    }
  /* perhaps these two are equivalent. FIXME: check for potential insert_under (0,.. calls */
  else if (linen > nlines)
    {
      insert_at (nlines, line);
      return;
    }
  /* part 1 - find the appropriate bucket beginning */
  if (lines[linen].get_category ())
    {
      n = linen + 1;
    }
  else if (lines[linen].get_pkg ())
    {
      n = linen;
      /* walk up to the beginning of the bucket */
      while (n > 0 && lines[n - 1].get_pkg ())
	n--;
    }
  else
    {
      /* nlines != 0 and lines[linen] is not a category or a package! */
      return;
    }
  /* part 2 - insert in sorted order in the bucket */
  while (n < nlines)
    {
      if (lines[n].get_category () || (lines[n].get_pkg ()
				       && strcasecmp (line.get_pkg ()->name,
						      lines[n].
						      get_pkg ()->name) < 0))
	{
	  insert_at (n, line);
	  n = nlines;
	}
      else if (lines[n].get_pkg () == line.get_pkg ())
	{
	  n = nlines;
	}
      n++;
    }
  if (n == nlines)
    {
      /* insert at the end of this bucket */
      insert_at (n, line);
    }
}

void
_view::clear_view (void)
{
  nlines = 0;
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
_view::click (int row, int x)
{
  if (row > nlines)
    return 0;
  if (lines[row].get_pkg ())
    return lines[row].click (x);
  else
    {
      /* if we are the last line or the next line is a category too, expand */
      if (row == (nlines - 1) || lines[row + 1].get_category ())
	{
	  int count = nlines;
	  for (CategoryPackage * catpkg =
	       lines[row].get_category ()->packages; catpkg;
	       catpkg = catpkg->next)
	    {
	      packagemeta &pkg = catpkg->pkg;
	      int n = row + 1;
	      pick_line line;
	      line.set_line (&pkg);
	      /* this is a nasty hack. It will go away when the hierarchy is coded */
	      while (n < nlines)
		{
		  if (lines[n].get_category () || (lines[n].get_pkg ()
						   && strcasecmp (pkg.name,
								  lines
								  [n].get_pkg
								  ()->name) <
						   0))
		    {
		      insert_at (n, line);
		      n = nlines;
		    }
		  else if (lines[n].get_pkg () == &pkg)
		    {
		      n = nlines;
		    }
		  n++;
		}
	      if (n == nlines)
		{
		  /* insert at the end of this category */
		  insert_at (n, line);
		  n = nlines + 1;
		}
	    }
	  return nlines - count;
	}
      else
	/* contract */
	{
	  int count = 0, n = row + 1;
	  while (n < nlines & !lines[n].get_category ())
	    {
	      memmove (&lines[n], &lines[n + 1],
		       (nlines - n) * sizeof (pick_line));
	      nlines--;
	      count++;
	    }
	  return -count;
	}
    }
}


static void
set_view_mode (HWND h, views mode)
{
  chooser->set_view_mode (mode);

  chooser->clear_view ();
#if 0
  for (packagemeta * pkg = package; pkg->name; pkg++)
    if (!pkg->exclude)
      set_action (pkg, 0);
#endif
  packagedb db;
  switch (chooser->get_view_mode ())
    {
    case VIEW_PACKAGE:
      for (packagemeta * pkg = db.getfirstpackage(); pkg; pkg = db.getnextpackage ())
	if ((!pkg->desired && pkg->installed) || pkg->desired->srcpicked || pkg->desired->binpicked)
	  chooser->insert_pkg (*pkg);
      break;
    case VIEW_PACKAGE_FULL:
      for (packagemeta * pkg = db.getfirstpackage(); pkg; pkg = db.getnextpackage ())
	  chooser->insert_pkg (*pkg);
      break;
    case VIEW_CATEGORY:
	/* start collapsed. TODO: make this a chooser flag */
	for (Category * cat = db.categories.getfirstcategory (); cat;
	     cat = cat->next)
	  chooser->insert_category (cat, CATEGORY_COLLAPSED);
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
  si.nMax =
    chooser->headers[chooser->last_col].x +
    chooser->headers[chooser->last_col].width + HMARGIN;
  si.nPage = r.right;
  SetScrollInfo (h, SB_HORZ, &si, TRUE);

  si.nMax = chooser->nlines * row_height;
  si.nPage = r.bottom - header_height;
  SetScrollInfo (h, SB_VERT, &si, TRUE);

  scroll_ulc_x = scroll_ulc_y = 0;

  InvalidateRect (h, &r, TRUE);

  if (nextbutton)
    SetFocus (nextbutton);
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
  HDC dc = GetDC (lv);
  sysfont = GetStockObject (DEFAULT_GUI_FONT);
  SelectObject (dc, sysfont);
  GetTextMetrics (dc, &tm);
  header_height = tm.tmHeight + 5 + 3;

  bitmap_dc = CreateCompatibleDC (dc);

  row_height = (tm.tmHeight + tm.tmExternalLeading + ROW_MARGIN);
  int irh = tm.tmExternalLeading + tm.tmDescent + 11 + ROW_MARGIN;
  if (row_height < irh)
    row_height = irh;

  chooser = new (view) (VIEW_CATEGORY, dc);

  default_trust (lv, TRUST_CURR);
  set_view_mode (lv, VIEW_CATEGORY);
  if (!SetDlgItemText (dlg, IDC_CHOOSE_VIEWCAPTION, chooser->mode_caption ()))
    log (LOG_BABBLE, "Failed to set View button caption %ld",
	 GetLastError ());
  packagedb db;
  for (packagemeta * pkg = db.getfirstpackage(); pkg; pkg = db.getnextpackage ())
    add_required (*pkg);
  /* FIXME: do we need to init the desired fields ? */
  static int ta[] = { IDC_CHOOSE_CURR, 0 };
  rbset (dlg, ta, IDC_CHOOSE_CURR);

  ReleaseDC (lv, dc);
}

static BOOL
dialog_cmd (HWND h, int id, HWND hwndctl, UINT code)
{
  packagedb db;
  switch (id)
    {
    case IDC_CHOOSE_PREV:
      default_trust (lv, TRUST_PREV);
      for (packagemeta * pkg = db.getfirstpackage(); pkg; pkg = db.getnextpackage ())
	add_required (*pkg);
      set_view_mode (lv, chooser->get_view_mode ());
      break;
    case IDC_CHOOSE_CURR:
      default_trust (lv, TRUST_CURR);
      for (packagemeta * pkg = db.getfirstpackage(); pkg; pkg = db.getnextpackage ())
	add_required (*pkg);
      set_view_mode (lv, chooser->get_view_mode ());
      break;
    case IDC_CHOOSE_EXP:
      default_trust (lv, TRUST_TEST);
      for (packagemeta * pkg = db.getfirstpackage(); pkg; pkg = db.getnextpackage ())
	add_required (*pkg);
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

char *
base (const char *s)
{
  if (!s)
    return 0;
  const char *rv = s;
  while (*s)
    {
      if ((*s == '/' || *s == ':' || *s == '\\') && s[1])
	rv = s + 1;
      s++;
    }
  return (char *) rv;
}

int
find_tar_ext (const char *path)
{
  char *end = strchr (path, '\0');
  /* check in longest first order */
  char *ext = strstr (path, ".tar.bz2");
  if (ext)
    return (end - ext) == 8 ? ext - path : 0;
  if ((ext = strstr (path, ".tar.gz")));
  return (end - ext) == 7 ? ext - path : 0;
  if ((ext = strstr (path, ".tar")));
  return (end - ext) == 4 ? ext - path : 0;
}

/* Parse a filename into package, version, and extension components. */
int
parse_filename (const char *in_fn, fileparse & f)
{
  char *p, *ver;
  char fn[strlen (in_fn) + 1];

  strcpy (fn, in_fn);
  int n = find_tar_ext (fn);

  if (!n)
    return 0;

  strcpy (f.tail, fn + n);
  fn[n] = '\0';
  f.pkg[0] = f.what[0] = '\0';
  p = base (fn);
  for (ver = p; *ver; ver++)
    if (*ver == '-' || *ver == '_')
      if (isdigit (ver[1]))
	{
	  *ver++ = 0;
	  strcpy (f.pkg, p);
	  break;
	}
      else if (strcasecmp (ver, "-src") == 0 ||
	       strcasecmp (ver, "-patch") == 0)
	{
	  *ver++ = 0;
	  strcpy (f.pkg, p);
	  strcpy (f.what, strlwr (ver));
	  strcpy (f.pkgtar, p);
	  strcat (f.pkgtar, f.tail);
	  ver = strchr (ver, '\0');
	  break;
	}

  if (!f.pkg[0])
    strcpy (f.pkg, p);

  if (!f.what[0])
    {
      int n;
      p = strchr (ver, '\0');
      strcpy (f.pkgtar, in_fn);
      if ((p -= 4) >= ver && strcasecmp (p, "-src") == 0)
	n = 4;
      else if ((p -= 2) >= ver && strcasecmp (p, "-patch") == 0)
	n = 6;
      else
	n = 0;
      if (n)
	{
	  strcpy (f.what, p + 1);
	  *p = '\0';
	  p = f.pkgtar + (p - fn) + n;
	  memmove (p - n, p, strlen (p));
	}
    }

  strcpy (f.ver, *ver ? ver : "0.0");
  return 1;
}

#if 0
/* Return a pointer to a package given the name. */
Package *
getpkgbyname (const char *pkgname)
{
  for (Package * pkg = package; pkg && pkg->name; pkg++)
    if (strcasecmp (pkg->name, pkgname) == 0)
      return pkg;

  return NULL;
}

/* Return a pointer to a category of a given package given the name. */
Category *
getpackagecategorybyname (Package * pkg, const char *categoryname)
{
  for (Category * cat = pkg->category; cat; cat = cat->next)
    if (strcasecmp (cat->name, categoryname) == 0)
      return cat;

  return NULL;
}
#endif


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
  pkg = db.getpackagebyname (f.pkg);
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
  for (size_t n = 0; n < pkg->versioncount; n++)
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
    else if (strcasecmp (f.ver, pkg->versions[n]->Canonicalversion ())< 0)
      /* try curr */
      if (!pkg->curr)
	pkg->curr = thenewver;
      else if (strcasecmp (f.ver, pkg->versions[n]->Canonicalversion ())< 0)
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

_Info::_Info (const char *_install, const char *_version, int _install_size,
	      const char *_source, int _source_size)
{
  memset (this, 0, sizeof (*this));
  install = strdup (_install);
  version = strdup (_version);
  install_size = _install_size;
  if (_source)
    {
      source = strdup (_source);
      source_size = _source_size;
    }
}

int
package_sort (const void *va, const void *vb)
{
  packagemeta *a = (packagemeta *) va;
  packagemeta *b = (packagemeta *) vb;
  return strcasecmp (a->name, b->name);
}

void
do_choose (HINSTANCE h)
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

  rv = DialogBox (h, MAKEINTRESOURCE (IDD_CHOOSE), 0, dialog_proc);
  if (rv == -1)
    fatal (IDS_DIALOG_FAILED);

  log (LOG_BABBLE, "Chooser results...");
  packagedb db;
  for (packagemeta * pkg = db.getfirstpackage(); pkg; pkg = db.getnextpackage ())
    {
//      static const char *infos[] = { "nada", "prev", "curr", "test" };
      const char *trust = ((pkg->desired == pkg->prev) ? "prev"
			   : (pkg->desired == pkg->curr) ? "curr"
			   : (pkg->desired == pkg->exp) ? "test" : "unknown");
      const char *action = choose_caption (pkg);
      const char *installed = pkg->installed ? pkg->installed->Canonical_version () : "none";

      log (LOG_BABBLE, "[%s] action=%s trust=%s installed=%s"
	   " src?=%s",
	   pkg->name, action, trust, installed, 
	   pkg->desired && pkg->desired->srcpicked ? "yes" : "no");
      if (pkg->Categories().categories ())
	{
	  /* List categories the package belongs to */
	  int categories_len = 0;
	  Category *cp;
	  for (cp = pkg->Categories().getfirstcategory(); cp; cp = cp->next)
	    if (cp->name)
	      categories_len += strlen (cp->name) + 2;

	  if (categories_len > 0)
	    {
	      char *categories = (char *) malloc (categories_len);
	      strcpy (categories, pkg->Categories().getfirstcategory()->name);
	      for (cp = pkg->Categories().getfirstcategory()->next; cp; cp = cp->next)
		if (cp->name)
		  {
		    strcat (categories, ", ");
		    strcat (categories, cp->name);
		  }
	      log (LOG_BABBLE, "     categories=%s", categories);
	      free (categories);
	    }
	}
      if (pkg->desired && pkg->desired->required)
	{
	  /* List other packages this package depends on */
	  int requires_len = 0;
	  Dependency *dp;
	  for (dp = pkg->desired->required; dp; dp = dp->next)
	    if (dp->package)
	      requires_len += strlen (dp->package) + 2;

	  if (requires_len > 0)
	    {
	      char *requires = (char *) malloc (requires_len);
	      strcpy (requires, pkg->desired->required->package);
	      for (dp = pkg->desired->required->next; dp; dp = dp->next)
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
