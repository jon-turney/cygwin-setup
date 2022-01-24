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

/* The purpose of this file is to centralize all the message
   functions. */

#include "msg.h"

#include "LogFile.h"
#include "win32.h"
#include <shlwapi.h>

#include <stdio.h>
#include <stdarg.h>
#include "dialog.h"
#include "state.h"
#include "String++.h"
#include "resource.h"

// no prototype in shlwapi.h until MinGW64 headers 9.0.0
#if __MINGW64_VERSION_MAJOR < 9
extern "C" int WINAPI SHMessageBoxCheckW(HWND hwnd, LPCWSTR pszText, LPCWSTR pszCaption, UINT uType, int iDefault, LPCWSTR pszRegVal);
#endif

static int
unattended_result(int mb_type)
{
  // Return some default values.
  switch (mb_type & MB_TYPEMASK)
    {
    case MB_OK:
    case MB_OKCANCEL:
      return IDOK;
      break;
    case MB_YESNO:
    case MB_YESNOCANCEL:
      return IDYES;
      break;
    case MB_ABORTRETRYIGNORE:
      return IDIGNORE;
      break;
    case MB_RETRYCANCEL:
      return IDCANCEL;
      break;
    default:
      Log (LOG_PLAIN) << "unattended_mode failed for " << (mb_type & MB_TYPEMASK) << endLog;
      return 0;
    }
}

int
mbox (HWND owner, const char *buf, const char *name, int type)
{
  // 'name' is not the mbox caption, just some text written to the log
  Log (LOG_PLAIN) << "mbox " << name << ": " << buf << endLog;
  if (unattended_mode)
    {
      Log (LOG_PLAIN) << "unattended_mode is set at mbox: returning default value" << endLog;
      return unattended_result(type);
    }

  char caption[32];
  LoadString (hinstance, IDS_MBOX_CAPTION, caption, sizeof (caption));

  return MessageBox (owner, buf, caption, type);
}

static int
mbox (HWND owner, const char *name, int type, int id, va_list args)
{
  char buf[1000], fmt[1000];

  if (LoadString (hinstance, id, fmt, sizeof (fmt)) <= 0)
    ExitProcess (0);

  vsnprintf (buf, 1000, fmt, args);
  return mbox(owner, buf, name, type);
}

void
note (HWND owner, int id, ...)
{
  va_list args;
  va_start (args, id);
  mbox (owner, "note", 0, id, args);
}

void
fatal (HWND owner, int id, ...)
{
  va_list args;
  va_start (args, id);
  mbox (owner, "fatal", 0, id, args);
  Logger ().exit (1);
}

int
yesno (HWND owner, int id, ...)
{
  va_list args;
  va_start (args, id);
  return mbox (owner, "yesno", MB_YESNO, id, args);
}

static HHOOK hMsgBoxHook;

LRESULT CALLBACK CBTProc(int nCode, WPARAM wParam, LPARAM lParam) {
  HWND hWnd;
  switch (nCode) {
    case HCBT_ACTIVATE:
      hWnd = (HWND)wParam;
      if (GetDlgItem(hWnd, IDCANCEL) != NULL)
        {
          // XXX: ideally we'd discover the text used for 'Continue' buttons in
          // MessageBoxes, rather than having our own translation.
          std::wstring cont = LoadStringW(IDS_CONTINUE);
          SetDlgItemTextW(hWnd, IDCANCEL, cont.c_str());
        }
      UnhookWindowsHookEx(hMsgBoxHook);
  }
  return CallNextHookEx(hMsgBoxHook, nCode, wParam, lParam);
}

// registry key to store "don't show this again" state, made unique by including a GUID
static const char *regkey = "Cygwin-Setup-d975d7b8-8c44-44a1-915a-7cf44b79cd88";

int
mbox(HWND owner, unsigned int format_id, int mb_type, ...)
{
  std::wstring fmt = LoadStringWEx(format_id, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
  if (fmt.empty())
    fmt = L"Internal error: format string resource not found";

  va_list args;
  va_start(args, mb_type);
  std::wstring buf = vformat(fmt, args);
  va_end(args);

  // write unlocalized to log as UTF8
  Log (LOG_PLAIN) << "mbox " << ": " << wstring_to_string(buf) << endLog;

  if (unattended_mode)
    return unattended_result(mb_type);

  fmt = LoadStringW(format_id);
  if (fmt.empty())
    fmt = L"Internal error: format string resource not found";

  va_start(args, mb_type);
  buf = vformat(fmt, args);
  va_end(args);

  bool retry_continue = (mb_type & MB_TYPEMASK) == MB_RETRYCONTINUE;
  if (retry_continue) {
    mb_type &= ~MB_TYPEMASK;
    mb_type |= MB_RETRYCANCEL;
    // Install a window hook, so we can intercept the message-box creation, and
    // customize it (replacing the text on the 'cancel' button with 'continue')
    // Only install for THIS thread!!!
    hMsgBoxHook = SetWindowsHookEx(WH_CBT, CBTProc, NULL, GetCurrentThreadId());
  }

  std::wstring caption = LoadStringW(IDS_MBOX_CAPTION);
  int retval;
  if (mb_type & MB_DSA_CHECKBOX)
    {
      mb_type &= ~MB_DSA_CHECKBOX;
      std::wstring regkey_msg = format(L"%s-%d", regkey, format_id);
      retval = SHMessageBoxCheckW(owner, buf.c_str(), caption.c_str(), mb_type,
                                  unattended_result(mb_type), regkey_msg.c_str());
    }
  else
    retval = MessageBoxW(owner, buf.c_str(), caption.c_str(), mb_type);

  // When the the retry_continue customization is active, adjust the return
  // value for less confusing results
  if (retry_continue && retval == IDCANCEL)
    retval = IDCONTINUE;

  return retval;
}
