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

// This is the implementation of the PropertyPage class.  It works closely with the
// PropSheet class to implement a single page of the property sheet.

#include "proppage.h"
#include "propsheet.h"
#include "win32.h"
#include "resource.h"
#include "state.h"

#include "getopt++/BoolOption.h"

bool PropertyPage::DoOnceForSheet = true;

PropertyPage::PropertyPage ()
{
  proc = NULL;
  cmdproc = NULL;
  IsFirst = false;
  IsLast = false;
}

PropertyPage::~PropertyPage ()
{
}

bool PropertyPage::Create (int TemplateID)
{
  return Create (NULL, NULL, TemplateID);
}

bool PropertyPage::Create (DLGPROC dlgproc, int TemplateID)
{
  return Create (dlgproc, NULL, TemplateID);
}

bool
PropertyPage::Create (DLGPROC dlgproc,
			BOOL (*cproc) (HWND h, int id, HWND hwndctl,
				       UINT code), int TemplateID)
{
  psp.dwSize = sizeof (PROPSHEETPAGE);
  psp.dwFlags = 0;
  psp.hInstance = GetInstance ();
  psp.pfnDlgProc = FirstDialogProcReflector;
  psp.pszTemplate = MAKEINTRESOURCE(TemplateID);
  psp.lParam = (LPARAM) this;
  psp.pfnCallback = NULL;

  proc = dlgproc;
  cmdproc = cproc;

  return true;
}

BOOL CALLBACK
PropertyPage::FirstDialogProcReflector (HWND hwnd, UINT message,
					WPARAM wParam, LPARAM lParam)
{
  PropertyPage *This;

  if (message != WM_INITDIALOG)
    {
      // Don't handle anything until we get a WM_INITDIALOG message, which
      // will have our 'this' pointer with it.
      return FALSE;
    }

  This = (PropertyPage *) (((PROPSHEETPAGE *) lParam)->lParam);

  SetWindowLong (hwnd, DWL_USER, (DWORD) This);
  SetWindowLong (hwnd, DWL_DLGPROC, (DWORD) DialogProcReflector);

  This->SetHWND (hwnd);
  return This->DialogProc (message, wParam, lParam);
}

BOOL CALLBACK
PropertyPage::DialogProcReflector (HWND hwnd, UINT message, WPARAM wParam,
				   LPARAM lParam)
{
  PropertyPage *This;

  This = (PropertyPage *) GetWindowLong (hwnd, DWL_USER);

  return This->DialogProc (message, wParam, lParam);
}

BOOL CALLBACK
PropertyPage::DialogProc (UINT message, WPARAM wParam, LPARAM lParam)
{
  if (proc != NULL)
    {
      proc (GetHWND (), message, wParam, lParam);
    }

  bool retval;

  switch (message)
    {
    case WM_INITDIALOG:
      {
	OnInit ();

	// Set header title font of each internal page to MS Sans Serif, Bold, 8 Pt.
	// This will just silently fail on the first and last pages.
	SetDlgItemFont(IDC_STATIC_HEADER_TITLE, "MS Sans Serif", 8, FW_BOLD);

	// TRUE = Set focus to default control (in wParam).
	return TRUE;
	break;
      }
    case WM_NOTIFY:
      switch (((NMHDR FAR *) lParam)->code)
	{
	case PSN_APPLY:
	  SetWindowLong (GetHWND (), DWL_MSGRESULT, PSNRET_NOERROR);
	  return TRUE;
	  break;
	case PSN_SETACTIVE:
	  {
	    if (DoOnceForSheet)
	      {
		// Tell our parent PropSheet what its own HWND is.
		GetOwner ()->SetHWNDFromPage (((NMHDR FAR *) lParam)->
					      hwndFrom);
		GetOwner ()->CenterWindow ();
		DoOnceForSheet = false;
	      }

	    // Set the wizard buttons apropriately
	    if (IsFirst)
	      {
		// Disable "Back" on first page.
		GetOwner ()->SetButtons (PSWIZB_NEXT);
	      }
	    else if (IsLast)
	      {
		// Disable "Next", enable "Finish" on last page
		GetOwner ()->SetButtons (PSWIZB_BACK | PSWIZB_FINISH);
	      }
	    else
	      {
		// Middle page, enable both "Next" and "Back" buttons
		GetOwner ()->SetButtons (PSWIZB_BACK | PSWIZB_NEXT);
	      }

	    if(!wantsActivation())
	    {
	      ::SetWindowLong (GetHWND (), DWL_MSGRESULT, -1);
	      return TRUE;
	    }

	    OnActivate ();

	    if (unattended_mode) 
	      {
		// -2 == disable unattended mode, display page
		// -1 == display page but stay in unattended mode (progress bars)
		// 0 == skip to next page
		// IDD_* == skip to specified page
		long nextwindow = OnUnattended();
		if (nextwindow == -2)
		  {
		    unattended_mode = false;
		    SetWindowLong (GetHWND (), DWL_MSGRESULT, 0);
		    return TRUE;
		  }
		else if (nextwindow == -1)
		  {
		    SetWindowLong (GetHWND (), DWL_MSGRESULT, 0);
		    return TRUE;
		  }
		else if (nextwindow == 0)
		  {
		    SetWindowLong (GetHWND (), DWL_MSGRESULT, -1);
		    return TRUE;
		  }
		else
		  {
		    SetWindowLong (GetHWND (), DWL_MSGRESULT, nextwindow);
		    return TRUE;
		  }
	      } 
	    else 
	      {
		// 0 == Accept activation, -1 = Don't accept
		::SetWindowLong (GetHWND (), DWL_MSGRESULT, 0);
		return TRUE;
	      }

	  }
	  break;
	case PSN_KILLACTIVE:
	  OnDeactivate ();
	  // FALSE = Allow deactivation
	  SetWindowLong (GetHWND (), DWL_MSGRESULT, FALSE);
	  return TRUE;
	  break;
	case PSN_WIZNEXT:
	  {
	    LONG retval;
	    retval = OnNext ();
	    SetWindowLong (GetHWND (), DWL_MSGRESULT, retval);
	    return TRUE;
	  }
	  break;
	case PSN_WIZBACK:
	  {
	    LONG retval;
	    retval = OnBack ();
	    SetWindowLong (GetHWND (), DWL_MSGRESULT, retval);
	    return TRUE;
	  }
	  break;
	case PSN_WIZFINISH:
	  retval = OnFinish ();
	  // False = Allow the wizard to finish
	  SetWindowLong (GetHWND (), DWL_MSGRESULT, FALSE);
	  return TRUE;
	  break;
	default:
	  // Unrecognized notification
	  return FALSE;
	  break;
	}
      break;
    case WM_COMMAND:
      {
	bool retval;

	retval =
	  OnMessageCmd (LOWORD (wParam), (HWND) lParam, HIWORD (wParam));
	if (retval == true)
	  {
	    // Handled, return 0
	    SetWindowLong (GetHWND (), DWL_MSGRESULT, 0);
	    return TRUE;
	  }
	else if (cmdproc != NULL)
	  {
	    return HANDLE_WM_COMMAND (GetHWND (), wParam, lParam, cmdproc);
	  }
	break;
      }
    default:
      break;
    }

  if ((message >= WM_APP) && (message < 0xC000))
    {
      // It's a private app message
      return OnMessageApp (message, wParam, lParam);
    }

  // Wasn't handled
  return FALSE;
}
