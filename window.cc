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

// This is the implementation of the Window class.  It serves both as a window class
// in its own right and as a base class for other window-like classes (e.g. PropertyPage,
// PropSheet).

#include <windows.h>
#include "window.h"

ATOM Window::WindowClassAtom = 0;
HINSTANCE Window::AppInstance = NULL;

// FIXME: I know, this is brutal.  Mutexing should at least make window creation threadsafe,
// but if somebody has any ideas as to how to get rid of it entirely, please tell me / do so.
struct REFLECTION_INFO
{
  Window *
    This;
  bool
    FirstCall;
};
REFLECTION_INFO ReflectionInfo;

Window::Window ()
{
  WindowHandle = NULL;
  Parent = NULL;
}

Window::~Window ()
{
  // FIXME: Maybe do some reference counting and do this Unregister
  // when there are no more of us left.  Not real critical unless
  // we're in a DLL which we're not right now.
  //UnregisterClass(WindowClassAtom, InstanceHandle);
}

LRESULT CALLBACK
Window::FirstWindowProcReflector (HWND hwnd, UINT uMsg, WPARAM wParam,
				  LPARAM lParam)
{
  // Get our this pointer
  REFLECTION_INFO *rip = &ReflectionInfo;

  if (rip->FirstCall)
    {
      rip->FirstCall = false;

      // Set the Window handle so the real WindowProc has one to work with.
      rip->This->WindowHandle = hwnd;

      // Set a backreference to this class instance in the HWND.
      // FIXME: Should really be SetWindowLongPtr(), but it appears to
      // not be defined yet.
      SetWindowLong (hwnd, GWL_USERDATA, (LONG) rip->This);

      // Set a new WindowProc now that we have the peliminaries done.
      // Like subclassing, only not.
      SetWindowLong (hwnd, GWL_WNDPROC, (LONG) & Window::WindowProcReflector);
    }

  return rip->This->WindowProc (uMsg, wParam, lParam);
}

LRESULT CALLBACK
Window::WindowProcReflector (HWND hwnd, UINT uMsg, WPARAM wParam,
			     LPARAM lParam)
{
  Window *This;

  // Get our this pointer
  // FIXME: Should really be GetWindowLongPtr(), but it appears to
  // not be defined yet.
  This = (Window *) GetWindowLong (hwnd, GWL_USERDATA);

  return This->WindowProc (uMsg, wParam, lParam);
}

bool Window::Create (Window * parent, DWORD Style)
{
  // First register the window class, if we haven't already
  if (RegisterWindowClass () == false)
    {
      // Registration failed
      return false;
    }

  // Set up the reflection info, so that the Windows window can find us.
  ReflectionInfo.This = this;
  ReflectionInfo.FirstCall = true;

  Parent = parent;

  // Create the window instance
  WindowHandle = CreateWindow ("MainWindowClass",	//MAKEINTATOM(WindowClassAtom),     // window class atom (name)
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
			       GetInstance (), (LPVOID) NULL);

  if (WindowHandle == NULL)
    {
      // Failed
      return false;
    }

  return true;
}

bool Window::RegisterWindowClass ()
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

void
Window::CenterWindow ()
{
  RECT WindowRect, ParentRect;
  int WindowWidth, WindowHeight;
  POINT p;

  // Get the window rectangle
  GetWindowRect (GetHWND (), &WindowRect);

  if (GetParent () == NULL)
    {
      // Center on desktop window
      GetWindowRect (GetDesktopWindow (), &ParentRect);
    }
  else
    {
      // Center on client area of parent
      GetClientRect (GetParent ()->GetHWND (), &ParentRect);
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
  MoveWindow (GetHWND (), p.x, p.y, WindowWidth, WindowHeight, TRUE);
}

LRESULT Window::WindowProc (UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
    {
    default:
      return DefWindowProc (WindowHandle, uMsg, wParam, lParam);
    }

  return 0;
}

bool Window::MessageLoop ()
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
