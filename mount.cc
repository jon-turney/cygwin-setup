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

/* The purpose of this file is to hide all the details about accessing
   Cygwin's mount table.  If the format or location of the mount table
   changes, this is the file to change to match it. */

static char *cvsid = "\n%%% $Id$\n";

#include "win32.h"

#include <stdio.h>
#include "../cygwin/include/cygwin/version.h"
#include "../cygwin/include/sys/mount.h"

#include "mount.h"
#include "msg.h"
#include "resource.h"
#include "dialog.h"


static char *
find2 (HKEY rkey, int *istext)
{
  char buf[1000];
  char *retval = 0;
  HKEY key;
  DWORD retvallen = 0;
  DWORD flags = 0;
  DWORD type;

  sprintf (buf, "Software\\%s\\%s\\%s\\/",
	   CYGWIN_INFO_CYGNUS_REGISTRY_NAME,
	   CYGWIN_INFO_CYGWIN_REGISTRY_NAME,
	   CYGWIN_INFO_CYGWIN_MOUNT_REGISTRY_NAME);

  if (RegOpenKeyEx (rkey, buf, 0, KEY_READ, &key) != ERROR_SUCCESS)
    return 0;

  if (RegQueryValueEx (key, "native", 0, &type, 0, &retvallen)
      == ERROR_SUCCESS)
    {
      retval = new char[retvallen+1];
      if (RegQueryValueEx (key, "native", 0, &type, (BYTE *)retval, &retvallen)
	  != ERROR_SUCCESS)
	{
	  delete retval;
	  retval = 0;
	}
    }

  retvallen = sizeof (flags);
  RegQueryValueEx (key, "flags", 0, &type, (BYTE *)&flags, &retvallen);

  RegCloseKey (key);

  if (retval)
    *istext = (flags & MOUNT_BINARY) ? 0 : 1;
  return retval;
}

char *
find_root_mount (int *istext, int *issystem)
{
  char *rv;
  if (rv = find2 (HKEY_CURRENT_USER, istext))
    {
      *issystem = 0;
      return rv;
    }
  if (rv = find2 (HKEY_LOCAL_MACHINE, istext))
    {
      *issystem = 1;
      return rv;
    }
  return 0;
}

void
create_mount (char *posix, char *win32, int istext, int issystem)
{
  char buf[1000];
  char *retval = 0;
  HKEY key;
  DWORD retvallen = 0, disposition;
  DWORD flags;

  remove_mount (posix);

  sprintf (buf, "Software\\%s\\%s\\%s\\%s",
	   CYGWIN_INFO_CYGNUS_REGISTRY_NAME,
	   CYGWIN_INFO_CYGWIN_REGISTRY_NAME,
	   CYGWIN_INFO_CYGWIN_MOUNT_REGISTRY_NAME,
	   posix);

  HKEY kr = issystem ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  if (RegCreateKeyEx (kr, buf, 0, "Cygwin", 0, KEY_ALL_ACCESS,
		      0, &key, &disposition) != ERROR_SUCCESS)
    fatal ("mount");

  RegSetValueEx (key, "native", 0, REG_SZ, (BYTE *)win32, strlen (win32)+1);
  flags = 0;
  if (!istext)
    flags |= MOUNT_BINARY;
  if (issystem)
    flags |= MOUNT_SYSTEM;
  RegSetValueEx (key, "flags", 0, REG_DWORD, (BYTE *)&flags, sizeof (flags));
}

static void
remove1 (HKEY rkey, char *posix)
{
  char buf[1000];

  sprintf (buf, "Software\\%s\\%s\\%s\\%s",
	   CYGWIN_INFO_CYGNUS_REGISTRY_NAME,
	   CYGWIN_INFO_CYGWIN_REGISTRY_NAME,
	   CYGWIN_INFO_CYGWIN_MOUNT_REGISTRY_NAME,
	   posix);

  RegDeleteKey (rkey, buf);
}

void
remove_mount (char *posix)
{
  remove1 (HKEY_LOCAL_MACHINE, posix);
  remove1 (HKEY_CURRENT_USER, posix);
}

static void
set_cygdrive_flags (HKEY key, int istext, DWORD cygdrive_flags)
{
  int cur_istext = (cygdrive_flags & MOUNT_BINARY) ? 0 : 1;
  if (cur_istext != istext)
    {
      if (!istext)
	cygdrive_flags |= MOUNT_BINARY;
      else
	cygdrive_flags &= ~MOUNT_BINARY;
      RegSetValueEx (key, CYGWIN_INFO_CYGDRIVE_FLAGS, 0, REG_DWORD,
		     (BYTE *)&cygdrive_flags, sizeof (cygdrive_flags));
    }
}

static LONG
get_cygdrive_flags (HKEY key, DWORD *cygdrive_flags)
{
  DWORD retvallen = sizeof(*cygdrive_flags);
  LONG status = RegQueryValueEx (key, CYGWIN_INFO_CYGDRIVE_FLAGS, 0, 0,
				 (BYTE *)cygdrive_flags, &retvallen);
  return status;
}

static DWORD
default_cygdrive(HKEY key)
{
  RegSetValueEx (key, CYGWIN_INFO_CYGDRIVE_PREFIX, 0, REG_SZ,
		 (BYTE *)CYGWIN_INFO_CYGDRIVE_DEFAULT_PREFIX,
		 strlen (CYGWIN_INFO_CYGDRIVE_DEFAULT_PREFIX) + 1);
  DWORD cygdrive_flags = MOUNT_AUTO;
  RegSetValueEx (key, CYGWIN_INFO_CYGDRIVE_FLAGS, 0, REG_DWORD,
		 (BYTE *)&cygdrive_flags, sizeof (cygdrive_flags));
  return cygdrive_flags;
}

void
set_cygdrive_flags (int istext, int issystem)
{
  int found_system = 0;

  char buf[1000];
  sprintf (buf, "Software\\%s\\%s\\%s",
	   CYGWIN_INFO_CYGNUS_REGISTRY_NAME,
	   CYGWIN_INFO_CYGWIN_REGISTRY_NAME,
	   CYGWIN_INFO_CYGWIN_MOUNT_REGISTRY_NAME);

  if (issystem)
    {
      HKEY key;
      DWORD disposition;
      LONG status = RegCreateKeyEx (HKEY_LOCAL_MACHINE, buf, 0, 0, 0,
      				    KEY_ALL_ACCESS, 0, &key, &disposition);
      if (status == ERROR_SUCCESS)
	{
	  DWORD cygdrive_flags = 0;
	  status = get_cygdrive_flags (key, &cygdrive_flags);
	  if (status == ERROR_SUCCESS)
	    {
	      set_cygdrive_flags (key, istext, cygdrive_flags);
	      found_system = 1;
	    }
	  RegCloseKey(key);
	}
    }

  HKEY key;
  DWORD disposition;
  LONG status = RegCreateKeyEx (HKEY_CURRENT_USER, buf, 0, 0, 0, KEY_ALL_ACCESS,
				0, &key, &disposition);
  if (status != ERROR_SUCCESS)
    fatal ("set_cygdrive_flags");

  DWORD cygdrive_flags = 0;
  status = get_cygdrive_flags (key, &cygdrive_flags);
  if (status == ERROR_FILE_NOT_FOUND && !found_system)
    {
      cygdrive_flags = default_cygdrive(key);
      status = ERROR_SUCCESS;
    }

  if (status == ERROR_SUCCESS)
    set_cygdrive_flags (key, istext, cygdrive_flags);

  RegCloseKey(key);
}
