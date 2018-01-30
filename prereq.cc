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

#include "prereq.h"
#include "resource.h"
#include "state.h"
#include "threebar.h"
#include "LogSingleton.h"
#include "ControlAdjuster.h"
#include "package_db.h"

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
  Log (LOG_PLAIN) << s << endLog;
  SetDlgItemText (GetHWND (), IDC_PREREQ_EDIT, s.c_str ());

  SetFocus (GetDlgItem (IDC_PREREQ_CHECK));
}

long
PrereqPage::OnNext ()
{
  HWND h = GetHWND ();
  packagedb db;

  if (!IsDlgButtonChecked (h, IDC_PREREQ_CHECK))
    {
      // breakage imminent!  danger, danger
      int res = MessageBox (h,
          "Some packages may not work properly if you continue."
          "\r\n\r\n"
          "Are you sure you want to proceed (NOT RECOMMENDED)?",
          "WARNING - Unsolved Problems",
          MB_YESNO | MB_ICONEXCLAMATION | MB_DEFBUTTON2);
      if (res == IDNO)
        return -1;
      else
        {
          Log (LOG_PLAIN) <<
            "NOTE!  User continued with unsolved problems!  "
            "Expect some packages to give errors or not function at all." << endLog;
          // Change the solver's transaction list to reflect the user's choices.
          db.solution.db2trans();
        }
    }
  else
    {
      db.solution.applyDefaultProblemSolutions();
    }

  PrereqChecker p;
  p.finalize();

  return IDD_CONFIRM;
}

long
PrereqPage::OnBack ()
{
  // Add reinstall tasks
  PrereqChecker p;
  p.augment();

  // Reset the package database to correspond to the solver's solution
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

  packagedb db;
  db.solution.applyDefaultProblemSolutions();

  PrereqChecker p;
  p.finalize();

  return IDD_CONFIRM;
}

// ---------------------------------------------------------------------------
// implements class PrereqChecker
// ---------------------------------------------------------------------------

// instantiate the static members
bool PrereqChecker::use_test_packages;
SolverTasks PrereqChecker::q;

bool
PrereqChecker::isMet ()
{
  packagedb db;

  Progress.SetText1 ("Solving dependencies...");
  Progress.SetText2 ("");
  Progress.SetText3 ("");

  // Create task list corresponding to current state of package database
  q.setTasks();

  // apply solver to those tasks and global state (use test or not)
  return db.solution.update(q, SolverSolution::keep, use_test_packages);
}

void
PrereqChecker::finalize ()
{
  augment();

  packagedb db;
  db.solution.addSource(IncludeSource);
  db.solution.dumpTransactionList();
}

void
PrereqChecker::augment ()
{
  packagedb db;
  db.solution.augmentTasks(q);
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
      p.finalize();
      retval = IDD_CONFIRM;
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
