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
 * and Robert Collins <rbtcollins@hotmail.com>
 *
 */

#ifndef SETUP_WIN32_H
#define SETUP_WIN32_H

#include <string>

/* Any include of <windows.h> should be through this file, which wraps it in
 * various other handling. */

/* Basic Windows features only. */
#define WIN32_LEAN_AND_MEAN

/* libstdc++-v3 _really_ dislikes min & max defined as macros. */
/* As of gcc 3.3.1, it defines NOMINMAX itself, so test first,
 * to avoid a redefinition error */
#ifndef NOMINMAX
#define NOMINMAX
#endif

/* In w32api 3.1, __declspec(dllimport) decoration is added to
 * certain symbols. This breaks our autoload mechanism - the symptom is
 * multiple declaration errors at link time. This define turns that off again.
 * It will default to off again in later w32api versions, but we need to work
 * with 3.1 for now. */
#define WINBASEAPI

/* Require at least Internet Explorer 3, in order to have access to
 * sufficient Windows Common Controls features from <commctrl.h> . */
#define _WIN32_IE 0x0300

#include <windows.h>

/* FIXME: The use of _access(fname, 0) as an existence check should be
 * replaced with a more readable idiom, and this fragment removed. */
#ifndef _access
#define _access access
#endif

/* Maximum size of a SID on NT/W2K. */
#define MAX_SID_LEN	40

/* Computes the size of an ACL in relation to the number of ACEs it
   should contain. */
#define TOKEN_ACL_SIZE(cnt) (sizeof (ACL) + \
			     (cnt) * (sizeof (ACCESS_ALLOWED_ACE) + MAX_SID_LEN))

class SIDWrapper {
  public:
    SIDWrapper () : value (NULL) {}
    /* Prevent synthetics. If assignment is needed, this should be
       refcounting.  */
    SIDWrapper (SIDWrapper const &);
    SIDWrapper& operator= (SIDWrapper const &);
    ~SIDWrapper () { if (value) FreeSid (value); }

    /* We could look at doing weird typcast overloads here,
       but manual access is easier for now.  */
    PSID &theSID () { return value; }
    PSID const &theSID () const { return value; }
  private:
    PSID value;
};

class HANDLEWrapper {
  public:
    HANDLEWrapper () : value (NULL) {}
    /* Prevent synthetics. If assignment is needed, we should duphandles,
       or refcount.  */
    HANDLEWrapper (HANDLEWrapper const &);
    HANDLEWrapper& operator= (HANDLEWrapper const &);
    ~HANDLEWrapper () { if (value) CloseHandle (value); }
    HANDLE &theHANDLE () { return value; }
    HANDLE const &theHANDLE () const { return value; }
  private:
    HANDLE value;
};

class NTSecurity
{
public:
  NTSecurity () : nullSID (), everyOneSID (), administratorsSID (), usersSID (),
		  _wellKnownSIDsinitialized (false), token () {}
  ~NTSecurity() {}

  /* prevent synthetics */
  NTSecurity& operator= (NTSecurity const &);
  NTSecurity (NTSecurity const &);

  /* Set POSIX-like permissions on files.  The fname is only used for printing
     log output.  The function requires an open HANDLE with sufficient
     permissions (READ_DAC | WRITE_DAC). */
  void SetPosixPerms (const char *fname, HANDLE fh, mode_t mode);
  void resetPrimaryGroup();
  void setAdminGroup ();
  void setDefaultSecurity();
private:
  void NoteFailedAPI (const std::string &);
  bool wellKnownSIDsinitialized () const { return _wellKnownSIDsinitialized; }
  void wellKnownSIDsinitialized (bool b) { _wellKnownSIDsinitialized = b; }
  void initialiseWellKnownSIDs ();
  void setDefaultDACL ();
  void setBackupPrivileges ();

  SIDWrapper nullSID, everyOneSID, administratorsSID, usersSID;
  struct {
    TOKEN_PRIMARY_GROUP pgrp;
    char buf[MAX_SID_LEN];
  } primaryGroupSID;

  bool _wellKnownSIDsinitialized;
  HANDLEWrapper token;
  DWORD size;
};

extern NTSecurity nt_sec;

#undef major
#undef minor
class VersionInfo
{
  public:
     VersionInfo ();
     bool isNT () { return (v.dwPlatformId == VER_PLATFORM_WIN32_NT); }
     DWORD major () const { return v.dwMajorVersion; }
     DWORD minor () const { return v.dwMinorVersion; }
  private:
     OSVERSIONINFO v;
};

VersionInfo& GetVer ();

#define IsWindowsNT() (GetVer ().isNT ())
#define OSMajorVersion() (GetVer ().major ())
#define OSMinorVersion() (GetVer ().minor ())

#endif /* SETUP_WIN32_H */
