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
      int res = MessageBox (h, 
          "If you continue without correcting the listed conflicts, your "
          "Cygwin installation will not function properly.\r\n"
          "We strongly recommend that you let Setup install the listed packages.\r\n\r\n"
          "Are you sure you want to proceed?",
          "WARNING - Required Packages Not Selected",
          MB_YESNO | MB_ICONEXCLAMATION | MB_DEFBUTTON2);
      if (res == IDNO)
        return -1;
      else
        log (LOG_PLAIN) << 
            "NOTE!  User refused suggested missing dependencies!  "
            "Expect some packages to give errors or not function at all." << endLog;
    }
  else
    {
      // add the missing requirements
      PrereqChecker p;
      p.selectMissing ();
    }
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
PrereqPage::OnBack ()
{
  return IDD_CHOOSE;
}


// ---------------------------------------------------------------------------
// implements class PrereqChecker
// ---------------------------------------------------------------------------

// instantiate the static members
map <packagemeta *, vector <packagemeta *>, packagemeta_ltcomp> PrereqChecker::unmet;
trusts PrereqChecker::theTrust = TRUST_CURR;

/* This function builds a list of unmet dependencies to present to the user on
   the PrereqPage propsheet.  The data is stored as an associative map of
   unmet[missing-package] = vector of packages that depend on missing-package */
bool
PrereqChecker::isMet ()
{
  packagedb db;
  bool foundUnmet = false;

  // unmet is static - clear it each time this is called
  unmet.clear ();
  
  // loop through each package
  for (vector <packagemeta *>::iterator n = db.packages.begin ();
        n != db.packages.end (); ++n)
    {
      // if the package is installed or selected to be installed...
      if ((*n)->desired)
        {
          // loop through each dependency
          for (vector <vector <PackageSpecification *> *>::iterator i = 
                (*n)->desired.depends ()->begin ();
                i != (*n)->desired.depends ()->end (); ++i)
            {
              // XXX: the following assumes that there is only a single
              // node in each OR clause, which is currently the case.
              // if setup is ever pushed to use AND/OR in "depends:"
              // lines this will have to be updated
              PackageSpecification *spec = (*i)->at(0);
                  
              packagemeta *pack = db.findBinary (*spec);
              if (!pack)
                continue;  // asking for a package that doesn't exist - error?

              if (pack->desired && spec->satisfies (pack->desired))
                {
                  // dependency met
                }
              else
                {                  
                  foundUnmet = true;
                  unmet[pack].push_back (*n);
                }
            }
        }
    }
    
  return !foundUnmet;
}

/* Formats 'unmet' as a string for display to the user.  */
void
PrereqChecker::getUnmetString (std::string &s)
{
  s = "";
  
  map <packagemeta *, vector <packagemeta *>, packagemeta_ltcomp>::iterator i;
  for (i = unmet.begin(); i != unmet.end(); i++)
    {
      s = s + "Package: " + std::string(i->first->name) + "\r\n\tRequired by: ";
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
      packageversion vers = i->first->trustp (theTrust);
      i->first->desired = vers;
      vers.sourcePackage ().pick (false);
      
      if (vers == i->first->installed)
        {
          vers.pick (false);
          log (LOG_PLAIN) << "Adding required dependency " << i->first->name <<
               ": Selecting already-installed version " <<
               i->first->installed.Canonical_version () << "." << endLog;
        }
      else
        {
          vers.pick (vers.accessible ());
          log (LOG_PLAIN) << "Adding required dependency " << i->first->name <<
              ": Selecting version " << vers.Canonical_version () <<
              " for installation." << endLog;
        }
    }
}
