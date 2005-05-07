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

#ifndef SETUP_WINDOW_H
#define SETUP_WINDOW_H

// This is the header for the Window class.  It serves both as a window class
// in its own right and as a base class for other window-like classes (e.g. PropertyPage,
// PropSheet).

#include <vector>
#include <map>
#include "win32.h"
#include <commctrl.h>
#include "LogSingleton.h"

class String;
class RECTWrapper;

class Window
{
  static ATOM WindowClassAtom;
  static HINSTANCE AppInstance;

  virtual bool registerWindowClass ();
protected:
  static LRESULT CALLBACK FirstWindowProcReflector (HWND hwnd, UINT uMsg,
						    WPARAM wParam,
						    LPARAM lParam);
private:
  static LRESULT CALLBACK WindowProcReflector (HWND hwnd, UINT uMsg,
					       WPARAM wParam, LPARAM lParam);

  // Our Windows(tm) window handle.
  HWND WindowHandle;

  Window *Parent;

  // contains handles to fonts we've created
  // that are to be deleted in our dtor
  std::vector<HFONT> Fonts;

  // if we have activated tool tips this will contain the handle
  HWND TooltipHandle;
  
  // maps a control ID to a string resource ID
  std::map<int, int> TooltipStrings;
  
protected:
  void SetHWND (HWND h)
  {
    WindowHandle = h;
  };
  void setParent(Window *aParent)
  {
    Parent = aParent;
  };

public:
  Window ();
  virtual ~ Window ();

  virtual bool Create (Window * Parent = NULL,
		       DWORD Style =
		       WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN);
  
  static void SetAppInstance (HINSTANCE h)
  {
    // This only has to be called once in the entire app, before
    // any Windows are created.
    AppInstance = h;
  };

  virtual LRESULT WindowProc (UINT uMsg, WPARAM wParam, LPARAM lParam);
  virtual bool MessageLoop ();

  void Show (int State);

  HWND GetHWND () const
  {
    // Ideally this could be hidden from the user completely.
    return WindowHandle;
  };
  HINSTANCE GetInstance () const
  {
    return AppInstance;
  };

  Window *GetParent () const
  {
    return Parent;
  };
  HWND GetDlgItem (int id) const
  {
    return::GetDlgItem (GetHWND (), id);
  };
  bool SetDlgItemFont (int id, const TCHAR * fontname, int Pointsize,
		       int Weight = FW_NORMAL, bool Italic =
		       false, bool Underline = false, bool Strikeout = false);

  UINT IsButtonChecked (int nIDButton) const;

  void PostMessage (UINT uMsg, WPARAM wParam = 0, LPARAM lParam = 0);

  virtual bool OnMessageApp (UINT uMsg, WPARAM wParam, LPARAM lParam)
  {
    // Not processed by default.  Override in derived classes to
    // do something with app messages if you need to.
    return false;
  };

  virtual bool OnMessageCmd (int id, HWND hwndctl, UINT code)
  {
    // Not processed by default.  Override in derived classes to
    // do something with command messages if you need to.
    return false;
  };

  RECT GetWindowRect() const;
  RECT GetClientRect() const;

  // Center the window on the parent, or on screen if no parent.
  void CenterWindow ();

  // Reposition the window
  bool MoveWindow(long x, long y, long w, long h, bool Repaint = true);
  bool MoveWindow(const RECTWrapper &r, bool Repaint = true);

  // Set the title of the window.
  void SetWindowText (const String & s);

  RECT ScreenToClient(const RECT &r) const;
  void ActivateTooltips ();
  void SetTooltipState (bool b);
  void AddTooltip (HWND target, HWND win, const char *text);
  void AddTooltip (int id, const char *text);
  void AddTooltip (int id, int string_resource);
  BOOL TooltipNotificationHandler (LPARAM lParam);
};

#endif /* SETUP_WINDOW_H */
