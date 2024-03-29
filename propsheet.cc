/*
 * Copyright (c) 2001, 2002, 2003 Gary R. Van Sickle.
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
#include "resource.h"
#include "RECTWrapper.h"
#include "ControlAdjuster.h"
#include "choose.h"
#include "msg.h"
#include "quiet.h"
#include "windowsx.h"

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
}

PropSheet::~PropSheet ()
{
}

HPROPSHEETPAGE *
PropSheet::CreatePages ()
{
  HPROPSHEETPAGE *retarray;

  // Create the return array
  retarray = new HPROPSHEETPAGE[PropertyPages.size()];

  // Create the pages with CreatePropertySheetPage().
  // We do it here rather than in the PropertyPages themselves
  // because, for reasons known only to Microsoft, these handles will be
  // destroyed by the property sheet before the PropertySheet() call returns,
  // at least if it's modal (don't know about modeless).
  unsigned int i;
  for (i = 0; i < PropertyPages.size(); i++)
    {
      retarray[i] =
	CreatePropertySheetPageW (PropertyPages[i]->GetPROPSHEETPAGEPtr ());

      // Set position info
      if (i == 0)
	{
	  PropertyPages[i]->YouAreFirst ();
	}
      else if (i == PropertyPages.size() - 1)
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

// Stuff needed by the PropSheet wndproc hook
struct PropSheetData
{
  WNDPROC oldWndProc;
  bool clientRectValid;
  RECTWrapper lastClientRect;
  bool gotPage;
  RECTWrapper pageRect;
  bool hasMinRect;
  RECTWrapper minRect;

  PropSheetData ()
  {
    oldWndProc = 0;
    clientRectValid = false;
    gotPage = false;
    hasMinRect = false;
  }

// @@@ Ugly. Really only works because only one PS is used now.
  static PropSheetData& Instance()
  {
    static PropSheetData TheInstance;
    return TheInstance;
  }
};

static ControlAdjuster::ControlInfo PropSheetControlsInfo[] = {
  {0x3023, CP_RIGHT,   CP_BOTTOM},	// Back
  {0x3024, CP_RIGHT,   CP_BOTTOM},	// Next
  {0x3025, CP_RIGHT,   CP_BOTTOM},	// Finish
  {0x3026, CP_STRETCH, CP_BOTTOM},	// Line above buttons
  {	2, CP_RIGHT,   CP_BOTTOM},	// Cancel
  {0, CP_LEFT, CP_TOP}
};

static bool IsDialog (HWND hwnd)
{
  char className[7];
  GetClassName (hwnd, className, sizeof (className));

  return (strcmp (className, "#32770") == 0);
}

BOOL CALLBACK EnumPages (HWND hwnd, LPARAM lParam)
{
  // Is it really a dialog?
  if (IsDialog (hwnd))
    {
      PropSheetData& psd = PropSheetData::Instance();
      SetWindowPos (hwnd, 0, psd.pageRect.left, psd.pageRect.top,
	psd.pageRect.width (), psd.pageRect.height (),
	SWP_NOACTIVATE | SWP_NOZORDER);
    }

  return TRUE;
}

static void
GetSizeGripRect(HWND hwnd, LPRECT rect)
{
  GetClientRect(hwnd, rect);
  rect->left = rect->right - GetSystemMetrics(SM_CXHSCROLL);
  rect->top = rect->bottom - GetSystemMetrics(SM_CYVSCROLL);
}

static LRESULT CALLBACK PropSheetWndProc (HWND hwnd, UINT uMsg,
  WPARAM wParam, LPARAM lParam)
{
  PropSheetData& psd = PropSheetData::Instance();
  switch (uMsg)
    {
    case WM_SYSCOMMAND:
      if ((wParam & 0xfff0) == SC_CLOSE)
	goto areyousure;
      break;
    case WM_COMMAND:
      if (wParam != 2)
	break;
    areyousure:
      if (unattended_mode == unattended)
        return 0;
      if (mbox(hwnd, IDS_CONFIRM_EXIT, MB_YESNO) == IDNO)
	return 0;
      break;
    case WM_SIZE:
      {
	RECTWrapper clientRect;
	GetClientRect (hwnd, &clientRect);

	/*
	  When the window is minimized, the client rect is reduced to
	  (0,0-0,0), which causes child adjusting to screw slightly up. Work
	  around by not adjusting child upon minimization - it isn't really
	  needed then, anyway.
	 */
	if (wParam != SIZE_MINIMIZED)
	  {
	    /*
	      The first time we get a WM_SIZE, the client rect will be all zeros.
	     */
	    if (psd.clientRectValid)
	      {
		const int dX =
		  clientRect.width () - psd.lastClientRect.width ();
		const int dY =
		  clientRect.height () - psd.lastClientRect.height ();

		ControlAdjuster::AdjustControls (hwnd, PropSheetControlsInfo,
		  dX, dY);

		psd.pageRect.right += dX;
		psd.pageRect.bottom += dY;

		/*
		  The pages are child windows, but don't have IDs.
		  So change them by enumerating all childs and adjust all
		  dialogs among them.
		 */
		if (psd.gotPage)
		  EnumChildWindows (hwnd, &EnumPages, 0);
	      }
	    else
	      {
		psd.clientRectValid = true;
	      }
	    /*
	      Store away the current size and use it as the minmal window size.
	     */
	    if (!psd.hasMinRect)
	      {
		GetWindowRect (hwnd, &psd.minRect);
		psd.hasMinRect = true;
	      }

	    psd.lastClientRect = clientRect;
	  }
      }
      break;
    case WM_GETMINMAXINFO:
      {
	if (psd.hasMinRect)
	  {
	    LPMINMAXINFO mmi = (LPMINMAXINFO)lParam;
	    mmi->ptMinTrackSize.x = psd.minRect.width ();
	    mmi->ptMinTrackSize.y = psd.minRect.height ();
	  }
      }
      break;
    case WM_PAINT:
      {
        RECT rect;
        GetSizeGripRect(hwnd, &rect);

        // draw size grip
        HDC dc = GetDC(hwnd);
        DrawFrameControl(dc, &rect, DFC_SCROLL, DFCS_SCROLLSIZEGRIP);
        ReleaseDC(hwnd, dc);
      }
      break;
    case WM_NCHITTEST:
      {
        RECT rect;
        GetSizeGripRect(hwnd, &rect);
        MapWindowPoints(hwnd, NULL, reinterpret_cast<POINT *>(&rect), 2);

        // when over the size grip, pretend to be over bottom-right corner of
        // the sizing border
        POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        if (PtInRect(&rect, point))
            return HTBOTTOMRIGHT;

        // otherwise, fall through...
      }
      break;
    case WM_SIZING:
      {
        // invalidate the area containing the size grip when the window is
        // resized, so it's redrawn
        RECT rect;
        GetSizeGripRect(hwnd, &rect);
        InvalidateRect(hwnd, &rect, FALSE);
      }
      break;
    }

  return CallWindowProc (psd.oldWndProc,
    hwnd, uMsg, wParam, lParam);
}

static int CALLBACK
PropSheetProc (HWND hwndDlg, UINT uMsg, LPARAM lParam)
{
  switch (uMsg)
    {
    case PSCB_PRECREATE:
      {
        LONG additionalStyle = (WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME);

        // 'noinput' quiet mode adds WS_DISABLE to prevent user interaction
        // with the propsheet
        if (UnattendedOption == QuietNoInput)
          additionalStyle |= WS_DISABLED;

        // 'hidden' quiet mode totally hides the propsheet window
        LONG forbiddenStyle = 0;
        if (UnattendedOption == QuietHidden)
          forbiddenStyle |= WS_VISIBLE;

	// Add a minimize box to the sheet/wizard.
	if (((LPDLGTEMPLATEEX) lParam)->signature == 0xFFFF)
	  {
	    ((LPDLGTEMPLATEEX) lParam)->style |= additionalStyle;
            ((LPDLGTEMPLATEEX) lParam)->style &= ~forbiddenStyle;
	  }
	else
	  {
	    ((LPDLGTEMPLATE) lParam)->style |= additionalStyle;
            ((LPDLGTEMPLATE) lParam)->style &= ~forbiddenStyle;
	  }
      }
      return TRUE;
    case PSCB_INITIALIZED:
      {
        /*
          PropSheet() with PSH_USEICONID only sets the small icon,
          so we must set the big icon ourselves
        */
        SendMessage(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_CYGWIN)));
	/*
	  Hook into the window proc.
	  We need to catch some messages for resizing.
	 */
        PropSheetData::Instance().oldWndProc = (WNDPROC)GetWindowLongPtrW (hwndDlg, GWLP_WNDPROC);
        SetWindowLongPtrW (hwndDlg, GWLP_WNDPROC, (LONG_PTR)&PropSheetWndProc);
        /*
          Store the PropSheet HWND in the Chooser, for use in resizing

          (XXX: this is just silly: The PropSheet HWND is the parent of the
          PropPage HWND, so chooser has access to that HWND as GetParent()
          anyhow... )
         */
	ChooserPage::SetHwndDialog (hwndDlg);
      }
      return TRUE;
    }
  return TRUE;
}

bool
PropSheet::Create (const Window * Parent, DWORD Style)
{
  PROPSHEETHEADERW p;

  PageHandles = CreatePages ();

  p.dwSize = sizeof (PROPSHEETHEADERW);
  p.dwFlags = PSH_NOAPPLYNOW | PSH_WIZARD | PSH_USECALLBACK
    /*| PSH_MODELESS */ | PSH_USEICONID;
  if (Parent != NULL)
    {
      p.hwndParent = Parent->GetHWND ();
    }
  else
    {
      p.hwndParent = NULL;
    }
  p.hInstance = GetInstance ();
  p.nPages = PropertyPages.size();
  p.pszIcon = MAKEINTRESOURCEW(IDI_CYGWIN);
  p.nStartPage = 0;
  p.phpage = PageHandles;
  p.pfnCallback = PropSheetProc;

  // The winmain event loop actually resides in here.
  PropertySheetW (&p);

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

/*
  Adjust the size of a page so that it fits nicely into the window.
 */
void
PropSheet::AdjustPageSize (HWND page)
{
  PropSheetData& psd = PropSheetData::Instance();
  if (!psd.clientRectValid) return;

  /*
    It's probably not obvious what's done here:
    When this method is called the first time, the first page is already
    created and sized, but at the coordinates (0,0). The sheet, however,
    isn't in it's final size. My guess is that the sheet first creates the
    page, and then resizes itself to have the right metrics to contain the
    page and moves it to it's position. For our purposes, however, we need
    the final metrics of the page. So, the first time this method is called,
    we basically grab the size of the page, but calculate the top/left coords
    ourselves.
   */

  if (!psd.gotPage)
    {
      psd.gotPage = true;

      RECTWrapper& pageRect = psd.pageRect;
      ::GetWindowRect (page, &pageRect);
      // We want client coords.
      ::ScreenToClient (page, (LPPOINT)&pageRect.left);
      ::ScreenToClient (page, (LPPOINT)&pageRect.right);

      LONG dialogBaseUnits = ::GetDialogBaseUnits ();
      // The margins in DUs are a result of "educated guesses" and T&E.
      int marginX = MulDiv (5, LOWORD(dialogBaseUnits), 4);
      int marginY = MulDiv (5, HIWORD(dialogBaseUnits), 8);

      pageRect.move (marginX, marginY);
    }

  SetWindowPos (page, 0, psd.pageRect.left, psd.pageRect.top,
    psd.pageRect.width (), psd.pageRect.height (),
    SWP_NOACTIVATE | SWP_NOZORDER);
}

void
PropSheet::AddPage (PropertyPage * p)
{
  // Add a page to the property sheet.
  p->YouAreBeingAddedToASheet (this);
  PropertyPages.push_back(p);
}

bool
PropSheet::SetActivePage (int i)
{
  // Posts a message to the message queue, so this won't block
  return static_cast < bool > (PropSheet_SetCurSel (GetHWND (), NULL, i));
}

bool
PropSheet::SetActivePageByID (int resource_id)
{
  // Posts a message to the message queue, so this won't block
  return static_cast < bool >
    (PropSheet_SetCurSelByID (GetHWND (), resource_id));
}

void
PropSheet::SetButtons (DWORD flags)
{
  // Posts a message to the message queue, so this won't block
  PropSheet_SetWizButtons (GetHWND (), flags);
}

void
PropSheet::PressButton (int button)
{
  PropSheet_PressButton (GetHWND (), button);
}
