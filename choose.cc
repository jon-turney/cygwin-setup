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

#include "PickView.h"

#include "port.h"
#include "threebar.h"
extern ThreeBarProgressPage Progress;

#define alloca __builtin_alloca

static int initialized = 0;

static HWND lv, nextbutton, choose_inst_text;
static PickView *chooser = NULL;

static void set_view_mode (HWND h, PickView::views mode);

static void
paint (HWND hwnd)
{
  HDC hdc;
  PAINTSTRUCT ps;
  int x, y;

  hdc = BeginPaint (hwnd, &ps);

  SelectObject (hdc, chooser->sysfont);
  SetBkColor (hdc, GetSysColor (COLOR_WINDOW));
  SetTextColor (hdc, GetSysColor (COLOR_WINDOWTEXT));

  RECT cr;
  GetClientRect (hwnd, &cr);

  x = cr.left - chooser->scroll_ulc_x;
  y = cr.top - chooser->scroll_ulc_y + chooser->header_height;

  IntersectClipRect (hdc, cr.left, cr.top + chooser->header_height, cr.right,
		     cr.bottom);

  chooser->contents.paint (hdc, x, y, 0, (chooser->get_view_mode () ==
					  PickView::views::Category) ? 0 : 1);

  if (chooser->contents.itemcount () == 0)
    {
      static const char *msg = "Nothing to Install/Update";
      if (source == IDC_SOURCE_DOWNLOAD)
	msg = "Nothing to Download";
      TextOut (hdc, HMARGIN, chooser->header_height, msg, strlen (msg));
    }

  EndPaint (hwnd, &ps);
}

static LRESULT CALLBACK
list_vscroll (HWND hwnd, HWND hctl, UINT code, int pos)
{
  chooser->scroll (hwnd, SB_VERT, &chooser->scroll_ulc_y, code);
  return 0;
}

static LRESULT CALLBACK
list_hscroll (HWND hwnd, HWND hctl, UINT code, int pos)
{
  chooser->scroll (hwnd, SB_HORZ, &chooser->scroll_ulc_x, code);
  return 0;
}

static LRESULT CALLBACK
list_click (HWND hwnd, BOOL dblclk, int x, int y, UINT hitCode)
{
  int row, refresh;

  if (chooser->contents.itemcount () == 0)
    return 0;

  if (y < chooser->header_height)
    return 0;
  x += chooser->scroll_ulc_x;
  y += chooser->scroll_ulc_y - chooser->header_height;

  row = (y + ROW_MARGIN / 2) / chooser->row_height;

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

      si.nMax = chooser->contents.itemcount () * chooser->row_height;
      si.nPage = r.bottom - chooser->header_height;

      /* if we are under the minimum display count ,
       * set the offset to 0
       */
      if ((unsigned int) si.nMax <= si.nPage)
	chooser->scroll_ulc_y = 0;
      si.nPos = chooser->scroll_ulc_y;

      SetScrollInfo (lv, SB_VERT, &si, TRUE);

      InvalidateRect (lv, &r, TRUE);

    }
  else
    {
      RECT rect;
      rect.left = chooser->headers[chooser->new_col].x - chooser->scroll_ulc_x;
      rect.right = chooser->headers[chooser->src_col + 1].x - chooser->scroll_ulc_x;
      rect.top = chooser->header_height + row * chooser->row_height - chooser->scroll_ulc_y;
      rect.bottom = rect.top + chooser->row_height;
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
		  chooser->scroll (hwnd, SB_HORZ, &chooser->scroll_ulc_x,
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
set_existence ()
{
  packagedb db;
  /* Remove packages that are in the db, not installed, and have no 
     mirror info. */
  size_t n = 1;
  while (n <= db.packages.number ())
    {
      packagemeta & pkg = *db.packages[n];
      bool mirrors = false;
      size_t o = 1;
      while (o <= pkg.versions.number () && !mirrors)
	{
	  packageversion & ver = *pkg.versions[o];
	  if (ver.bin.sites.number () || ver.bin.Cached () ||
	      ver.src.sites.number () || ver.src.Cached ())
	    mirrors = true;
	  ++o;
	}
      if (!pkg.installed && !mirrors)
        {
	  packagemeta * pkgm = db.packages.removebyindex (n);
	  delete pkgm;
	}
      else
	++n;
    }
}

static void
fill_missing_category ()
{
  packagedb db;
  for (size_t n = 1; n <= db.packages.number (); n++)
    {
      packagemeta & pkg = *db.packages[n];
      if (!pkg.Categories.number ())
	pkg.add_category (db.categories.registerbykey ("Misc"));
      pkg.add_category (db.categories.registerbykey ("All"));
    }
}

static void
default_trust (HWND h, trusts trust)
{
  chooser->deftrust = trust;
  packagedb db;
  for (size_t n = 1; n <= db.packages.number (); n++)
    {
      packagemeta & pkg = *db.packages[n];
      if (pkg.installed
	  || pkg.Categories.getbykey (db.categories.registerbykey ("Base"))
	  || pkg.Categories.getbykey (db.categories.registerbykey ("Misc")))
	{
	  pkg.desired = pkg.trustp (trust);
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
  // and then do the same for categories with no packages.
  size_t n = 1;
  while (n <= db.categories.number ())
    {
      if (!db.categories[n]->packages)
        {
           Category * cat = db.categories.removebyindex (n);
           delete cat;
        }
      else
	++n;
    }
}

static void
set_view_mode (HWND h, PickView::views mode)
{
  chooser->set_view_mode (mode);

  chooser->clear_view ();
  packagedb db;
  if (chooser->get_view_mode () == PickView::views::Package)
    {
      for (size_t n = 1; n <= db.packages.number (); n++)
	{
	  packagemeta & pkg = *db.packages[n];
	  if ((!pkg.desired && pkg.installed)
	      || (pkg.desired
		  && (pkg.desired->srcpicked || pkg.desired->binpicked)))
	    chooser->insert_pkg (pkg);
	}
    }
  else if (chooser->get_view_mode () == PickView::views::PackageFull)
    {
      for (size_t n = 1; n <= db.packages.number (); n++)
	{
	  packagemeta & pkg = *db.packages[n];
	  chooser->insert_pkg (pkg);
	}
    }
  else if (chooser->get_view_mode () == PickView::views::Category)
    {
      /* start collapsed. TODO: make this a chooser flag */
      for (size_t n = 1; n <= db.categories.number (); n++)
	chooser->insert_category (db.categories[n], CATEGORY_COLLAPSED);
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

  si.nMax = chooser->contents.itemcount () * chooser->row_height;
  si.nPage = r.bottom - chooser->header_height;
  SetScrollInfo (h, SB_VERT, &si, TRUE);

  chooser->scroll_ulc_x = chooser->scroll_ulc_y = 0;

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
  packagedb db;
  chooser = new PickView (PickView::views::Category, lv, db.categories.registerbykey("All"));

  default_trust (lv, TRUST_CURR);
  set_view_mode (lv, PickView::views::Category);
  if (!SetDlgItemText (dlg, IDC_CHOOSE_VIEWCAPTION, chooser->mode_caption ()))
    log (LOG_BABBLE, "Failed to set View button caption %ld",
	 GetLastError ());
  for (size_t n = 1; n <= db.packages.number (); n++)
    {
      packagemeta & pkg = *db.packages[n];
      pkg.set_requirements (chooser->deftrust);
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
      for (size_t n = 1; n <= db.packages.number (); n++)
	{
	  packagemeta & pkg = *db.packages[n];
	  pkg.set_requirements (TRUST_PREV);
	}
      set_view_mode (lv, chooser->get_view_mode ());
      break;
    case IDC_CHOOSE_CURR:
      default_trust (lv, TRUST_CURR);
      for (size_t n = 1; n <= db.packages.number (); n++)
	{
	  packagemeta & pkg = *db.packages[n];
	  pkg.set_requirements (TRUST_CURR);
	}
      set_view_mode (lv, chooser->get_view_mode ());
      break;
    case IDC_CHOOSE_EXP:
      default_trust (lv, TRUST_TEST);
      for (size_t n = 1; n <= db.packages.number (); n++)
	{
	  packagemeta & pkg = *db.packages[n];
	  pkg.set_requirements (TRUST_TEST);
	}
      set_view_mode (lv, chooser->get_view_mode ());
      break;
    case IDC_CHOOSE_VIEW:
      set_view_mode (lv, ++chooser->get_view_mode ());
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
  for (size_t n = 1; n <= db.packages.number (); n++)
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
	      char *categories = new char [categories_len];
	      strcpy (categories, pkg.Categories[1]->key.name);
	      for (size_t n = 2; n <= pkg.Categories.number (); n++)
		{
		  strcat (categories, ", ");
		  strcat (categories, pkg.Categories[n]->key.name);
		}
	      log (LOG_BABBLE, "     categories=%s", categories);
	      delete[] categories;
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
	      char *requires = new char [requires_len];
	      strcpy (requires, pkg.desired->required->package);
	      for (dp = pkg.desired->required->next; dp; dp = dp->next)
		if (dp->package)
		  {
		    strcat (requires, ", ");
		    strcat (requires, dp->package);
		  }
	      log (LOG_BABBLE, "     requires=%s", requires);
	      delete[] requires;
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
