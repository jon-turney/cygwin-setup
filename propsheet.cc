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

// This is the implementation of the PropSheet class.  This class encapsulates
// a Windows property sheet / wizard and interfaces with the PropertyPage class.
// It's named PropSheet instead of PropertySheet because the latter conflicts with
// the Windows function of the same name.

#include "propsheet.h"
#include "proppage.h"

//#include <shlwapi.h>
// ...but since there is no shlwapi.h in mingw yet:
typedef struct _DllVersionInfo
{
  DWORD cbSize;
  DWORD dwMajorVersion;
  DWORD dwMinorVersion;
  DWORD dwBuildNumber;
  DWORD dwPlatformID;
}
DLLVERSIONINFO;
typedef HRESULT CALLBACK (*DLLGETVERSIONPROC) (DLLVERSIONINFO * pdvi);
#define PROPSHEETHEADER_V1_SIZE 40

// Sort of a "hidden" Windows structure.  Used in the PropSheetCallback.
#include <pshpack1.h>
typedef struct DLGTEMPLATEEX
{
  WORD dlgVer;
  WORD signature;
  DWORD helpID;
  DWORD exStyle;
  DWORD style;
  WORD cDlgItems;
  short x;
  short y;
  short cx;
  short cy;
}
DLGTEMPLATEEX, *LPDLGTEMPLATEEX;
#include <poppack.h>

PropSheet::PropSheet ()
{
  NumPropPages = 0;
}

PropSheet::~PropSheet ()
{
}

HPROPSHEETPAGE *
PropSheet::CreatePages ()
{
  HPROPSHEETPAGE *retarray;

  // Create the return array
  retarray = new HPROPSHEETPAGE[NumPropPages];

  // Create the pages with CreatePropertySheetPage().
  // We do it here rather than in the PropertyPages themselves
  // because, for reasons known only to Microsoft, these handles will be
  // destroyed by the property sheet before the PropertySheet() call returns,
  // at least if it's modal (don't know about modeless).
  int i;
  for (i = 0; i < NumPropPages; i++)
    {
      retarray[i] =
	CreatePropertySheetPage (PropertyPages[i]->GetPROPSHEETPAGEPtr ());

      // Set position info
      if (i == 0)
	{
	  PropertyPages[i]->YouAreFirst ();
	}
      else if (i == NumPropPages - 1)
	{
	  PropertyPages[i]->YouAreLast ();
	}
      else
	{
	  PropertyPages[i]->YouAreMiddle ();
	}
    }

  return retarray;
}

static int CALLBACK
PropSheetProc (HWND hwndDlg, UINT uMsg, LPARAM lParam)
{
  switch (uMsg)
    {
    case PSCB_PRECREATE:
      {
	// Add a minimize box to the sheet/wizard.
	if (((LPDLGTEMPLATEEX) lParam)->signature == 0xFFFF)
	  {
	    ((LPDLGTEMPLATEEX) lParam)->style |= WS_MINIMIZEBOX;
	  }
	else
	  {
	    ((LPDLGTEMPLATE) lParam)->style |= WS_MINIMIZEBOX;
	  }
      }
      return TRUE;
    }
  return TRUE;
}

static DWORD
GetPROPSHEETHEADERSize ()
{
  // For compatibility with all versions of comctl32.dll, we have to do this.

  DLLVERSIONINFO vi;
  HMODULE mod;
  DLLGETVERSIONPROC DllGetVersion;
  DWORD retval = 0;


  // This 'isn't safe' in a DLL, according to MSDN
  mod = LoadLibrary ("comctl32.dll");

  DllGetVersion = (DLLGETVERSIONPROC) GetProcAddress (mod, "DllGetVersion");
  if (DllGetVersion == NULL)
    {
      // Something's wildly broken, punt.
      retval = PROPSHEETHEADER_V1_SIZE;
    }
  else
    {
      vi.cbSize = sizeof (DLLVERSIONINFO);
      DllGetVersion (&vi);

      if ((vi.dwMajorVersion < 4) ||
	  ((vi.dwMajorVersion == 4) && (vi.dwMinorVersion < 71)))
	{
	  // Recent.
	  retval = sizeof (PROPSHEETHEADER);
	}
      else
	{
	  // Old (== Win95/NT4 w/o IE 4 or better)
	  retval = PROPSHEETHEADER_V1_SIZE;
	}
    }

  FreeLibrary (mod);

  return retval;
}

bool
PropSheet::Create (const Window * Parent, DWORD Style)
{
  PROPSHEETHEADER p;

  PageHandles = CreatePages ();

  p.dwSize = GetPROPSHEETHEADERSize ();
  p.dwFlags =
    PSH_NOAPPLYNOW | PSH_WIZARD | PSH_USECALLBACK /*| PSH_MODELESS */ ;
  if (Parent != NULL)
    {
      p.hwndParent = Parent->GetHWND ();
    }
  else
    {
      p.hwndParent = NULL;
    }
  p.hInstance = GetInstance ();
  p.nPages = NumPropPages;
  p.nStartPage = 0;
  p.phpage = PageHandles;
  p.pfnCallback = PropSheetProc;


  PropertySheet (&p);

  // Do a modeless property sheet...
  //SetHWND((HWND)PropertySheet(&p));
  /*Show(SW_SHOWNORMAL);

     // ...but pretend it's modal
     MessageLoop();
     MessageBox(NULL, "DONE", NULL, MB_OK);

     // FIXME: Enable the parent before destroying this window to prevent another window
     // from becoming the foreground window
     // ala: EnableWindow(<parent_hwnd>, TRUE);
     //DestroyWindow(WindowHandle);
   */
  SetHWND (NULL);


  return true;
}

void
PropSheet::SetHWNDFromPage (HWND h)
{
  // If we're a modal dialog, there's no way for us to know our window handle unless
  // one of our pages tells us through this function.
  SetHWND (h);
}

void
PropSheet::AddPage (PropertyPage * p)
{
  // Add a page to the property sheet.
  p->YouAreBeingAddedToASheet (this);
  PropertyPages[NumPropPages] = p;
  NumPropPages++;
}

bool
PropSheet::SetActivePage (int i)
{
  // Posts a message to the message queue, so this won't block
  return static_cast < bool > (::PropSheet_SetCurSel (GetHWND (), NULL, i));
}

bool
PropSheet::SetActivePageByID (int resource_id)
{
  // Posts a message to the message queue, so this won't block
  return static_cast < bool >
    (::PropSheet_SetCurSelByID (GetHWND (), resource_id));
}

void
PropSheet::SetButtons (DWORD flags)
{
  // Posts a message to the message queue, so this won't block
  ::PropSheet_SetWizButtons (GetHWND (), flags);
}

void
PropSheet::PressButton (int button)
{
  ::PropSheet_PressButton (GetHWND (), button);
}
