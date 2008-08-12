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

#if 0
static const char *cvsid = "\n%%% $Id$\n";
#endif

#include "win32.h"

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

// These headers aren't available outside the winsup tree
// #include "../cygwin/include/cygwin/version.h"
// KEEP SYNCHRONISED WITH /src/winsup/cygwin/include/cygwin/version.h

#define CYGWIN_INFO_CYGNUS_REGISTRY_NAME "Cygnus Solutions"
#define CYGWIN_INFO_CYGWIN_REGISTRY_NAME "Cygwin"
#define CYGWIN_INFO_CYGWIN_SETUP_REGISTRY_NAME "setup"
#define CYGWIN_INFO_CYGWIN_MOUNT_REGISTRY_NAME "mounts v2"
#define CYGWIN_INFO_CYGDRIVE_FLAGS "cygdrive flags"
#define CYGWIN_INFO_CYGDRIVE_PREFIX "cygdrive prefix"
#define CYGWIN_INFO_CYGDRIVE_DEFAULT_PREFIX "/cygdrive"


// #include "../cygwin/include/sys/mount.h"

// KEEP SYNCHRONISED WITH /src/winsup/cygwin/include/sys/mount.h
#ifdef __cplusplus
extern "C" {
#endif

enum
{
  MOUNT_SYMLINK =       0x001,  /* "mount point" is a symlink */
  MOUNT_BINARY =        0x002,  /* "binary" format read/writes */
  MOUNT_SYSTEM =        0x008,  /* mount point came from system table */
  MOUNT_EXEC   =        0x010,  /* Any file in the mounted directory gets 'x' bit */
  MOUNT_AUTO   =        0x020,  /* mount point refers to auto device mount */
  MOUNT_CYGWIN_EXEC =   0x040,  /* file or directory is or contains a cygwin executable */
  MOUNT_MIXED   =       0x080,  /* reads are text, writes are binary */
};

//  int mount (const char *, const char *, unsigned __flags);
//    int umount (const char *);
//    int cygwin_umount (const char *__path, unsigned __flags);

#ifdef __cplusplus
};
#endif



#include "mount.h"
#include "msg.h"
#include "resource.h"
#include "dialog.h"
#include "state.h"

#ifdef MAINTAINER_FEATURES
#include "getopt++/GetOption.h"
#include "getopt++/StringOption.h"
static StringOption CygwinRegistryNameOption (CYGWIN_INFO_CYGWIN_REGISTRY_NAME, '#', "override-registry-name", "Override registry name to allow parallel installs for testing purposes", false);
#undef CYGWIN_INFO_CYGWIN_REGISTRY_NAME
#define CYGWIN_INFO_CYGWIN_REGISTRY_NAME (((std::string)CygwinRegistryNameOption).c_str())
#endif

/* Used when treating / and \ as equivalent. */
#define SLASH_P(ch) \
    ({ \
        char __c = (ch); \
        ((__c) == '/' || (__c) == '\\'); \
    })

static struct mnt
{
  std::string native;
  std::string posix;
  int istext;
}
mount_table[255];

struct mnt *root_here = NULL;

static std::string
find2 (HKEY rkey, int *istext, const std::string& what)
{
  HKEY key;

  if (RegOpenKeyEx (rkey, what.c_str (), 0, KEY_READ, &key) !=
      ERROR_SUCCESS)
    return 0;

  DWORD retvallen = 0;
  DWORD type;

  std::string Sretval;
  if (RegQueryValueEx (key, "native", 0, &type, 0, &retvallen)
      == ERROR_SUCCESS)
    {
      char retval[retvallen];
      if (RegQueryValueEx
	  (key, "native", 0, &type, (BYTE *) retval,
	   &retvallen) == ERROR_SUCCESS)
	Sretval = std::string (retval);
    }

  DWORD flags = 0;
  retvallen = sizeof (flags);
  RegQueryValueEx (key, "flags", 0, &type, (BYTE *) & flags, &retvallen);

  RegCloseKey (key);

  if (Sretval.size ())
    *istext = (flags & MOUNT_BINARY) ? 0 : 1;

  return Sretval;
}

static void
remove1 (HKEY rkey, const std::string posix)
{
  char buf[1000];

  snprintf (buf, sizeof(buf), "Software\\%s\\%s\\%s\\%s",
	   CYGWIN_INFO_CYGNUS_REGISTRY_NAME,
	   CYGWIN_INFO_CYGWIN_REGISTRY_NAME,
	   CYGWIN_INFO_CYGWIN_MOUNT_REGISTRY_NAME, posix.c_str ());

  RegDeleteKey (rkey, buf);
}

static void
remove_mount (const std::string posix)
{
  remove1 (HKEY_LOCAL_MACHINE, posix);
  remove1 (HKEY_CURRENT_USER, posix);
}

void
create_mount (const std::string posix, const std::string win32, int istext,
	      int issystem)
{
  char buf[1000];
  HKEY key;
  DWORD disposition;
  DWORD flags;
  DWORD rv;

  remove_mount (posix);

  snprintf (buf, sizeof(buf), "Software\\%s\\%s\\%s\\%s",
	   CYGWIN_INFO_CYGNUS_REGISTRY_NAME,
	   CYGWIN_INFO_CYGWIN_REGISTRY_NAME,
	   CYGWIN_INFO_CYGWIN_MOUNT_REGISTRY_NAME, posix.c_str ());

  HKEY kr = issystem ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  rv = RegCreateKeyEx (kr, buf, 0, (char *)"Cygwin", 0, KEY_ALL_ACCESS,
                       0, &key, &disposition);
  if (rv != ERROR_SUCCESS)
    fatal ("mount", rv);

  RegSetValueEx (key, "native", 0, REG_SZ, (BYTE *) win32.c_str (),
		 win32.size () + 1);
  flags = 0;
  if (!istext)
    flags |= MOUNT_BINARY;
  if (issystem)
    flags |= MOUNT_SYSTEM;
  RegSetValueEx (key, "flags", 0, REG_DWORD, (BYTE *) & flags,
		 sizeof (flags));

  RegCloseKey (key);
  read_mounts (std::string ());
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
		     (BYTE *) & cygdrive_flags, sizeof (cygdrive_flags));
    }
}

static LONG
get_cygdrive_flags (HKEY key, DWORD * cygdrive_flags)
{
  DWORD retvallen = sizeof (*cygdrive_flags);
  LONG status = RegQueryValueEx (key, CYGWIN_INFO_CYGDRIVE_FLAGS, 0, 0,
				 (BYTE *) cygdrive_flags, &retvallen);
  return status;
}

static DWORD
default_cygdrive (HKEY key)
{
  RegSetValueEx (key, CYGWIN_INFO_CYGDRIVE_PREFIX, 0, REG_SZ,
		 (BYTE *) CYGWIN_INFO_CYGDRIVE_DEFAULT_PREFIX,
		 strlen (CYGWIN_INFO_CYGDRIVE_DEFAULT_PREFIX) + 1);
  DWORD cygdrive_flags = MOUNT_AUTO;
  RegSetValueEx (key, CYGWIN_INFO_CYGDRIVE_FLAGS, 0, REG_DWORD,
		 (BYTE *) & cygdrive_flags, sizeof (cygdrive_flags));
  return cygdrive_flags;
}

void
set_cygdrive_flags (int istext, int issystem)
{
  int found_system = 0;

  char buf[1000];
  snprintf (buf, sizeof(buf), "Software\\%s\\%s\\%s",
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
	  if (status == ERROR_FILE_NOT_FOUND)
	    {
	      status = ERROR_SUCCESS;
	      cygdrive_flags = default_cygdrive (key);
	    }
	  if (status == ERROR_SUCCESS)
	    {
	      set_cygdrive_flags (key, istext, cygdrive_flags);
	      found_system = 1;
	    }
	  RegCloseKey (key);
	}
    }

  HKEY key;
  DWORD disposition;
  LONG status =
    RegCreateKeyEx (HKEY_CURRENT_USER, buf, 0, 0, 0, KEY_ALL_ACCESS,
		    0, &key, &disposition);
  if (status != ERROR_SUCCESS)
    fatal ("set_cygdrive_flags");

  DWORD cygdrive_flags = 0;
  status = get_cygdrive_flags (key, &cygdrive_flags);
  if (status == ERROR_FILE_NOT_FOUND && !found_system)
    {
      cygdrive_flags = default_cygdrive (key);
      status = ERROR_SUCCESS;
    }

  if (status == ERROR_SUCCESS)
    set_cygdrive_flags (key, istext, cygdrive_flags);

  RegCloseKey (key);
}

static int
in_table (struct mnt *m)
{
  for (struct mnt * m1 = mount_table; m1 < m; m1++)
    if (casecompare(m1->posix, m->posix) == 0)
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
  if (!IsWindowsNT ())
    return 1;

  // Get the process token for the current process
  HANDLE token;
  if (!OpenProcessToken (GetCurrentProcess (), TOKEN_QUERY, &token))
    return 0;

  // Get the group token information
  DWORD size;
  GetTokenInformation (token, TokenGroups, NULL, 0, &size);
  if (GetLastError () != ERROR_INSUFFICIENT_BUFFER)
    {
      CloseHandle (token);
      return 0;
    }

  char *buf = (char *) alloca (size);
  PTOKEN_GROUPS groups = (PTOKEN_GROUPS) buf;
  DWORD status = GetTokenInformation (token, TokenGroups, buf, size, &size);
  CloseHandle (token);
  if (!status)
    return 0;

  // Create the Administrators group SID
  PSID admin_sid;
  SID_IDENTIFIER_AUTHORITY authority = { SECURITY_NT_AUTHORITY };
  if (!AllocateAndInitializeSid (&authority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                 DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0,
                                 &admin_sid))
    return 0;

  // Check to see if the user is a member of the Administrators group
  int ret = 0;
  for (UINT i = 0; i < groups->GroupCount; i++)
    {
      if (EqualSid (groups->Groups[i].Sid, admin_sid))
	{
	  ret = 1;
	  break;
	}
    }

  // Destroy the Administrators group SID
  FreeSid (admin_sid);

  // Return whether or not the user is a member of the Administrators group
  return ret;
}

void
create_install_root ()
{
  char buf[1000];
  HKEY key;
  DWORD disposition;
  DWORD rv;

  snprintf (buf, sizeof(buf), "Software\\%s\\%s",
	    CYGWIN_INFO_CYGWIN_REGISTRY_NAME,
	    CYGWIN_INFO_CYGWIN_SETUP_REGISTRY_NAME);
  HKEY kr = (root_scope == IDC_ROOT_USER) ? HKEY_CURRENT_USER
					  : HKEY_LOCAL_MACHINE;
  rv = RegCreateKeyEx (kr, buf, 0, (char *)"Cygwin", 0, KEY_ALL_ACCESS,
		       0, &key, &disposition);
  if (rv != ERROR_SUCCESS)
    fatal ("mount", rv);
  rv = RegSetValueEx (key, "rootdir", 0, REG_SZ,
		      (BYTE *) get_root_dir ().c_str (),
		      get_root_dir ().size () + 1);
  if (rv != ERROR_SUCCESS)
    fatal ("mount", rv);
  RegCloseKey (key);

  read_mounts (std::string ());
}

static void
read_mounts_9x ()
{
  DWORD posix_path_size;
  int res;
  struct mnt *m = mount_table;
  DWORD disposition;
  char buf[10000];

  root_here = NULL;
  for (mnt * m1 = mount_table; m1->posix.size (); m1++)
    {
      m1->posix.clear();
      m1->native.clear();
    }

  /* Loop through subkeys */
  /* FIXME: we would like to not check MAX_MOUNTS but the heap in the
     shared area is currently statically allocated so we can't have an
     arbitrarily large number of mounts. */
  for (int issystem = 0; issystem <= 1; issystem++)
    {
      snprintf (buf, sizeof(buf), "Software\\%s\\%s\\%s",
	       CYGWIN_INFO_CYGNUS_REGISTRY_NAME,
	       CYGWIN_INFO_CYGWIN_REGISTRY_NAME,
	       CYGWIN_INFO_CYGWIN_MOUNT_REGISTRY_NAME);

      HKEY key = issystem ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
      if ((IsWindowsNT () ? RegOpenKeyEx (key, buf, 0, KEY_ALL_ACCESS, &key)
			  : RegCreateKeyEx (key, buf, 0, (char *)"Cygwin", 0,
					    KEY_ALL_ACCESS, 0, &key,
					    &disposition))
	  != ERROR_SUCCESS)
	break;
      for (int i = 0;; i++, m++)
	{
	  char aBuffer[MAX_PATH + 1];
	  posix_path_size = MAX_PATH;
	  /* FIXME: if maximum posix_path_size is 256, we're going to
	     run into problems if we ever try to store a mount point that's
	     over 256 but is under MAX_PATH. */
	  res = RegEnumKeyEx (key, i, aBuffer, &posix_path_size, NULL,
			      NULL, NULL, NULL);

	  if (res == ERROR_NO_MORE_ITEMS)
	    {
	      m->posix.clear();
	      break;
	    }
	  m->posix = std::string (aBuffer);

	  if (!m->posix.size () || in_table (m))
	    goto no_go;
	  else if (res != ERROR_SUCCESS)
	    break;
	  else
	    {
	      m->native = find2 (key, &m->istext, m->posix);
	      if (!m->native.size ())
		goto no_go;

	      if (m->posix == "/")
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
	  m->posix.clear();
	  --m;
	}
      RegCloseKey (key);
    }

  if (!IsWindowsNT () && !root_here)
    {
      root_here = m;
      m->posix = "/";
      char windir[MAX_PATH];
      root_text = IDC_ROOT_BINARY;
      root_scope = (is_admin ())? IDC_ROOT_SYSTEM : IDC_ROOT_USER;
      GetWindowsDirectory (windir, sizeof (windir));
      windir[2] = 0;
      set_root_dir (std::string (windir) + "\\cygwin");
      m++;
    }
}

static void
add_usr_mnts (struct mnt *m)
{
  /* Set default /usr/bin and /usr/lib */
  m->posix = "/usr/bin";
  m->native = root_here->native + "\\bin";
  ++m;
  m->posix = "/usr/lib";
  m->native = root_here->native + "\\lib";
}

static void
read_mounts_nt (const std::string val)
{
  DWORD posix_path_size;
  struct mnt *m = mount_table;
  DWORD disposition;
  char buf[10000];

  root_here = NULL;
  for (mnt * m1 = mount_table; m1->posix.size (); m1++)
    {
      m1->posix.clear();
      m1->native.clear();
    }

  root_text = IDC_ROOT_BINARY;
  root_scope = (is_admin ())? IDC_ROOT_SYSTEM : IDC_ROOT_USER;

  if (val.size ())
    {
      m->native = val;
      m->posix = "/";
      root_here = m;
      add_usr_mnts (++m);
    }
  else
    {
      /* Always check HKEY_LOCAL_MACHINE first. */
      for (int isuser = 0; isuser <= 1; isuser++)
	{
	  snprintf (buf, sizeof(buf), "Software\\%s\\%s",
		   CYGWIN_INFO_CYGWIN_REGISTRY_NAME,
		   CYGWIN_INFO_CYGWIN_SETUP_REGISTRY_NAME);
	  HKEY key = isuser ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
	  if (RegCreateKeyEx (key, buf, 0, (char *)"Cygwin", 0, KEY_ALL_ACCESS,
			      0, &key, &disposition) != ERROR_SUCCESS)
	    break;
	  DWORD type;
	  char aBuffer[MAX_PATH + 1];
	  posix_path_size = MAX_PATH;
	  if (RegQueryValueEx
	      (key, "rootdir", 0, &type, (BYTE *) aBuffer,
	       &posix_path_size) == ERROR_SUCCESS)
	    {
	      m->native = std::string (aBuffer);
	      m->posix = "/";
	      root_scope = isuser ? IDC_ROOT_USER : IDC_ROOT_SYSTEM;
	      root_here = m;

	      /* TODO: Read /etc/fstab and skip add_usr_mnts if available. */
	      add_usr_mnts (++m);
	      break;
	    }
	  RegCloseKey (key);
	}
    }

  /* Check for an old installation to allow overriding. */
  if (!root_here)
    read_mounts_9x ();

  if (!root_here)
    {
      char windir[MAX_PATH];
      GetWindowsDirectory (windir, sizeof (windir));
      windir[2] = 0;
      m->native = std::string (windir) + "\\cygwin";
      m->posix = "/";
      root_here = m;
      add_usr_mnts (++m);
    }
}

void
read_mounts (const std::string val)
{
  if (IsWindowsNT ())
    read_mounts_nt (val);
  else
    read_mounts_9x ();
}

void
set_root_dir (const std::string val)
{
  if (IsWindowsNT ())
    read_mounts (val);
  else
    root_here->native = val;
}

const std::string
get_root_dir ()
{
  return root_here ? root_here->native : std::string();
}

/* Return non-zero if PATH1 is a prefix of PATH2.
   Both are assumed to be of the same path style and / vs \ usage.
   Neither may be "".

   Examples:
   /foo/ is a prefix of /foo  <-- may seem odd, but desired
   /foo is a prefix of /foo/
   / is a prefix of /foo/bar
   / is not a prefix of foo/bar
   foo/ is a prefix foo/bar
   /foo is not a prefix of /foobar
*/

static int
path_prefix_p (const std::string path1, const std::string path2)
{
  size_t len1 = path1.size ();
  /* Handle case where PATH1 has trailing '/' and when it doesn't.  */
  if (len1 > 0 && SLASH_P (path1.c_str ()[len1 - 1]))
    --len1;

  if (len1 == 0)
    return SLASH_P (path2.c_str ()[0])
      && !SLASH_P (path2.c_str ()[1]);

  if (casecompare(path1, path2, len1) != 0)
    return 0;

  return SLASH_P (path2.c_str ()[len1]) || path2.size () == len1
    || path1.c_str ()[len1 - 1] == ':';
}

std::string
cygpath (const std::string& thePath)
{
  size_t max_len = 0;
  struct mnt *m, *match = NULL;
  for (m = mount_table; m->posix.size (); m++)
    {
      size_t n = m->posix.size ();
      if (n <= max_len || !path_prefix_p (m->posix, thePath))
	continue;
      max_len = n;
      match = m;
    }

  if (!match)
    return std::string();

  std::string native;
  if (max_len == thePath.size ())
    {
      native = match->native;
    }
  else if (match->posix.size () > 1)
    native = match->native + thePath.substr(max_len, std::string::npos);
  else
    native = match->native + "/" + thePath.substr(max_len, std::string::npos);
  return native;
}
