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
#include <stdlib.h>
#include <io.h>
#include <ctype.h>
#include <process.h>
#include <algorithm>

#include "dialog.h"
#include "resource.h"
#include "state.h"
#include "msg.h"
#include "LogSingleton.h"
#include "filemanip.h"
#include "io_stream.h"
#include "propsheet.h"
#include "choose.h"

#include "package_db.h"
#include "package_meta.h"
#include "package_version.h"

#include "threebar.h"
#include "Generic.h"
#include "ControlAdjuster.h"
#include "prereq.h"

#include "UserSettings.h"

#include "getopt++/BoolOption.h"
static BoolOption UpgradeAlsoOption (false, 'g', "upgrade-also", "also upgrade installed packages");
static BoolOption CleanOrphansOption (false, 'o', "delete-orphans", "remove orphaned packages");
static BoolOption MirrorOption (false, 'm', "mirror-mode", "Skip availability check when installing from local directory (requires local directory to be clean mirror!)");

using namespace std;

extern ThreeBarProgressPage Progress;

HWND ChooserPage::ins_dialog;

/*
  Sizing information.
 */
static ControlAdjuster::ControlInfo ChooserControlsInfo[] = {
  {IDC_CHOOSE_SEARCH_LABEL, 	CP_LEFT,    CP_TOP},
  {IDC_CHOOSE_SEARCH_EDIT,	CP_LEFT,    CP_TOP},
  {IDC_CHOOSE_KEEP, 		CP_RIGHT,   CP_TOP},
  {IDC_CHOOSE_CURR, 		CP_RIGHT,   CP_TOP},
  {IDC_CHOOSE_EXP, 		CP_RIGHT,   CP_TOP},
  {IDC_CHOOSE_VIEW, 		CP_RIGHT,   CP_TOP},
  {IDC_LISTVIEW_POS, 		CP_RIGHT,   CP_TOP},
  {IDC_CHOOSE_VIEWCAPTION,	CP_RIGHT,   CP_TOP},
  {IDC_CHOOSE_LIST,		CP_STRETCH, CP_STRETCH},
  {IDC_CHOOSE_HIDE,             CP_LEFT,    CP_BOTTOM},
  {0, CP_LEFT, CP_TOP}
};

ChooserPage::ChooserPage () :
  cmd_show_set (false), saved_geom (false), saw_geom_change (false)
{
  sizeProcessor.AddControlInfo (ChooserControlsInfo);

  const char *fg_ret =
    UserSettings::instance().get ("chooser_window_settings");
  if (!fg_ret)
    return;

  writer buf;
  UINT *py = buf.wpi;
  char *buf_copy = strdup (fg_ret);
  for (char *p = strtok (buf_copy, ","); p; p = strtok (NULL, ","))
    *py++ = atoi (p);
  free (buf_copy);
  if ((py - buf.wpi) == (sizeof (buf.wpi) / sizeof (buf.wpi[0])))
    {
      saved_geom = true;
      window_placement = buf.wp;
    }
}

ChooserPage::~ChooserPage ()
{
  if (saw_geom_change)
    {
      writer buf;
      buf.wp = window_placement;
      std::string toset;
      const char *comma = "";
      for (unsigned i = 0; i < (sizeof (buf.wpi) / sizeof (buf.wpi[0])); i++)
	{
	  char intbuf[33];
	  sprintf (intbuf, "%u", buf.wpi[i]);
	  toset += comma;
	  toset += intbuf;
	  comma = ",";
	}
      UserSettings::instance().set ("chooser_window_settings", toset);
    }
}

void
ChooserPage::createListview ()
{
  SetBusy ();
  static std::vector<packagemeta *> empty_cat;
  static Category dummy_cat (std::string ("No packages found."), empty_cat);
  packagedb db;
  packagedb::categoriesType::iterator it = db.categories.find("All");
  Category &cat = (it == db.categories.end ()) ? dummy_cat : *it;
  chooser = new PickView (cat);
  RECT r = getDefaultListViewSize();
  if (!chooser->Create(this, WS_CHILD | WS_HSCROLL | WS_VSCROLL | WS_VISIBLE,&r))
    // TODO throw exception
    exit (11);
  chooser->init(PickView::views::Category);
  chooser->Show(SW_SHOW);
  chooser->setViewMode (PickView::views::Category);
  if (!SetDlgItemText (GetHWND (), IDC_CHOOSE_VIEWCAPTION, chooser->mode_caption ()))
    Log (LOG_BABBLE) << "Failed to set View button caption %ld" <<
	 GetLastError () << endLog;

  /* FIXME: do we need to init the desired fields ? */
  static int ta[] = { IDC_CHOOSE_KEEP, IDC_CHOOSE_CURR, IDC_CHOOSE_EXP, 0 };
  rbset (GetHWND (), ta, IDC_CHOOSE_CURR);
  ClearBusy ();
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

void
ChooserPage::PlaceDialog (bool doit)
{
  if (unattended_mode == unattended)
    /* Don't jump up and down in (fully) unattended mode */;
  else if (doit)
    {
      pre_chooser_placement.length = sizeof pre_chooser_placement;
      GetWindowPlacement (ins_dialog, &pre_chooser_placement);
      if (saved_geom)
	SetWindowPlacement (ins_dialog, &window_placement);
      else
	{
	  ShowWindow (ins_dialog, SW_MAXIMIZE);
	  window_placement.length = sizeof window_placement;
	  GetWindowPlacement (ins_dialog, &window_placement);
	}
      cmd_show_set = true;
    }
  else if (cmd_show_set)
    {
      WINDOWPLACEMENT wp;
      wp.length = sizeof wp;
      if (GetWindowPlacement (ins_dialog, &wp)
	  && memcmp (&wp, &window_placement, sizeof (wp)) != 0)
	saw_geom_change = true;
      SetWindowPlacement (ins_dialog, &pre_chooser_placement);
      if (saw_geom_change)
	window_placement = wp;
      cmd_show_set = false;
    }
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

RECT
ChooserPage::getDefaultListViewSize()
{
  RECT result;
  getParentRect (GetHWND (), GetDlgItem (IDC_LISTVIEW_POS), &result);
  result.top += 2;
  result.bottom -= 2;
  return result;
}

void
ChooserPage::OnInit ()
{
  CheckDlgButton (GetHWND (), IDC_CHOOSE_HIDE, BST_CHECKED);

  SetBusy ();
  if (source == IDC_SOURCE_DOWNLOAD || source == IDC_SOURCE_LOCALDIR)
    packagemeta::ScanDownloadedFiles (MirrorOption);

  packagedb db;
  db.setExistence ();
  db.fillMissingCategory ();

  for (packagedb::packagecollection::iterator i = db.packages.begin ();
       i != db.packages.end (); ++i)
    {
      packagemeta &pkg = *(i->second);
      bool wanted    = pkg.isManuallyWanted();
      bool deleted   = pkg.isManuallyDeleted();
      bool basemisc  = (pkg.categories.find ("Base") != pkg.categories.end ()
		     || pkg.categories.find ("Misc") != pkg.categories.end ());
      bool upgrade   = wanted || (!pkg.installed && basemisc)
		     || UpgradeAlsoOption || !hasManualSelections;
      bool install   = wanted  && !deleted && !pkg.installed;
      bool reinstall = (wanted  || basemisc) && deleted;
      bool uninstall = (!(wanted  || basemisc) && deleted)
		     || (!pkg.curr && CleanOrphansOption);
      if (install)
	pkg.set_action (packagemeta::Install_action, pkg.curr);
      else if (reinstall)
	pkg.set_action (packagemeta::Reinstall_action, pkg.curr);
      else if (uninstall)
	pkg.set_action (packagemeta::Uninstall_action, packageversion ());
      else
	pkg.set_action (packagemeta::Default_action,
			upgrade ? pkg.trustp (true,  TRUST_UNKNOWN)
				: pkg.installed);
    }

  ClearBusy ();

  if (source == IDC_SOURCE_DOWNLOAD)
    setPrompt("Select packages to download ");
  else
    setPrompt("Select packages to install ");
  createListview ();

  AddTooltip (IDC_CHOOSE_KEEP, IDS_TRUSTKEEP_TOOLTIP);
  AddTooltip (IDC_CHOOSE_CURR, IDS_TRUSTCURR_TOOLTIP);
  AddTooltip (IDC_CHOOSE_EXP, IDS_TRUSTEXP_TOOLTIP);
  AddTooltip (IDC_CHOOSE_VIEW, IDS_VIEWBUTTON_TOOLTIP);
  AddTooltip (IDC_CHOOSE_HIDE, IDS_HIDEOBS_TOOLTIP);
  AddTooltip (IDC_CHOOSE_SEARCH_EDIT, IDS_SEARCH_TOOLTIP);
}

void
ChooserPage::OnActivate()
{
  chooser->refresh();;
  PlaceDialog (true);
}

long
ChooserPage::OnUnattended()
{
  if (unattended_mode == unattended)
    return OnNext ();
  // Magic constant -1 (FIXME) means 'display page but stay unattended', as
  // also used for progress bars; see proppage.cc!PropertyPage::DialogProc().
  return -1;
}

void
ChooserPage::logResults()
{
  Log (LOG_BABBLE) << "Chooser results..." << endLog;
  packagedb db;

  for (packagedb::packagecollection::iterator i = db.packages.begin(); i != db.packages.end(); i++)
    {
      i->second->logSelectionStatus();
    }
}

long
ChooserPage::OnNext ()
{
#ifdef DEBUG
  logResults();
#endif

  PlaceDialog (false);
  Progress.SetActivateTask (WM_APP_PREREQ_CHECK);

  return IDD_INSTATUS;
}

long
ChooserPage::OnBack ()
{
  PlaceDialog (false);

  if (source == IDC_SOURCE_LOCALDIR)
    return IDD_LOCAL_DIR;
  else
    return IDD_SITE;
}

void
ChooserPage::keepClicked()
{
  packagedb db;
  for (packagedb::packagecollection::iterator i = db.packages.begin ();
        i != db.packages.end (); ++i)
    {
      packagemeta & pkg = *(i->second);
      pkg.desired = pkg.installed;
    }
  chooser->refresh();
}

void
ChooserPage::changeTrust(trusts aTrust)
{
  SetBusy ();
  chooser->defaultTrust (aTrust);
  packagedb db;
  db.markUnVisited ();

  for (packagedb::packagecollection::iterator i = db.packages.begin(); i != db.packages.end(); i++)
    {
      i->second->set_requirements(aTrust);
    }

  chooser->refresh();
  PrereqChecker p;
  p.setTrust (aTrust);
  ClearBusy ();
}

bool
ChooserPage::OnMessageCmd (int id, HWND hwndctl, UINT code)
{
  if (code == EN_CHANGE && id == IDC_CHOOSE_SEARCH_EDIT)
    {
      std::string value (egetString (GetHWND (), IDC_CHOOSE_SEARCH_EDIT));
      chooser->SetPackageFilter (value);
      chooser->refresh ();
      return true;
    }
  else if (code != BN_CLICKED && code != EN_CHANGE)
    {
      // Not a click notification, we don't care.
      return false;
    }

  switch (id)
    {
    case IDC_CHOOSE_CLEAR_SEARCH:
      {
	std::string value;
	eset (GetHWND (), IDC_CHOOSE_SEARCH_EDIT, value);
	chooser->SetPackageFilter (value);
	chooser->refresh ();
      }
      break;

    case IDC_CHOOSE_KEEP:
      if (IsButtonChecked (id))
        keepClicked();
      break;

    case IDC_CHOOSE_CURR:
      if (IsButtonChecked (id))
        changeTrust (TRUST_CURR);
      break;

    case IDC_CHOOSE_EXP:
      if (IsButtonChecked (id))
        changeTrust (TRUST_TEST);
      break;

    case IDC_CHOOSE_VIEW:
      chooser->cycleViewMode ();
      if (!SetDlgItemText
        (GetHWND (), IDC_CHOOSE_VIEWCAPTION, chooser->mode_caption ()))
      Log (LOG_BABBLE) << "Failed to set View button caption " <<
           GetLastError () << endLog;
      break;

    case IDC_CHOOSE_HIDE:
      chooser->setObsolete (!IsButtonChecked (id));
      break;
    default:
      // Wasn't recognized or handled.
      return false;
    }

  // Was handled since we never got to default above.
  return true;
}

INT_PTR CALLBACK
ChooserPage::OnMouseWheel (UINT message, WPARAM wParam, LPARAM lParam)
{
  return chooser->WindowProc (message, wParam, lParam);
}
