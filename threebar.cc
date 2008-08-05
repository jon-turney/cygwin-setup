/*
 * Copyright (c) 2001, Gary R. Van Sickle.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Gary R. Van Sickle <g.r.vansickle@worldnet.att.net>
 *
 */

// This is the implementation of the ThreeBarProgressPage class.  It is a fairly generic
// progress indicator property page with three progress bars.

#include <string>
#include "win32.h"
#include "commctrl.h"
#include "resource.h"

#include "dialog.h"
#include "site.h"

#include "propsheet.h"
#include "threebar.h"
#include "String++.h"
#include "state.h"

#include "ControlAdjuster.h"

/*
  Sizing information.
 */
static ControlAdjuster::ControlInfo ThreeBarControlsInfo[] = {
  {IDC_INS_ACTION, 		CP_LEFT,    CP_TOP},
  {IDC_INS_PKG, 		CP_LEFT,    CP_TOP},
  {IDC_INS_FILE, 		CP_LEFT,    CP_TOP},
  {IDC_INS_DISKFULL, 		CP_STRETCH, CP_TOP},
  {IDC_INS_IPROGRESS, 		CP_STRETCH, CP_TOP},
  {IDC_INS_PPROGRESS,		CP_STRETCH, CP_TOP},
  {IDC_INS_BL_PACKAGE, 		CP_LEFT,    CP_TOP},
  {IDC_INS_BL_TOTAL,		CP_LEFT,    CP_TOP},
  {IDC_INS_BL_DISK,		CP_LEFT,    CP_TOP},
  {0, CP_LEFT, CP_TOP}
};

HWND ThreeBarProgressPage::ins_dialog;
ThreeBarProgressPage::ThreeBarProgressPage ():
  cmd_show_set (false)
{
  sizeProcessor.AddControlInfo (ThreeBarControlsInfo);
}

bool ThreeBarProgressPage::Create ()
{
  return PropertyPage::Create (IDD_INSTATUS);
}

void
ThreeBarProgressPage::OnInit ()
{
  // Get HWNDs to the dialog controls
  ins_action = GetDlgItem (IDC_INS_ACTION);
  ins_pkgname = GetDlgItem (IDC_INS_PKG);
  ins_filename = GetDlgItem (IDC_INS_FILE);
  // Bars
  ins_pprogress = GetDlgItem (IDC_INS_PPROGRESS);
  ins_iprogress = GetDlgItem (IDC_INS_IPROGRESS);
  ins_diskfull = GetDlgItem (IDC_INS_DISKFULL);
  // Bar labels
  ins_bl_package = GetDlgItem (IDC_INS_BL_PACKAGE);
  ins_bl_total = GetDlgItem (IDC_INS_BL_TOTAL);
  ins_bl_disk = GetDlgItem (IDC_INS_BL_DISK);
}

void
ThreeBarProgressPage::SetText1 (const TCHAR * t)
{
  ::SetWindowText (ins_action, t);
}

void
ThreeBarProgressPage::SetText2 (const TCHAR * t)
{
  ::SetWindowText (ins_pkgname, t);
}

void
ThreeBarProgressPage::SetText3 (const TCHAR * t)
{
  ::SetWindowText (ins_filename, t);
}

void
ThreeBarProgressPage::SetText4 (const TCHAR * t)
{
  ::SetWindowText (ins_bl_package, t);
}

void
ThreeBarProgressPage::SetBar1 (long progress, long max)
{
  int percent = (int) (100.0 * ((double) progress) / (double) max);
  SendMessage (ins_pprogress, PBM_SETPOS, (WPARAM) percent, 0);
}

void
ThreeBarProgressPage::SetBar2 (long progress, long max)
{
  int percent = (int) (100.0 * ((double) progress) / (double) max);
  SendMessage (ins_iprogress, PBM_SETPOS, (WPARAM) percent, 0);
  std::string s = stringify(percent);
  s += "% - Cygwin Setup";
  GetOwner ()->SetWindowText (s.c_str());
}

void
ThreeBarProgressPage::SetBar3 (long progress, long max)
{
  int percent = (int) (100.0 * ((double) progress) / (double) max);
  SendMessage (ins_diskfull, PBM_SETPOS, (WPARAM) percent, 0);
}

void
ThreeBarProgressPage::EnableSingleBar (bool enable)
{
  // Switch to/from single bar mode
  ShowWindow (ins_bl_total, enable ? SW_HIDE : SW_SHOW);
  ShowWindow (ins_bl_disk, enable ? SW_HIDE : SW_SHOW);
  ShowWindow (ins_iprogress, enable ? SW_HIDE : SW_SHOW);
  ShowWindow (ins_diskfull, enable ? SW_HIDE : SW_SHOW);
}

void
ThreeBarProgressPage::OnActivate ()
{
  // Disable back and next buttons
  GetOwner ()->SetButtons (0);

  // Set all bars to 0
  SetBar1 (0);
  SetBar2 (0);
  SetBar3 (0);

  switch (task)
    {
    case WM_APP_START_SITE_INFO_DOWNLOAD:
    case WM_APP_START_SETUP_INI_DOWNLOAD:
      // For these tasks, show only a single progress bar.
      EnableSingleBar ();
      break;
    default:
      // Show the normal 3-bar view by default
      EnableSingleBar (false);
      break;
    }

  Window::PostMessage (task);
}

void
ThreeBarProgressPage::MaximizeDialog (bool doit)
{
  if (doit)
    {
      WINDOWPLACEMENT wp;
      if (GetWindowPlacement (ins_dialog, &wp))
	{
	  cmd_show = wp.showCmd;
	  cmd_show_set = true;
	  ShowWindow (ins_dialog, SW_MAXIMIZE);
	}
    }
  else if (cmd_show_set)
    {
      ShowWindow (ins_dialog, cmd_show);
      cmd_show_set = false;
    }
}

bool
ThreeBarProgressPage::OnMessageApp (UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
    {
    case WM_APP_START_DOWNLOAD:
      {
	MaximizeDialog (false);
	// Start the package download thread.
	do_download (GetInstance (), GetHWND ());
	break;
      }
    case WM_APP_DOWNLOAD_THREAD_COMPLETE:
      {
	if (lParam == IDD_S_INSTALL)
	  {
	    // Download is complete and we want to go on to the install.
	    Window::PostMessage (WM_APP_START_INSTALL);
	  }
	else if (lParam != 0)
	  {
	    // Download either failed or completed in download-only mode.
	    GetOwner ()->SetActivePageByID (lParam);
	  }
	else
	  {
	    fatal("Unexpected fallthrough from the download thread", NO_ERROR);
	  }
	break;
      }
    case WM_APP_START_INSTALL:
      {
	MaximizeDialog (false);
	// Start the install thread.
	do_install (GetInstance (), GetHWND ());
	break;
      }
    case WM_APP_INSTALL_THREAD_COMPLETE:
      {
	// Install is complete and we want to go on to the postinstall.
	Window::PostMessage (WM_APP_START_POSTINSTALL);
	break;
      }
    case WM_APP_START_POSTINSTALL:
      {
	// Start the postinstall script thread.
	do_postinstall (GetInstance (), GetHWND ());
	break;
      }
    case WM_APP_POSTINSTALL_THREAD_COMPLETE:
      {
	// Re-enable and "Push" the Next button
	GetOwner ()->SetButtons (PSWIZB_NEXT);
	GetOwner ()->PressButton (PSBTN_NEXT);
	break;
      }
    case WM_APP_START_SITE_INFO_DOWNLOAD:
      {
	do_download_site_info (GetInstance (), GetHWND ());
	break;
      }
    case WM_APP_SITE_INFO_DOWNLOAD_COMPLETE:
      {
	GetOwner ()->SetActivePageByID (lParam);
	break;
      }
    case WM_APP_START_SETUP_INI_DOWNLOAD:
      {
	do_ini (GetInstance (), GetHWND ());
	break;
      }
    case WM_APP_SETUP_INI_DOWNLOAD_COMPLETE:
      {
	if (lParam)
	  {
	    GetOwner ()->SetActivePageByID (IDD_CHOOSE);
	    MaximizeDialog (true);
	  }
	else
	  {
	    if (source == IDC_SOURCE_CWD)
	      {
		// There was a setup.ini file (as found by
		// do_fromcwd), but it had parse errors.
		GetOwner ()->SetActivePageByID (IDD_SOURCE);
	      }
	    else
	      {
		// Download failed, try another site.
		GetOwner ()->SetActivePageByID (IDD_SITE);
	      }
	  }
	break;
      }
    default:
      {
	// Not handled
	return false;
      }
    }

  return true;
}
