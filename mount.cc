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
find_root_mount (int *istext)
{
  char *rv;
  if (rv = find2 (HKEY_CURRENT_USER, istext))
    return rv;
  return find2 (HKEY_LOCAL_MACHINE, istext);
}

void
create_mount (char *posix, char *win32, int istext)
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

  if (RegCreateKeyEx (HKEY_CURRENT_USER, buf, 0, "Cygwin", 0, KEY_ALL_ACCESS,
		      0, &key, &disposition) != ERROR_SUCCESS)
    fatal ("mount");

  RegSetValueEx (key, "native", 0, REG_SZ, (BYTE *)win32, strlen (win32)+1);
  if (istext)
    flags = 0;
  else
    flags = MOUNT_BINARY;
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
