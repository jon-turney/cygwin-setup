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

#include "port.h"
#include "threebar.h"
#include "Generic.h"

using namespace std;

extern ThreeBarProgressPage Progress;


void
ChooserPage::fillMissingCategory ()
{
   packagedb db;
  for_each(db.packages.begin(), db.packages.end(), visit_if(mem_fun(&packagemeta::setDefaultCategories), mem_fun(&packagemeta::hasNoCategories)));
}

void
ChooserPage::setViewMode (PickView::views mode)
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
  ::GetClientRect (chooser->GetHWND(), &r);
  SCROLLINFO si;
  memset (&si, 0, sizeof (si));
  si.cbSize = sizeof (si);
  si.fMask = SIF_ALL | SIF_DISABLENOSCROLL;
  si.nMin = 0;
  si.nMax = chooser->headers[chooser->last_col].x + chooser->headers[chooser->last_col].width;	// + HMARGIN;
  si.nPage = r.right;
  SetScrollInfo (chooser->GetHWND(), SB_HORZ, &si, TRUE);

  si.nMax = chooser->contents.itemcount () * chooser->row_height;
  si.nPage = r.bottom - chooser->header_height;
  SetScrollInfo (chooser->GetHWND(), SB_VERT, &si, TRUE);

  chooser->scroll_ulc_x = chooser->scroll_ulc_y = 0;

  InvalidateRect (chooser->GetHWND(), &r, TRUE);
}

void
ChooserPage::createListview (HWND dlg, RECT * r)
{
  packagedb db;
  chooser = new PickView (*db.categories.find("All"));
  if (!chooser->Create(this, WS_CHILD | WS_HSCROLL | WS_VSCROLL | WS_VISIBLE,r))
    // TODO throw exception
    exit (11);
  chooser->init(PickView::views::Category);
  chooser->Show(SW_SHOW);

  chooser->defaultTrust (TRUST_CURR);
  setViewMode (PickView::views::Category);
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
  if (source == IDC_SOURCE_DOWNLOAD || source == IDC_SOURCE_CWD)
    packagemeta::ScanDownloadedFiles ();

  packagedb db;
  db.setExistence ();
  fillMissingCategory ();

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
    setViewMode (chooser->get_view_mode ());
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
  setViewMode (chooser->get_view_mode ());
}

template <trusts aTrust>
void
ChooserPage::changeTrust()
{
  chooser->defaultTrust (aTrust);
  packagedb db;
  for_each(db.packages.begin(), db.packages.end(), SetRequirement(aTrust));
  setViewMode (chooser->get_view_mode ());
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
      setViewMode (++chooser->get_view_mode ());
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
