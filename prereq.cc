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

// Sizing information.
static ControlAdjuster::ControlInfo PrereqControlsInfo[] = {
  {IDC_PREREQ_CHECK, 		CP_LEFT,    CP_BOTTOM},
  {IDC_PREREQ_EDIT,		CP_STRETCH, CP_STRETCH},
  {0, CP_LEFT, CP_TOP}
};

extern ThreeBarProgressPage Progress;

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
  // and found that there were missing packages; so we can just call
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
      int res = mbox (h,
          "The listed packages are required for packages depending on them to "
          "work.  We strongly recommend that you allow Setup to select them."
          "\r\n\r\n"
          "Are you sure you want to proceed without these packages?",
          "WARNING - Required Packages Not Selected",
          MB_YESNO | MB_ICONEXCLAMATION | MB_DEFBUTTON2);
      if (res == IDNO)
        return -1;
      else
        Log (LOG_PLAIN) <<
            "NOTE!  User refused suggested missing dependencies!  "
            "Expect some packages to give errors or not function at all." << endLog;
    }
  else
    {
      // add the missing requirements
      PrereqChecker p;
      p.selectMissing ();
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
  return IDD_CHOOSE;
}

long
PrereqPage::OnUnattended ()
{
  // in chooser-only mode, show this page so the user can choose to fix dependency problems or not
  if (unattended_mode == chooseronly)
    return -1;

  // in unattended mode, add the missing requirements, then carry on to download/install
  PrereqChecker p;
  p.selectMissing ();

  return whatNext();
}

// ---------------------------------------------------------------------------
// implements class PrereqChecker
// ---------------------------------------------------------------------------

// instantiate the static members
map <packagemeta *, vector <packagemeta *>, packagemeta_ltcomp> PrereqChecker::unmet;
map <std::string, vector <packagemeta *> > PrereqChecker::notfound;
trusts PrereqChecker::theTrust = TRUST_CURR;

/* This function builds a list of unmet dependencies to present to the user on
   the PrereqPage propsheet.

   The data is stored in two associative maps:
     unmet[package] = vector of packages that depend on package.
     notfound[package-name] = vector of packages that depend on package.
*/
bool
PrereqChecker::isMet ()
{
  packagedb db;

  Progress.SetText1 ("Checking prerequisites...");
  Progress.SetText2 ("");
  Progress.SetText3 ("");

  // clear static data each time this is called
  unmet.clear ();
  notfound.clear ();

  // packages that need to be checked for dependencies
  queue <packagemeta *> todo;

  // go through all packages, adding desired ones to the initial work list
  for (packagedb::packagecollection::iterator p = db.packages.begin ();
        p != db.packages.end (); ++p)
    {
      if (p->second->desired)
        todo.push (p->second);
    }

  int max = todo.size();
  int pos = 0;

  // churn through the work list
  while (!todo.empty ())
    {
      // get the first package off the work list
      packagemeta *pack = todo.front ();
      todo.pop ();

      pos++;
      Progress.SetText2 (pack->name.c_str());
      static char buf[100];
      sprintf(buf, "%d %%  (%d/%d)", pos * 100 / max, pos, max);
      Progress.SetText3(buf);
      Progress.SetBar1(pos, max);

      // Fetch the dependencies of the package. This assumes that the
      // dependencies of the prev, curr, and exp versions are all the same.
      vector <vector <PackageSpecification *> *> *deps = pack->curr.depends ();

      // go through the package's dependencies
      for (vector <vector <PackageSpecification *> *>::iterator d =
            deps->begin (); d != deps->end (); ++d)
        {
          // XXX: the following assumes that there is only a single
          // node in each OR clause, which is currently the case.
          // if setup is ever pushed to use AND/OR in "depends:"
          // lines this will have to be updated
          PackageSpecification *dep_spec = (*d)->at(0);
          packagemeta *dep = db.findBinary (*dep_spec);

          if (dep)
            {
              if (!(dep->desired && dep_spec->satisfies (dep->desired)))
                {
                  // we've got an unmet dependency
                  if (unmet.find (dep) == unmet.end ())
                    {
                      // newly found dependency: add to worklist
                      todo.push (dep);
                      max++;
                    }
                  unmet[dep].push_back (pack);
                }
            }
          else
            {
              // dependency on a package which doesn't have any binary versions
              // (i.e. it is source only or doesn't exist)
              Log (LOG_PLAIN) << "package " << pack->name << " has dependency "
                              << dep_spec->packageName() << " we can't find" << endLog;
              notfound[dep_spec->packageName()].push_back (pack);
            }
        }
    }

  return unmet.empty () && notfound.empty ();
}

/* Formats 'unmet' as a string for display to the user.  */
void
PrereqChecker::getUnmetString (std::string &s)
{
  s = "";

  {
    map <std::string, vector <packagemeta *> >::iterator i;
    for (i = notfound.begin(); i != notfound.end(); i++)
      {
        s = s + i->first
          + "\t(not found)"
          + "\r\n\tRequired by: ";
        for (unsigned int j = 0; j < i->second.size(); j++)
          {
            s += i->second[j]->name;
            if (j != i->second.size() - 1)
              s += ", ";
          }
        s += "\r\n\r\n";
      }
  }

  map <packagemeta *, vector <packagemeta *>, packagemeta_ltcomp>::iterator i;
  for (i = unmet.begin(); i != unmet.end(); i++)
    {
      s = s + i->first->name
	    + "\t(" + i->first->trustp (false, theTrust).Canonical_version ()
	    + ")\r\n\t" + i->first->SDesc ()
	    + "\r\n\tRequired by: ";
      for (unsigned int j = 0; j < i->second.size(); j++)
        {
          s += i->second[j]->name;
          if (j != i->second.size() - 1)
            s += ", ";
        }
      s += "\r\n\r\n";
    }
}

/* Takes the keys of 'unmet' and selects them, using the current trust.  */
void
PrereqChecker::selectMissing ()
{
  packagedb db;

  // provide a default, even though this should have been set for us
  if (!theTrust)
    theTrust = TRUST_CURR;

  // get each of the keys of 'unmet'
  map <packagemeta *, vector <packagemeta *>, packagemeta_ltcomp>::iterator i;
  for (i = unmet.begin(); i != unmet.end(); i++)
    {
      packageversion vers = i->first->trustp (false, theTrust);
      i->first->desired = vers;
      vers.sourcePackage ().pick (false, NULL);

      if (vers == i->first->installed)
        {
          vers.pick (false, NULL);
          Log (LOG_PLAIN) << "Adding required dependency " << i->first->name <<
               ": Selecting already-installed version " <<
               i->first->installed.Canonical_version () << "." << endLog;
        }
      else
        {
          vers.pick (vers.accessible (), i->first);
          Log (LOG_PLAIN) << "Adding required dependency " << i->first->name <<
              ": Selecting version " << vers.Canonical_version () <<
              " for installation." << endLog;
        }
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
  TOPLEVEL_CATCH("prereq_check");

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
