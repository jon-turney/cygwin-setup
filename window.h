#ifndef CINSTALL_WINDOW_H
#define CINSTALL_WINDOW_H

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

// This is the header for the Window class.  It serves both as a window class
// in its own right and as a base class for other window-like classes (e.g. PropertyPage,
// PropSheet).

#include <windows.h>

class Window
{
  static ATOM WindowClassAtom;
  static HINSTANCE AppInstance;


  bool RegisterWindowClass ();
  static LRESULT CALLBACK FirstWindowProcReflector (HWND hwnd, UINT uMsg,
						    WPARAM wParam,
						    LPARAM lParam);
  static LRESULT CALLBACK WindowProcReflector (HWND hwnd, UINT uMsg,
					       WPARAM wParam, LPARAM lParam);

  HWND WindowHandle;

  Window *Parent;

  // FIXME: replace with <vector> when we get a chance.
  static const int MAXFONTS = 5;
  HFONT Fonts[MAXFONTS];
  int FontCounter;

protected:
  void SetHWND (HWND h)
  {
    WindowHandle = h;
  };

public:
  Window ();
  virtual ~ Window ();

  static void SetAppInstance (HINSTANCE h)
  {
    AppInstance = h;
  };

  virtual LRESULT WindowProc (UINT uMsg, WPARAM wParam, LPARAM lParam);
  virtual bool MessageLoop ();

  virtual bool Create (Window * Parent = NULL,
		       DWORD Style =
		       WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN);
  void Show (int State);

  HWND GetHWND () const
  {
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
  bool SetDlgItemFont(int id, const TCHAR *fontname, int Pointsize,
	  int Weight = FW_NORMAL, bool Italic = false, bool Underline = false, bool Strikeout = false);

  UINT IsButtonChecked (int nIDButton) const;

  void PostMessage (UINT uMsg, WPARAM wParam = 0, LPARAM lParam = 0);

  virtual bool OnMessageApp (UINT uMsg, WPARAM wParam, LPARAM lParam)
  {
    return false;
  };

  virtual bool OnMessageCmd (int id, HWND hwndctl, UINT code)
  {
    // Not processed.
    return false;
  };

  // Center the window on the parent, or on screen if no parent.
  void CenterWindow ();

};

#endif // CINSTALL_WINDOW_H
