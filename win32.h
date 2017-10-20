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

#include <sys/types.h>
#include <string>

/* Needed for some buffers etc., to have a useful replacement for MAX_PATH. */
#define CYG_PATH_MAX	4096

/* Any include of <windows.h> should be through this file, which wraps it in
 * various other handling. */

/* Basic Windows features only. */
#define WIN32_LEAN_AND_MEAN

#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0a00

/* libstdc++-v3 _really_ dislikes min & max defined as macros. */
/* As of gcc 3.3.1, it defines NOMINMAX itself, so test first,
 * to avoid a redefinition error */
#ifndef NOMINMAX
#define NOMINMAX
#endif

/* 100ns difference between Windows and UNIX timebase. */
#define FACTOR (0x19db1ded53e8000LL)
/* # of 100ns intervals per second. */
#define NSPERSEC 10000000LL

/* Defining WINBASEAPI is required due the overloading definitions of some
   kernel32 functions in filemanip.cc. */
#define WINBASEAPI

#include <windows.h>

/* FIXME: The use of _access(fname, 0) as an existence check should be
 * replaced with a more readable idiom, and this fragment removed. */
#ifndef _access
#define _access access
#endif

/* When we have to check for a path delimiter, check for both, slash and
   backslash. */
#define isdirsep(ch) \
    ({ \
	char __c = (ch); \
	((__c) == '/' || (__c) == '\\'); \
    })

/* Maximum size of a SID. */
#define MAX_SID_LEN	40

/* Computes the size of an ACL in relation to the number of ACEs it
   should contain. */
#define TOKEN_ACL_SIZE(cnt) (sizeof (ACL) + \
			     (cnt) * (sizeof (ACCESS_ALLOWED_ACE) + MAX_SID_LEN))

struct acl_t {
  /* Make sure &acl is 4-byte aligned. */
  ACL acl __attribute__ ((aligned (4)));
  char aclbuf[TOKEN_ACL_SIZE (7)];
};

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
		  cr_ownerSID (), cr_groupSID (), groupSID (NULL),
		  _wellKnownSIDsinitialized (false), token () {}
  ~NTSecurity() {}

  /* prevent synthetics */
  NTSecurity& operator= (NTSecurity const &);
  NTSecurity (NTSecurity const &);

  /* Set POSIX-like permissions on files.  The fname is only used for printing
     log output.  The function requires an open HANDLE with sufficient
     permissions (READ_DAC | WRITE_DAC). */
  PSECURITY_DESCRIPTOR GetPosixPerms (const char *fname, PSID owner_sid,
				      PSID group_sid, mode_t mode,
				      SECURITY_DESCRIPTOR &out_sd, acl_t &acl);
  void resetPrimaryGroup();
  void setAdminGroup ();
  void initialiseWellKnownSIDs ();
  void setDefaultSecurity();
  bool isRunAsAdmin ();
private:
  void NoteFailedAPI (const std::string &);
  bool wellKnownSIDsinitialized () const { return _wellKnownSIDsinitialized; }
  void wellKnownSIDsinitialized (bool b) { _wellKnownSIDsinitialized = b; }
  void setDefaultDACL ();
  void setBackupPrivileges ();

  SIDWrapper nullSID, everyOneSID, administratorsSID, usersSID,
	     cr_ownerSID, cr_groupSID;
  struct {
    TOKEN_USER user;
    char buf[MAX_SID_LEN];
  } ownerSID;
  PSID groupSID;
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
     DWORD major () const { return v.dwMajorVersion; }
     DWORD minor () const { return v.dwMinorVersion; }
     DWORD buildNumber () const { return v.dwBuildNumber; }
  private:
     OSVERSIONINFO v;
};

VersionInfo& GetVer ();

#define OSMajorVersion() (GetVer ().major ())
#define OSMinorVersion() (GetVer ().minor ())
#define OSBuildNumber() (GetVer ().buildNumber ())

static inline void
GetDlgItemRect (HWND h, int item, LPRECT r)
{
  GetWindowRect (GetDlgItem (h, item), r);
  MapWindowPoints (HWND_DESKTOP, h, (LPPOINT) r, 2);
}

static inline void
SetDlgItemRect (HWND h, int item, LPRECT r)
{
  MoveWindow (GetDlgItem (h, item), r->left, r->top,
	      r->right - r->left, r->bottom - r->top, TRUE);
}

#endif /* SETUP_WIN32_H */
