/*
 * Copyright (c) 2000, Red Hat, Inc.
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

/* The purpose of this file is to manage the dialog box that lets the
   user choose the source of the install - from the net, from the
   current directory, or to just download files. */

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include "win32.h"
#include <stdio.h>
#include "dialog.h"
#include "resource.h"
#include "state.h"
#include "msg.h"
#include "log.h"
#include "package_db.h"

#include "AntiVirus.h"

#include "getopt++/BoolOption.h"

/* XXX: Split this into observer and model classes */
  
/* Default is to leave well enough alone */
static BoolOption DisableVirusOption (false, 'A', "disable-buggy-antivirus", "Disable known or suspected buggy anti virus software packages during execution.");

static bool KnownAVIsPresent = false;
static bool AVRunning = true;
static SC_HANDLE SCM = NULL;
static SC_HANDLE McAfeeService = NULL;
static void detect();

static int rb[] =
{ IDC_DISABLE_AV, IDC_LEAVE_AV, 0};

static int disableAV = IDC_LEAVE_AV;

static void
load_dialog (HWND h)
{
  rbset (h, rb, disableAV);
}

static void
save_dialog (HWND h)
{
  disableAV = rbget (h, rb);
}

static BOOL
dialog_cmd (HWND h, int id, HWND hwndctl, UINT code)
{
  switch (id)
    {

    case IDC_DISABLE_AV:
    case IDC_LEAVE_AV:
      save_dialog (h);
      break;

    default:
      break;
    }
  return 0;
}

bool
AntiVirusPage::Create ()
{
    detect();
    return PropertyPage::Create (NULL, dialog_cmd, IDD_VIRUS);
}

void
AntiVirusPage::OnActivate ()
{
  load_dialog (GetHWND ());
  // Check to see if any radio buttons are selected. If not, select a default.
  if ((!SendMessage
       (GetDlgItem (IDC_DISABLE_AV), BM_GETCHECK, 0,
	0) == BST_CHECKED)
      && (!SendMessage (GetDlgItem (IDC_LEAVE_AV), BM_GETCHECK, 0, 0)
	  == BST_CHECKED))
    {
      SendMessage (GetDlgItem (IDC_LEAVE_AV), BM_SETCHECK,
		   BST_CHECKED, 0);
    }

}

long
AntiVirusPage::OnNext ()
{
  HWND h = GetHWND ();

  save_dialog (h);
  /* if disable, do so now */
  return IDD_SOURCE;
}

long
AntiVirusPage::OnBack ()
{
  save_dialog (GetHWND ());
  return 0;
}

void
AntiVirusPage::OnDeactivate ()
{
  if (!KnownAVIsPresent)
	return;
  if (disableAV == IDC_LEAVE_AV)
      return;

  SERVICE_STATUS status;
  if (!ControlService (McAfeeService, SERVICE_CONTROL_STOP, &status) &&
      GetLastError() != ERROR_SERVICE_NOT_ACTIVE)
    {
      log (LOG_PLAIN, "Could not stop McAfee service, disabled AV logic\n");
      disableAV = IDC_LEAVE_AV;
      return;
    }
	
  AVRunning = false;
  log (LOG_PLAIN, String ("Disabled Anti Virus software"));
}

long
AntiVirusPage::OnUnattended ()
{
    if (!KnownAVIsPresent)
	return OnNext();
    if ((bool)DisableVirusOption)
	disableAV = IDC_DISABLE_AV;
    else
      	disableAV = IDC_LEAVE_AV;
  return OnNext();
}

void
detect ()
{
    if (Win32::OS () == Win32::Win9x)
	return;
    // TODO: trim the access rights down 
    SCM = OpenSCManager (NULL, NULL, SC_MANAGER_ALL_ACCESS);

    if (!SCM) {
	log (LOG_PLAIN, String ("Could not open Service control manager\n"));
	return;
    }
    
    /* in future, factor this to a routine to find service foo (ie norton, older
       mcafee etc 
       */
    McAfeeService = OpenService (SCM, "AvSynMgr", 
	SERVICE_QUERY_STATUS| SERVICE_STOP| SERVICE_START);

    if (!McAfeeService) {
	log (LOG_PLAIN, String("Could not open service McShield for query, start and stop. McAfee may not be installed, or we don't have access.\n"));
	CloseServiceHandle(SCM);
	return;
    }

    SERVICE_STATUS status;

    if (!QueryServiceStatus (McAfeeService, &status))
      {
	CloseServiceHandle(SCM);
	CloseServiceHandle(McAfeeService);
	log (LOG_PLAIN, String("Couldn't determine status of McAfee service.\n"));
	return;
      }

    if (status.dwCurrentState == SERVICE_STOPPED ||
	status.dwCurrentState == SERVICE_STOP_PENDING) 
      {
	CloseServiceHandle(SCM);
	CloseServiceHandle(McAfeeService);
	log (LOG_PLAIN, "Mcafee is already stopped, nothing to see here\n");
      }
    
    log (LOG_PLAIN, "Found McAfee anti virus program\n");
    KnownAVIsPresent = true;
}

bool
AntiVirus::Show()
{
    return KnownAVIsPresent;
}

void
AntiVirus::AtExit()
{
    if (!KnownAVIsPresent)
	return;
    if (disableAV == IDC_LEAVE_AV)
	return;
    if (AVRunning == true)
	return;

    if (!StartService(McAfeeService, 0, NULL))
        {
	  log (LOG_PLAIN, "Could not start McAfee service again, disabled AV logic\n");
	  disableAV = IDC_LEAVE_AV;
	  return;
	}

    log (LOG_PLAIN, String ("Enabled Anti Virus software"));  
    
    AVRunning = true;
	
}
