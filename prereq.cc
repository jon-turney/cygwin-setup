/*
 * Copyright (c) 2005 Brian Dessent
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Brian Dessent <brian@dessent.net>
 *
 */

#include "win32.h"
#include <commctrl.h>
#include <stdio.h>
#include <io.h>
#include <ctype.h>
#include <process.h>
#include <queue>

#include "prereq.h"
#include "dialog.h"
#include "resource.h"
#include "state.h"
#include "propsheet.h"
#include "threebar.h"
#include "Generic.h"
#include "LogSingleton.h"
#include "ControlAdjuster.h"
#include "package_db.h"
#include "package_meta.h"
#include "msg.h"
#include "Exception.h"
#include "getopt++/BoolOption.h"

// Sizing information.
static ControlAdjuster::ControlInfo PrereqControlsInfo[] = {
  {IDC_PREREQ_CHECK, 		CP_LEFT,    CP_BOTTOM},
  {IDC_PREREQ_EDIT,		CP_STRETCH, CP_STRETCH},
  {0, CP_LEFT, CP_TOP}
};

extern ThreeBarProgressPage Progress;
BoolOption IncludeSource (false, 'I', "include-source", "Automatically install source for every package installed");

// ---------------------------------------------------------------------------
// implements class PrereqPage
// ---------------------------------------------------------------------------

PrereqPage::PrereqPage ()
{
  sizeProcessor.AddControlInfo (PrereqControlsInfo);
}

bool
PrereqPage::Create ()
{
  return PropertyPage::Create (IDD_PREREQ);
}

void
PrereqPage::OnInit ()
{
  // start with the checkbox set
  CheckDlgButton (GetHWND (), IDC_PREREQ_CHECK, BST_CHECKED);

  // set the edit-area to a larger font
  SetDlgItemFont(IDC_PREREQ_EDIT, "MS Shell Dlg", 10);
}

void
PrereqPage::OnActivate()
{
  // if we have gotten this far, then PrereqChecker has already run isMet
  // and found that there were problems; so we can just call
  // getUnmetString to format the results and display it

  std::string s;
  PrereqChecker p;
  p.getUnmetString (s);
  SetDlgItemText (GetHWND (), IDC_PREREQ_EDIT, s.c_str ());

  SetFocus (GetDlgItem (IDC_PREREQ_CHECK));
}

long
PrereqPage::OnNext ()
{
  HWND h = GetHWND ();

  if (!IsDlgButtonChecked (h, IDC_PREREQ_CHECK))
    {
      // breakage imminent!  danger, danger
      int res = MessageBox (h,
          "We strongly recommend that you accept the default solutions. "
          "Some packages may not work properly if you don't."
          "\r\n\r\n"
          "Are you sure you want to proceed?",
          "WARNING - Unsolved Problems",
          MB_YESNO | MB_ICONEXCLAMATION | MB_DEFBUTTON2);
      if (res == IDNO)
        return -1;
      else
        {
          Log (LOG_PLAIN) <<
            "NOTE!  User refused the default solutions!  "
            "Expect some packages to give errors or not function at all." << endLog;
          // Change the solver's transaction list to reflect the user's choices.
          packagedb db;
          db.solution.db2trans();
        }
    }

  return whatNext();
}

long
PrereqPage::whatNext ()
{
  if (source == IDC_SOURCE_LOCALDIR)
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
PrereqPage::OnBack ()
{
  // Reset the package database to correspond to the solver's default
  // solution
  packagedb db;
  db.solution.trans2db();

  return IDD_CHOOSE;
}

long
PrereqPage::OnUnattended ()
{
  // in chooser-only mode, show this page so the user can choose to fix dependency problems or not
  if (unattended_mode == chooseronly)
    return -1;

  return whatNext();
}

// ---------------------------------------------------------------------------
// implements class PrereqChecker
// ---------------------------------------------------------------------------

// instantiate the static member
bool PrereqChecker::use_test_packages;

bool
PrereqChecker::isMet ()
{
  packagedb db;

  Progress.SetText1 ("Solving dependencies...");
  Progress.SetText2 ("");
  Progress.SetText3 ("");

  // Create task list corresponding to current state of package database
  SolverTasks q;
  q.setTasks();

  // apply solver to those tasks and global state (use test, include source)
  return db.solution.update(q, FALSE, use_test_packages, IncludeSource);
}

/* Formats problems and solutions as a string for display to the user.  */
void
PrereqChecker::getUnmetString (std::string &s)
{
  packagedb db;
  s = db.solution.report();

  //
  size_t pos = 0;
  while ((pos = s.find("\n", pos)) != std::string::npos)
    {
      s.replace(pos, 1, "\r\n");
      pos += 2;
    }
}

// ---------------------------------------------------------------------------
// progress page glue
// ---------------------------------------------------------------------------

static int
do_prereq_check_thread(HINSTANCE h, HWND owner)
{
  PrereqChecker p;
  int retval;

  if (p.isMet ())
    {
      if (source == IDC_SOURCE_LOCALDIR)
	Progress.SetActivateTask (WM_APP_START_INSTALL);  // install
      else
	Progress.SetActivateTask (WM_APP_START_DOWNLOAD); // start download
      retval = IDD_INSTATUS;
    }
  else
    {
      // rut-roh, some required things are not selected
      retval = IDD_PREREQ;
    }

  return retval;
}

static DWORD WINAPI
do_prereq_check_reflector (void *p)
{
  HANDLE *context;
  context = (HANDLE *) p;

  try
  {
    int next_dialog = do_prereq_check_thread ((HINSTANCE) context[0], (HWND) context[1]);

    // Tell the progress page that we're done prereq checking
    Progress.PostMessageNow (WM_APP_PREREQ_CHECK_THREAD_COMPLETE, 0, next_dialog);
  }
  TOPLEVEL_CATCH((HWND) context[1], "prereq_check");

  ExitThread(0);
}

static HANDLE context[2];

void
do_prereq_check (HINSTANCE h, HWND owner)
{
  context[0] = h;
  context[1] = owner;

  DWORD threadID;
  CreateThread (NULL, 0, do_prereq_check_reflector, context, 0, &threadID);
}
