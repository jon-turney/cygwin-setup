/*
 * Copyright (c) 2000, 2001 Red Hat, Inc.
 * Copyright (c) 2003 Robert Collins <rbtcollins@hotmail.com>
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
#include <io.h>
#include <ctype.h>
#include <process.h>

#include "dialog.h"
#include "resource.h"
#include "state.h"
#include "msg.h"
#include "LogSingleton.h"
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

using namespace std;

extern ThreeBarProgressPage Progress;

static HWND lv;
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

  // XXX we need a method to queryt he database to see if more
  // than just one package has changed! Until then...
#if 0
  if (refresh)
    {
#endif
      RECT r;
      GetClientRect (lv, &r);
      SCROLLINFO si;
      memset (&si, 0, sizeof (si));
      si.cbSize = sizeof (si);
      si.fMask = SIF_ALL | SIF_DISABLENOSCROLL;	/* SIF_RANGE was giving strange behaviour */
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
#if 0
    }
  else
    {
      RECT rect;
      rect.left =
	chooser->headers[chooser->new_col].x - chooser->scroll_ulc_x;
      rect.right =
	chooser->headers[chooser->src_col + 1].x - chooser->scroll_ulc_x;
      rect.top =
	chooser->header_height + row * chooser->row_height -
	chooser->scroll_ulc_y;
      rect.bottom = rect.top + chooser->row_height;
      InvalidateRect (hwnd, &rect, TRUE);
    }
#endif
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
		si.fMask = SIF_ALL | SIF_DISABLENOSCROLL;
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
  /* binary packages */
  /* Remove packages that are in the db, not installed, and have no 
     mirror info and are not cached for both binary and source packages. */
  vector <packagemeta *>::iterator i = db.packages.begin ();
  while (i != db.packages.end ())
    {
      packagemeta & pkg = **i;
      if (!pkg.installed && !pkg.accessible() && 
	  !pkg.sourceAccessible() )
	{
	  packagemeta *pkgm = *i;
	  delete pkgm;
	  i = db.packages.erase (i);
	}
      else
	++i;
    }
#if 0
  /* remove any source packages which are not accessible */
  vector <packagemeta *>::iterator i = db.sourcePackages.begin();
  while (i != db.sourcePackages.end())
    {
      packagemeta & pkg = **i;
      if (!packageAccessible (pkg))
	{
	  packagemeta *pkgm = *i;
	  delete pkgm;
	  i = db.sourcePackages.erase (i);
	}
      else
	++i;
    }
#endif
}

static void
fill_missing_category ()
{
  packagedb db;
  for (vector <packagemeta *>::iterator i = db.packages.begin ();
       i != db.packages.end (); ++i)
    {
      packagemeta & pkg = **i;
      if (!pkg.categories.size ())
	pkg.add_category ("Misc");
      pkg.add_category ("All");
    }
}

static void
default_trust (HWND h, trusts trust)
{
  chooser->deftrust = trust;
  packagedb db;
  for (vector <packagemeta *>::iterator i = db.packages.begin ();
       i != db.packages.end (); ++i)
    {
      packagemeta & pkg = **i;
      if (pkg.installed
	  || pkg.categories.find ("Base") != pkg.categories.end ()
	  || pkg.categories.find ("Misc") != pkg.categories.end ())
	{
	  pkg.desired = pkg.trustp (trust);
	  if (pkg.desired)
	    pkg.desired.pick (pkg.desired.accessible() 
	      		      && pkg.desired != pkg.installed);
	}
      else
	pkg.desired = packageversion ();
    }
  RECT r;
  GetClientRect (h, &r);
  InvalidateRect (h, &r, TRUE);
  // and then do the same for categories with no packages.
  for (packagedb::categoriesType::iterator n = packagedb::categories.begin();
       n != packagedb::categories.end(); ++n)
    if (!n->second.size())
      {
	log (LOG_BABBLE) << "Removing empty category " << n->first << endLog;
        packagedb::categories.erase (n++);
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
      for (vector <packagemeta *>::iterator i = db.packages.begin ();
	   i != db.packages.end (); ++i)
	{
	  packagemeta & pkg = **i;
	  if ((!pkg.desired && pkg.installed)
	      || (pkg.desired && (pkg.desired.picked () 
		  || pkg.desired.sourcePackage().picked())))
	    chooser->insert_pkg (pkg);
	}
    }
  else if (chooser->get_view_mode () == PickView::views::PackageKeeps)
    {
      for (vector <packagemeta *>::iterator i = db.packages.begin ();
	   i != db.packages.end (); ++i)
	{
	  packagemeta & pkg = **i;
	  if (pkg.installed && pkg.desired && !pkg.desired.picked() 
	      && !pkg.desired.sourcePackage().picked())
	    chooser->insert_pkg (pkg);
	}
    }
  else if (chooser->get_view_mode () == PickView::views::PackageSkips)
    {
      for (vector <packagemeta *>::iterator i = db.packages.begin ();
	   i != db.packages.end (); ++i)
	{
	  packagemeta & pkg = **i;
	  if (!pkg.desired && !pkg.installed)
	    chooser->insert_pkg (pkg);
	}
    }
  else if (chooser->get_view_mode () == PickView::views::PackageFull)
    {
      for (vector <packagemeta *>::iterator i = db.packages.begin ();
	   i != db.packages.end (); ++i)
	chooser->insert_pkg (**i);
    }
  else if (chooser->get_view_mode () == PickView::views::Category)
    {
      /* start collapsed. TODO: make this a chooser flag */
      for (packagedb::categoriesType::iterator n 
	   = packagedb::categories.begin();
	   n != packagedb::categories.end(); ++n)
	chooser->insert_category (&*n, CATEGORY_COLLAPSED);
    }

  RECT r;
  GetClientRect (h, &r);
  SCROLLINFO si;
  memset (&si, 0, sizeof (si));
  si.cbSize = sizeof (si);
  si.fMask = SIF_ALL | SIF_DISABLENOSCROLL;
  si.nMin = 0;
  si.nMax = chooser->headers[chooser->last_col].x + chooser->headers[chooser->last_col].width;	// + HMARGIN;
  si.nPage = r.right;
  SetScrollInfo (h, SB_HORZ, &si, TRUE);

  si.nMax = chooser->contents.itemcount () * chooser->row_height;
  si.nPage = r.bottom - chooser->header_height;
  SetScrollInfo (h, SB_VERT, &si, TRUE);

  chooser->scroll_ulc_x = chooser->scroll_ulc_y = 0;

  InvalidateRect (h, &r, TRUE);
}

void
ChooserPage::createListview (HWND dlg, RECT * r)
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
  chooser =
    new PickView (PickView::views::Category, lv,
		  *db.categories.find("All"));

  default_trust (lv, TRUST_CURR);
  set_view_mode (lv, PickView::views::Category);
  if (!SetDlgItemText (dlg, IDC_CHOOSE_VIEWCAPTION, chooser->mode_caption ()))
    log (LOG_BABBLE) << "Failed to set View button caption %ld" << 
	 GetLastError () << endLog;
  for_each (db.packages.begin(), db.packages.end(), bind2nd(mem_fun(&packagemeta::set_requirements), chooser->deftrust));
  /* FIXME: do we need to init the desired fields ? */
  static int ta[] = { IDC_CHOOSE_KEEP, IDC_CHOOSE_PREV, IDC_CHOOSE_CURR, IDC_CHOOSE_EXP, 0 };
  rbset (dlg, ta, IDC_CHOOSE_CURR);
}

/* TODO: review ::overrides for possible consolidation */
void
ChooserPage::getParentRect (HWND parent, HWND child, RECT * r)
{
  POINT p;
  ::GetWindowRect (child, r);
  p.x = r->left;
  p.y = r->top;
  ::ScreenToClient (parent, &p);
  r->left = p.x;
  r->top = p.y;
  p.x = r->right;
  p.y = r->bottom;
  ::ScreenToClient (parent, &p);
  r->right = p.x;
  r->bottom = p.y;
}

bool
ChooserPage::Create ()
{
    return PropertyPage::Create (IDD_CHOOSE);
}

void
ChooserPage::setPrompt(char const *aString)
{
  ::SetWindowText (GetDlgItem (IDC_CHOOSE_INST_TEXT), aString);
}

void
ChooserPage::OnInit ()
{
  register_windows (GetInstance ());

  if (source == IDC_SOURCE_DOWNLOAD || source == IDC_SOURCE_CWD)
    packagemeta::ScanDownloadedFiles ();

  set_existence ();
  fill_missing_category ();

  if (source == IDC_SOURCE_DOWNLOAD)
    setPrompt("Select packages to download ");
  else
    setPrompt("Select packages to install ");
  RECT r;
  getParentRect (GetHWND (), GetDlgItem (IDC_LISTVIEW_POS), &r);
  r.top += 2;
  r.bottom -= 2;
  createListview (GetHWND (), &r);
}

void
ChooserPage::OnActivate()
{
    set_view_mode (lv, chooser->get_view_mode ());
}

void
ChooserPage::logResults()
{
  log (LOG_BABBLE) << "Chooser results..." << endLog;
  packagedb db;
  for_each (db.packages.begin (), db.packages.end (), mem_fun(&packagemeta::logSelectionStatus));
}

long
ChooserPage::OnNext ()
{
  logResults();

  if (source == IDC_SOURCE_CWD)
    {
      // Next, install
      Progress.SetActivateTask (WM_APP_START_INSTALL);
    }
  else
    {
      // Next, start download from internet
      Progress.SetActivateTask (WM_APP_START_DOWNLOAD);
    }
  return IDD_INSTATUS;
}

long
ChooserPage::OnBack ()
{
  if (source == IDC_SOURCE_CWD)
    return IDD_LOCAL_DIR;
  else
    return IDD_SITE;
}

void
ChooserPage::keepClicked()
{
  packagedb db;
  for (vector <packagemeta *>::iterator i = db.packages.begin ();
        i != db.packages.end (); ++i)
    {
      packagemeta & pkg = **i;
      pkg.desired = pkg.installed;
    }
  set_view_mode (lv, chooser->get_view_mode ());
}

template <trusts aTrust>
void
ChooserPage::changeTrust()
{
  default_trust (lv, aTrust);
  packagedb db;
  for_each(db.packages.begin(), db.packages.end(), SetRequirement(aTrust));
  set_view_mode (lv, chooser->get_view_mode ());
}

bool
ChooserPage::OnMessageCmd (int id, HWND hwndctl, UINT code)
{
  if (code != BN_CLICKED)
    {
      // Not a click notification, we don't care.
      return false;
    }

  switch (id)
    {
    case IDC_CHOOSE_KEEP:
       return ifChecked(id, &ChooserPage::keepClicked);
    case IDC_CHOOSE_PREV:
       return ifChecked(id, &ChooserPage::changeTrust<TRUST_PREV>);
    case IDC_CHOOSE_CURR:
       return ifChecked(id, &ChooserPage::changeTrust<TRUST_CURR>);
    case IDC_CHOOSE_EXP:
       return ifChecked(id, &ChooserPage::changeTrust<TRUST_TEST>);
    case IDC_CHOOSE_VIEW:
      set_view_mode (lv, ++chooser->get_view_mode ());
      if (!SetDlgItemText
        (GetHWND (), IDC_CHOOSE_VIEWCAPTION, chooser->mode_caption ()))
      log (LOG_BABBLE) << "Failed to set View button caption " << 
           GetLastError () << endLog;
      break;
      
    default:
      // Wasn't recognized or handled.
      return false;
    }

  // Was handled since we never got to default above.
  return true;
}
