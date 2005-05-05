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

// This is the implementation of the Window class.  It serves both as a window class
// in its own right and as a base class for other window-like classes (e.g. PropertyPage,
// PropSheet).

#include "win32.h"
#include "window.h"
#include "String++.h"
#include "RECTWrapper.h"
#include "msg.h"
#include "resource.h"

ATOM Window::WindowClassAtom = 0;
HINSTANCE Window::AppInstance = NULL;

Window::Window ()
{
  WindowHandle = NULL;
  Parent = NULL;
}

Window::~Window ()
{
  // Delete any fonts we created.
  for (unsigned int i = 0; i < Fonts.size (); i++)
    DeleteObject (Fonts[i]);

  // FIXME: Maybe do some reference counting and do this Unregister
  // when there are no more of us left.  Not real critical unless
  // we're in a DLL which we're not right now.
  //UnregisterClass(WindowClassAtom, InstanceHandle);
}

LRESULT CALLBACK
Window::FirstWindowProcReflector (HWND hwnd, UINT uMsg, WPARAM wParam,
				  LPARAM lParam)
{
  Window *wnd = NULL;

  if(uMsg == WM_NCCREATE)
    {
      // This is the first message a window gets (so MSDN says anyway).
      // Take this opportunity to "link" the HWND to the 'this' ptr, steering
      // messages to the class instance's WindowProc().
      wnd = reinterpret_cast<Window *>(((LPCREATESTRUCT)lParam)->lpCreateParams);

      // Set a backreference to this class instance in the HWND.
      SetWindowLongPtr (hwnd, GWL_USERDATA, reinterpret_cast<LONG_PTR>(wnd));

      // Set a new WindowProc now that we have the peliminaries done.
      // We could instead simply do the contents of Window::WindowProcReflector
      // in the 'else' clause below, but this way we eliminate an unnecessary 'if/else' on
      // every message.  Yeah, it's probably not worth the trouble.
      SetWindowLongPtr (hwnd, GWL_WNDPROC, (LONG_PTR) & Window::WindowProcReflector);
      // Finally, store the window handle in the class.
      wnd->WindowHandle = hwnd;
    }
  else
    {
      // Should never get here.
      fatal(NULL, IDS_WINDOW_INIT_BADMSG, uMsg);
    }

  return wnd->WindowProc (uMsg, wParam, lParam);
}

LRESULT CALLBACK
Window::WindowProcReflector (HWND hwnd, UINT uMsg, WPARAM wParam,
			     LPARAM lParam)
{
  Window *This;

  // Get our this pointer
  This = reinterpret_cast<Window *>(GetWindowLongPtr (hwnd, GWL_USERDATA));

  return This->WindowProc (uMsg, wParam, lParam);
}

bool
Window::Create (Window * parent, DWORD Style)
{
  // First register the window class, if we haven't already
  if (registerWindowClass () == false)
    {
      // Registration failed
      return false;
    }

  // Save our parent, we'll probably need it eventually.
  Parent = parent;

  // Create the window instance
  WindowHandle = CreateWindowEx (
                   // Extended Style
                   0,
                   "MainWindowClass",	//MAKEINTATOM(WindowClassAtom),     // window class atom (name)
			       "Hello",	// no title-bar string yet
			       // Style bits
			       Style,
			       // Default positions and size
			       CW_USEDEFAULT,
			       CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			       // Parent Window 
			       parent ==
			       NULL ? (HWND) NULL : parent->GetHWND (),
			       // use class menu 
			       (HMENU) NULL,
			       // The application instance 
			       GetInstance (),
			       // The this ptr, which we'll use to set up the WindowProc reflection.
			       (LPVOID) this);

  if (WindowHandle == NULL)
    {
      // Failed
      return false;
    }

  return true;
}

bool
Window::registerWindowClass ()
{
  if (WindowClassAtom == 0)
    {
      // We're not registered yet
      WNDCLASSEX
	wc;

      wc.cbSize = sizeof (wc);
      // Some sensible style defaults
      wc.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
      // Our default window procedure.  This replaces itself
      // on the first call with the simpler Window::WindowProcReflector().
      wc.lpfnWndProc = Window::FirstWindowProcReflector;
      // No class bytes
      wc.cbClsExtra = 0;
      // One pointer to REFLECTION_INFO in the extra window instance bytes
      wc.cbWndExtra = 4;
      // The app instance
      wc.hInstance = GetInstance ();
      // Use a bunch of system defaults for the GUI elements
      wc.hIcon = NULL;
      wc.hIconSm = NULL;
      wc.hCursor = NULL;
      wc.hbrBackground = (HBRUSH) (COLOR_BACKGROUND + 1);
      // No menu
      wc.lpszMenuName = NULL;
      // We'll get a little crazy here with the class name
      wc.lpszClassName = "MainWindowClass";

      // All set, try to register
      WindowClassAtom = RegisterClassEx (&wc);

      if (WindowClassAtom == 0)
	{
	  // Failed
	  return false;
	}
    }

  // We're registered, or already were before the call,
  // return success in either case.
  return true;
}

void
Window::Show (int State)
{
  ::ShowWindow (WindowHandle, State);
}

RECT
Window::GetWindowRect() const
{
  RECT retval;
  ::GetWindowRect(WindowHandle, &retval);
  return retval;
}

RECT
Window::GetClientRect() const
{
  RECT retval;
  ::GetClientRect(WindowHandle, &retval);
  return retval;
}

bool
Window::MoveWindow(long x, long y, long w, long h, bool Repaint)
{
  return ::MoveWindow (WindowHandle, x, y, w, h, Repaint);
}

bool
Window::MoveWindow(const RECTWrapper &r, bool Repaint)
{
  return ::MoveWindow (WindowHandle, r.left, r.top, r.width(), r.height(), Repaint);
}

void
Window::CenterWindow ()
{
  RECT WindowRect, ParentRect;
  int WindowWidth, WindowHeight;
  POINT p;

  // Get the window rectangle
  WindowRect = GetWindowRect ();

  if (GetParent () == NULL)
    {
      // Center on desktop window
      ::GetWindowRect (GetDesktopWindow (), &ParentRect);
    }
  else
    {
      // Center on client area of parent
      ::GetClientRect (GetParent ()->GetHWND (), &ParentRect);
    }

  WindowWidth = WindowRect.right - WindowRect.left;
  WindowHeight = WindowRect.bottom - WindowRect.top;

  // Find center of area we're centering on
  p.x = (ParentRect.right - ParentRect.left) / 2;
  p.y = (ParentRect.bottom - ParentRect.top) / 2;

  // Convert that to screen coords
  if (GetParent () == NULL)
    {
      ClientToScreen (GetDesktopWindow (), &p);
    }
  else
    {
      ClientToScreen (GetParent ()->GetHWND (), &p);
    }

  // Calculate new top left corner for window
  p.x -= WindowWidth / 2;
  p.y -= WindowHeight / 2;

  // And finally move the window
  MoveWindow (p.x, p.y, WindowWidth, WindowHeight);
}

LRESULT
Window::WindowProc (UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  return DefWindowProc (WindowHandle, uMsg, wParam, lParam);
}

bool
Window::MessageLoop ()
{
  MSG
    msg;

  while (GetMessage (&msg, NULL, 0, 0) != 0
	 && GetMessage (&msg, (HWND) NULL, 0, 0) != -1)
    {
      if (!IsWindow (WindowHandle) || !IsDialogMessage (WindowHandle, &msg))
	{
	  TranslateMessage (&msg);
	  DispatchMessage (&msg);
	}
    }

  return true;
}

void
Window::PostMessage (UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  ::PostMessage (GetHWND (), uMsg, wParam, lParam);
}

UINT
Window::IsButtonChecked (int nIDButton) const
{
  return ::IsDlgButtonChecked (GetHWND (), nIDButton);
}

bool
Window::SetDlgItemFont (int id, const TCHAR * fontname, int Pointsize,
			  int Weight, bool Italic, bool Underline,
			  bool Strikeout)
{
  HWND ctrl;

  ctrl = GetDlgItem (id);
  if (ctrl == NULL)
    {
      // Couldn't get that ID
      return false;
    }

  // We need the DC for the point size calculation.
  HDC hdc = GetDC (ctrl);

  // Create the font.  We have to keep it around until the dialog item
  // goes away - basically until we're destroyed.
  HFONT hfnt;
  hfnt =
    CreateFont (-MulDiv (Pointsize, GetDeviceCaps (hdc, LOGPIXELSY), 72), 0,
		0, 0, Weight, Italic ? TRUE : FALSE,
		Underline ? TRUE : FALSE, Strikeout ? TRUE : FALSE,
		ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
		PROOF_QUALITY, DEFAULT_PITCH | FF_DONTCARE, fontname);
  if (hfnt == NULL)
    {
      // Font creation failed
      return false;
    }

  // Set the new font, and redraw any text which was already in the item.
  SendMessage (ctrl, WM_SETFONT, (WPARAM) hfnt, TRUE);

  // Store the handle so that we can DeleteObject() it in dtor
  Fonts.push_back (hfnt);

  return true;
}

void
Window::SetWindowText (const String & s)
{
  ::SetWindowText (WindowHandle, s.c_str ());
}

RECT
Window::ScreenToClient(const RECT &r) const
{
  POINT tl;
  POINT br;
  
  tl.y = r.top;
  tl.x = r.left;
  ::ScreenToClient(GetHWND(), &tl);
  br.y = r.bottom;
  br.x = r.right;
  ::ScreenToClient(GetHWND(), &br);
  
  RECT ret;
  
  ret.top = tl.y;
  ret.left = tl.x;
  ret.bottom = br.y;
  ret.right = br.x;
  
  return ret;
}

