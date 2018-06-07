/*
 * Copyright (c) 2018
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 */

#include "confirm.h"
#include "threebar.h"
#include "resource.h"
#include "state.h"
#include "ControlAdjuster.h"
#include "package_db.h"
#include "package_meta.h"

#include <algorithm>

extern ThreeBarProgressPage Progress;

// Sizing information.
static ControlAdjuster::ControlInfo ConfirmControlsInfo[] = {
  {IDC_CONFIRM_EDIT, CP_STRETCH, CP_STRETCH},
  {0, CP_LEFT, CP_TOP}
};

// ---------------------------------------------------------------------------
// implements class ConfirmPage
// ---------------------------------------------------------------------------

ConfirmPage::ConfirmPage ()
{
  sizeProcessor.AddControlInfo (ConfirmControlsInfo);
}

bool
ConfirmPage::Create ()
{
  return PropertyPage::Create (IDD_CONFIRM);
}

void
ConfirmPage::OnInit ()
{
  // set the edit-area to a larger font
  SetDlgItemFont(IDC_CONFIRM_EDIT, "MS Shell Dlg", 10);
}

void
ConfirmPage::OnActivate()
{
  // generate a report on actions we're going to take
  std::string s = "";

  packagedb db;
  const SolverTransactionList & trans = db.solution.transactions ();

  // first list things we will erase
  if (source != IDC_SOURCE_DOWNLOAD)
    {
      std::vector<std::string> erase;
      for (SolverTransactionList::const_iterator i = trans.begin ();
           i != trans.end (); i++)
        {
          if (i->type == SolverTransaction::transErase)
            {
              std::string line;
              packageversion pv = i->version;
              packagemeta *pkg = db.findBinary (PackageSpecification (pv.Name ()));

              line += "Uninstall ";
              line += i->version.Name();
              line += " ";
              line += i->version.Canonical_version();
              if (pkg && pkg->desired)
                line += " (automatically added)";
              line += "\r\n";
              erase.push_back (line);
            }
        }
      sort (erase.begin(), erase.end());
      for (std::vector<std::string>::const_iterator i = erase.begin ();
           i != erase.end(); i++)
        s += *i;
    }

  // then list things downloaded or installed
  std::vector<std::string> install;
  for (SolverTransactionList::const_iterator i = trans.begin ();
       i != trans.end (); i++)
    {
      packageversion pv = i->version;
      packagemeta *pkg = db.findBinary (PackageSpecification (pv.Name ()));

      if (i->type == SolverTransaction::transInstall)
          {
            std::string line;
            if (source != IDC_SOURCE_DOWNLOAD)
              line += "Install ";
            else
              line += "Download ";
            line += i->version.Name();
            line += " ";
            line += i->version.Canonical_version();
            if (i->version.Type() == package_source)
              line += " (source)";
            else if (pkg && pkg->desired != pv)
              line += " (automatically added)";
            line += "\r\n";
            install.push_back (line);
          }
    }
  sort (install.begin(), install.end());
  for (std::vector<std::string>::const_iterator i = install.begin ();
       i != install.end(); i++)
    s += *i;

  // be explicit about doing nothing
  if (s.empty())
    s += "No changes";

  SetDlgItemText (GetHWND (), IDC_CONFIRM_EDIT, s.c_str ());

  // move focus to 'next' button, so enter doesn't get eaten by edit control
  HWND nextButton = ::GetDlgItem(::GetParent(GetHWND()), 0x3024 /* ID_WIZNEXT */);
  PostMessage (GetHWND (), WM_NEXTDLGCTL, (WPARAM)nextButton, TRUE);
}

long
ConfirmPage::OnNext ()
{
  return whatNext();
}

long
ConfirmPage::whatNext ()
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
ConfirmPage::OnBack ()
{
  return IDD_CHOOSE;
}

long
ConfirmPage::OnUnattended ()
{
  return whatNext();
}
