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
#include <stdlib.h>
#include "../cygwin/include/cygwin/version.h"
#include "../cygwin/include/sys/mount.h"

#include "mount.h"
#include "msg.h"
#include "resource.h"
#include "dialog.h"
#include "state.h"
#include "concat.h"

static struct mnt
  {
    const char *native;
    char *posix;
    int istext;
  } mount_table[255];

struct mnt *root_here = NULL;

static char *
find2 (HKEY rkey, int *istext, char *what)
{
  char buf[1000];
  char *retval = 0;
  DWORD retvallen = 0;
  DWORD flags = 0;
  DWORD type;
  HKEY key;

  if (RegOpenKeyEx (rkey, what, 0, KEY_READ, &key) != ERROR_SUCCESS)
    return 0;

  if (RegQueryValueEx (key, "native", 0, &type, 0, &retvallen)
      == ERROR_SUCCESS)
    {
      retval = (char *) malloc (MAX_PATH + 1);
      if (RegQueryValueEx (key, "native", 0, &type, (BYTE *) retval, &retvallen)
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

void
create_mount (const char *posix, const char *win32, int istext, int issystem)
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

  RegSetValueEx (key, "native", 0, REG_SZ, (BYTE *) win32, strlen (win32) + 1);
  flags = 0;
  if (!istext)
    flags |= MOUNT_BINARY;
  if (issystem)
    flags |= MOUNT_SYSTEM;
  RegSetValueEx (key, "flags", 0, REG_DWORD, (BYTE *)&flags, sizeof (flags));
  
  RegCloseKey(key);
  read_mounts ();
}

static void
remove1 (HKEY rkey, const char *posix)
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
remove_mount (const char *posix)
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

static int
in_table (struct mnt *m)
{
  for (struct mnt *m1 = mount_table; m1 < m; m1++)
    if (strcasecmp (m1->posix, m->posix) == 0)
      return 1;
  return 0;
}

/*
 * is_admin () determines whether or not the current user is a member of the
 * Administrators group.  On Windows 9X, the current user is considered an
 * Administrator by definition.
 */

static int
is_admin ()
{
  // Windows 9X users are considered Administrators by definition
  OSVERSIONINFO verinfo;
  verinfo.dwOSVersionInfoSize = sizeof (verinfo);
  GetVersionEx (&verinfo);
  if (verinfo.dwPlatformId != VER_PLATFORM_WIN32_NT)
    return 1;

  // Get the process token for the current process
  HANDLE token;
  BOOL status = OpenProcessToken (GetCurrentProcess(), TOKEN_QUERY, &token);
  if (!status)
    return 0;

  // Get the group token information
  UCHAR token_info[1024];
  PTOKEN_GROUPS groups = (PTOKEN_GROUPS) token_info;
  DWORD token_info_len = sizeof (token_info);
  status = GetTokenInformation (token, TokenGroups, token_info, token_info_len, &token_info_len);
  CloseHandle(token);
  if (!status)
    return 0;

  // Create the Administrators group SID
  PSID admin_sid;
  SID_IDENTIFIER_AUTHORITY authority = SECURITY_NT_AUTHORITY;
  status = AllocateAndInitializeSid (&authority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &admin_sid);
  if (!status)
    return 0;

  // Check to see if the user is a member of the Administrators group
  status = 0;
  for (UINT i=0; i<groups->GroupCount; i++) {
    if (EqualSid(groups->Groups[i].Sid, admin_sid)) {
      status = 1;
      break;
    }
  }

  // Destroy the Administrators group SID
  FreeSid (admin_sid);

  // Return whether or not the user is a member of the Administrators group
  return status;
}

void
read_mounts ()
{
  DWORD posix_path_size;
  int res;
  struct mnt *m = mount_table;
  DWORD disposition;
  char buf[10000];

  root_here = NULL;
  for (mnt *m1 = mount_table; m1->posix; m1++)
    {
      free (m1->posix);
      if (m1->native)
	free ((char *) m1->native);
      m1->posix = NULL;
    }

  /* Loop through subkeys */
  /* FIXME: we would like to not check MAX_MOUNTS but the heap in the
     shared area is currently statically allocated so we can't have an
     arbitrarily large number of mounts. */
  for (int issystem = 0; issystem <= 1; issystem++)
    {
      sprintf (buf, "Software\\%s\\%s\\%s",
	       CYGWIN_INFO_CYGNUS_REGISTRY_NAME,
	       CYGWIN_INFO_CYGWIN_REGISTRY_NAME,
	       CYGWIN_INFO_CYGWIN_MOUNT_REGISTRY_NAME);

      HKEY key = issystem ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
      if (RegCreateKeyEx (key, buf, 0, "Cygwin", 0, KEY_ALL_ACCESS,
			  0, &key, &disposition) != ERROR_SUCCESS)
	break;
      for (int i = 0; ;i++, m++)
	{
	  m->posix = (char *) malloc (MAX_PATH + 1);
	  posix_path_size = MAX_PATH;
	  /* FIXME: if maximum posix_path_size is 256, we're going to
	     run into problems if we ever try to store a mount point that's
	     over 256 but is under MAX_PATH. */
	  res = RegEnumKeyEx (key, i, m->posix, &posix_path_size, NULL,
			      NULL, NULL, NULL);

	  if (res == ERROR_NO_MORE_ITEMS)
	    {
	      free (m->posix);
	      m->posix = NULL;
	      break;
	    }

	  if (!*m->posix || in_table (m))
	    goto no_go;
	  else if (res != ERROR_SUCCESS)
	    break;
	  else
	    {
	      m->native = find2 (key, &m->istext, m->posix);
	      if (!m->native)
		goto no_go;
		  
	      if (strcmp (m->posix, "/") == 0)
		{
		  root_here = m;
		  if (m->istext)
		    root_text = IDC_ROOT_TEXT;
		  else
		    root_text = IDC_ROOT_BINARY;
		  if (issystem)
		    root_scope = IDC_ROOT_SYSTEM;
		  else
		    root_scope = IDC_ROOT_USER;
		}
	    }
	  continue;
	no_go:
	  free (m->posix);
	  m->posix = NULL;
	  m--;
	}
      RegCloseKey (key);
    }

  if (!root_here)
    {
      root_here = m;
      m->posix = strdup ("/");
      char windir[_MAX_PATH];
      root_text = IDC_ROOT_BINARY;
      root_scope = (is_admin()) ? IDC_ROOT_SYSTEM : IDC_ROOT_USER;
      GetWindowsDirectory (windir, sizeof (windir));
      windir[2] = 0;
      set_root_dir (concat (windir, "\\cygwin", 0));
      m++;
    }
}

void
set_root_dir (const char *val)
{
  root_here->native = val;
}

const char *
get_root_dir ()
{
  return root_here ? root_here->native : NULL;
}

/* Return non-zero if PATH1 is a prefix of PATH2.
   Both are assumed to be of the same path style and / vs \ usage.
   Neither may be "".
   LEN1 = strlen (PATH1).  It's passed because often it's already known.

   Examples:
   /foo/ is a prefix of /foo  <-- may seem odd, but desired
   /foo is a prefix of /foo/
   / is a prefix of /foo/bar
   / is not a prefix of foo/bar
   foo/ is a prefix foo/bar
   /foo is not a prefix of /foobar
*/

static int
path_prefix_p (const char *path1, const char *path2, int len1)
{
  /* Handle case where PATH1 has trailing '/' and when it doesn't.  */
  if (len1 > 0 && SLASH_P (path1[len1 - 1]))
    len1--;

  if (len1 == 0)
    return SLASH_P (path2[0]) && !SLASH_P (path2[1]);

  if (strncasecmp (path1, path2, len1) != 0)
    return 0;

  return SLASH_P (path2[len1]) || path2[len1] == 0 || path1[len1 - 1] == ':';
}

char *
cygpath (const char *s, ...)
{
  va_list v;
  int i, max_len = -1;
  struct mnt *m, *match;

  va_start (v, s);
  char *path = vconcat (s, v);
  if (strncmp (path, "./", 2) == 0)
    memmove (path, path + 2, strlen (path + 2) + 1);
  if (strncmp (path, "/./", 3) == 0)
    memmove (path + 1, path + 3, strlen (path + 3) + 1);

  for (m = mount_table; m->posix ; m++)
    {
      int n = strlen (m->posix);
      if (n < max_len || !path_prefix_p (m->posix, path, n))
	continue;
      max_len = n;
      match = m;
    }

  char *native;
  if (max_len == strlen (path))
    native = strdup (match->native);
  else
    native = concat (match->native, "/", path + max_len, NULL);
  free (path);
  return native;
}
