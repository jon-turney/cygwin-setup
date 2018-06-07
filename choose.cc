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

#include "win32.h"
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <ctype.h>
#include <process.h>
#include <algorithm>

#include "ini.h"
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

#include "threebar.h"
#include "Generic.h"
#include "ControlAdjuster.h"
#include "prereq.h"

#include "UserSettings.h"

#include "Exception.h"

#include "getopt++/BoolOption.h"
static BoolOption UpgradeAlsoOption (false, 'g', "upgrade-also", "Also upgrade installed packages");
static BoolOption CleanOrphansOption (false, 'o', "delete-orphans", "Remove orphaned packages");
static BoolOption ForceCurrentOption (false, 'f', "force-current", "Select the current version for all packages");
static BoolOption PruneInstallOption (false, 'Y', "prune-install", "Prune the installation to only the requested packages");

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
  {IDC_CHOOSE_BEST, 		CP_RIGHT,   CP_TOP},
  {IDC_CHOOSE_SYNC, 		CP_RIGHT,   CP_TOP},
  {IDC_CHOOSE_EXP, 		CP_RIGHT,   CP_TOP},
  {IDC_CHOOSE_VIEW, 		CP_LEFT,    CP_TOP},
  {IDC_LISTVIEW_POS, 		CP_RIGHT,   CP_TOP},
  {IDC_CHOOSE_VIEWCAPTION,	CP_LEFT,    CP_TOP},
  {IDC_CHOOSE_LIST,		CP_STRETCH, CP_STRETCH},
  {IDC_CHOOSE_HIDE,             CP_LEFT,    CP_BOTTOM},
  {0, CP_LEFT, CP_TOP}
};

ChooserPage::ChooserPage () :
  cmd_show_set (false), saved_geom (false), saw_geom_change (false),
  timer_id (DEFAULT_TIMER_ID), activated (false)
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
  static Category dummy_cat (std::string ("All"), empty_cat);
  chooser = new PickView (dummy_cat);
  RECT r = getDefaultListViewSize();
  if (!chooser->Create(this, WS_CHILD | WS_HSCROLL | WS_VSCROLL | WS_VISIBLE,&r))
    throw new Exception (TOSTRING(__LINE__) " " __FILE__,
			 "Unable to create chooser list window",
			 APPERR_WINDOW_ERROR);
  chooser->init(PickView::views::Category);
  chooser->Show(SW_SHOW);
  chooser->setViewMode (!is_new_install || UpgradeAlsoOption || hasManualSelections ?
			PickView::views::PackagePending : PickView::views::Category);
  SendMessage (GetDlgItem (IDC_CHOOSE_VIEW), CB_SETCURSEL, (WPARAM)chooser->getViewMode(), 0);
  ClearBusy ();
}

void
ChooserPage::initialUpdateState()
{
  // set the initial update state
  if (ForceCurrentOption)
    {
      update_mode_id = IDC_CHOOSE_SYNC;
      changeTrust(update_mode_id, false, true);
    }
  else if (hasManualSelections && !UpgradeAlsoOption)
    {
      // if packages are added or removed on the command-line and --upgrade-also
      // isn't used, we keep the current versions of everything else
      update_mode_id = IDC_CHOOSE_KEEP;
    }
  else
    {
      update_mode_id = IDC_CHOOSE_BEST;
      changeTrust (update_mode_id, false, true);
    }

  static int ta[] = { IDC_CHOOSE_KEEP, IDC_CHOOSE_BEST, IDC_CHOOSE_SYNC, 0 };
  rbset (GetHWND (), ta, update_mode_id);
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

  /* Populate view dropdown list with choices */
  HWND viewlist = GetDlgItem (IDC_CHOOSE_VIEW);
  SendMessage (viewlist, CB_RESETCONTENT, 0, 0);
  for (int view = (int)PickView::views::PackageFull;
       view <= (int)PickView::views::Category;
       view++)
    {
      SendMessage(viewlist, CB_ADDSTRING, 0, (LPARAM)PickView::mode_caption((PickView::views)view));
    }

  if (source == IDC_SOURCE_DOWNLOAD)
    setPrompt("Select packages to download ");
  else
    setPrompt("Select packages to install ");

  createListview ();

  AddTooltip (IDC_CHOOSE_KEEP, IDS_TRUSTKEEP_TOOLTIP);
  AddTooltip (IDC_CHOOSE_BEST, IDS_TRUSTCURR_TOOLTIP);
  AddTooltip (IDC_CHOOSE_SYNC, IDS_TRUSTSYNC_TOOLTIP);
  AddTooltip (IDC_CHOOSE_EXP, IDS_TRUSTEXP_TOOLTIP);
  AddTooltip (IDC_CHOOSE_VIEW, IDS_VIEWBUTTON_TOOLTIP);
  AddTooltip (IDC_CHOOSE_HIDE, IDS_HIDEOBS_TOOLTIP);
  AddTooltip (IDC_CHOOSE_SEARCH_EDIT, IDS_SEARCH_TOOLTIP);

  /* Set focus to search edittext control. */
  PostMessage (GetHWND (), WM_NEXTDLGCTL,
               (WPARAM) GetDlgItem (IDC_CHOOSE_SEARCH_EDIT), TRUE);
}

void
ChooserPage::applyCommandLinePackageSelection()
{
  packagedb db;
  for (packagedb::packagecollection::iterator i = db.packages.begin ();
       i != db.packages.end (); ++i)
    {
      packagemeta &pkg = *(i->second);
      bool wanted    = pkg.isManuallyWanted();
      bool deleted   = pkg.isManuallyDeleted();
      bool base      = pkg.categories.find ("Base") != pkg.categories.end ();
      bool orphaned  = pkg.categories.find ("Orphaned") != pkg.categories.end ();
      bool upgrade   = wanted || (!pkg.installed && base);
      bool install   = wanted  && !deleted && !pkg.installed;
      bool reinstall = (wanted  || base) && deleted;
      bool uninstall = (!(wanted  || base) && (deleted || PruneInstallOption))
		     || (orphaned && CleanOrphansOption);
      if (install)
	pkg.set_action (packagemeta::Install_action, pkg.curr);
      else if (reinstall)
	pkg.set_action (packagemeta::Reinstall_action, pkg.curr);
      else if (uninstall)
	pkg.set_action (packagemeta::Uninstall_action, packageversion ());
      else if (PruneInstallOption)
	pkg.set_action (packagemeta::Default_action, pkg.curr);
      else if (upgrade)
	pkg.set_action (packagemeta::Default_action, pkg.trustp(true, TRUST_UNKNOWN));
      else
	pkg.set_action (packagemeta::Default_action, pkg.installed);
    }
}

void
ChooserPage::OnActivate()
{
  SetBusy();

  packagedb db;
  db.prep();

  if (!activated)
    {
      // Do things which should only happen once, but rely on packagedb being
      // ready to use, so OnInit() is too early
      db.noChanges();
      applyCommandLinePackageSelection();
      initialUpdateState();

      activated = true;
    }

  ClearBusy();

  chooser->refresh();
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
  update_mode_id = IDC_CHOOSE_KEEP;
  packagedb db;
  db.noChanges();
  chooser->refresh();
}

void
ChooserPage::changeTrust(int button, bool test, bool initial)
{
  SetBusy ();

  update_mode_id = button;

  SolverSolution::updateMode mode;
  switch (button)
    {
    default:
    case IDC_CHOOSE_KEEP:
      mode = SolverSolution::keep;
      break;

    case IDC_CHOOSE_BEST:
      mode = SolverSolution::updateBest;
      break;

    case IDC_CHOOSE_SYNC:
      mode = SolverSolution::updateForce;
      break;
    }

  packagedb db;
  SolverTasks q;

  // usually we want to apply the solver to an empty task list to get the list
  // of packages to upgrade (if any)
  // but initially we want a task list with any package changes caused by
  // command line options
  if (initial)
    q.setTasks();

  db.defaultTrust(q, mode, test);

  // configure PickView so 'test' or 'curr' version is chosen when an
  // uninstalled package is first clicked on.
  chooser->defaultTrust (test ? TRUST_TEST : TRUST_CURR);

  chooser->refresh();

  PrereqChecker::setTestPackages(test);
  ClearBusy ();
}

bool
ChooserPage::OnMessageCmd (int id, HWND hwndctl, UINT code)
{
#if DEBUG
  Log (LOG_BABBLE) << "OnMessageCmd " << id << " " << hwndctl << " " << std::hex << code << endLog;
#endif

  if (id == IDC_CHOOSE_SEARCH_EDIT)
    {
      HWND nextButton = ::GetDlgItem(::GetParent(GetHWND()), 0x3024 /* ID_WIZNEXT */);
      char buf[16];

      if ((code == EN_CHANGE) ||
          ((code == EN_SETFOCUS) && GetWindowText(GetDlgItem(IDC_CHOOSE_SEARCH_EDIT), buf, 15)))
        {
          // when focus arrives at this control and it has some text in it, or
          // when we change the text in it, change the default button to one
          // which immediately applies the search filter
          //
          // (we don't do this when the focus is on this control but it's empty
          // (the initial state of the dialog) so that enter in that state moves
          // onto the next page)
          SendMessage(GetHWND (), DM_SETDEFID, (WPARAM) IDC_CHOOSE_DO_SEARCH, 0);
          SendMessage(nextButton, BM_SETSTYLE, BS_PUSHBUTTON, TRUE);
        }
      if (code == EN_CHANGE)
        {
          // apply the search filter when we stop typing
          SetTimer(GetHWND (), timer_id, SEARCH_TIMER_DELAY, (TIMERPROC) NULL);
        }
      else if (code == EN_KILLFOCUS)
        {
          // when focus leaves this control, restore the normal default button
          SendMessage(GetHWND (), DM_SETDEFID, (WPARAM) 0x3024 /* ID_WIZNEXT */, 0);
          SendMessage(nextButton, BM_SETSTYLE, BS_DEFPUSHBUTTON, TRUE);
        }
      return true;
    }
  else if (code == BN_CLICKED)
  {
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

    case IDC_CHOOSE_DO_SEARCH:
      // invisible pushbutton which is the default pushbutton while typing into
      // the search textbox, so that 'enter' causes the filter to be applied
      // immediately, rather than activating the next page
      SendMessage(GetHWND (), WM_TIMER, (WPARAM) timer_id, 0);
      break;

    case IDC_CHOOSE_KEEP:
      if (IsButtonChecked (id))
        keepClicked();
      break;

    case IDC_CHOOSE_BEST:
      if (IsButtonChecked (id))
        changeTrust (id, IsButtonChecked(IDC_CHOOSE_EXP), false);
      break;

    case IDC_CHOOSE_SYNC:
      if (IsButtonChecked (id))
        changeTrust (id, IsButtonChecked(IDC_CHOOSE_EXP), false);
      break;

    case IDC_CHOOSE_EXP:
      changeTrust(update_mode_id, IsButtonChecked (id), false);
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
  else if (code == CBN_SELCHANGE)
    {
      if (id == IDC_CHOOSE_VIEW)
        {
          // switch to the selected view
          LRESULT view_mode = SendMessage (GetDlgItem (IDC_CHOOSE_VIEW),
                                           CB_GETCURSEL, 0, 0);
          if (view_mode != CB_ERR)
            chooser->setViewMode ((PickView::views)view_mode);

          return true;
        }
    }

  // We don't care.
  return false;
}

INT_PTR CALLBACK
ChooserPage::OnMouseWheel (UINT message, WPARAM wParam, LPARAM lParam)
{
  return chooser->WindowProc (message, wParam, lParam);
}

INT_PTR CALLBACK
ChooserPage::OnTimerMessage (UINT message, WPARAM wParam, LPARAM lparam)
{
  if (wParam == timer_id)
    {
      std::string value (egetString (GetHWND (), IDC_CHOOSE_SEARCH_EDIT));

      KillTimer (GetHWND (), timer_id);
      chooser->SetPackageFilter (value);
      chooser->refresh ();
      return TRUE;
    }

  return FALSE;
}
