/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009,
 * 2010, 2013 Red Hat, Inc.
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

#include "ini.h"
#include "win32.h"
#include "filemanip.h"
#include "LogSingleton.h"

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

// These headers aren't available outside the winsup tree
// #include "../cygwin/include/cygwin/version.h"
// KEEP SYNCHRONISED WITH /src/winsup/cygwin/include/cygwin/version.h

#define CYGWIN_INFO_CYGWIN_REGISTRY_NAME "Cygwin"
#define CYGWIN_INFO_CYGWIN_SETUP_REGISTRY_NAME "setup"

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
  do
    {
      rv = RegCreateKeyEx (kr, buf, 0, (char *)"Cygwin", 0,
			   KEY_ALL_ACCESS | SETUP_KEY_WOW64,
			   0, &key, &disposition);
      if (rv != ERROR_ACCESS_DENIED || kr != HKEY_LOCAL_MACHINE)
	break;
      Log (LOG_PLAIN) << "Access denied trying to create rootdir registry key"
		      << endLog;
      kr = HKEY_CURRENT_USER;
    }
  while (rv == ERROR_ACCESS_DENIED);
  if (rv == ERROR_SUCCESS)
    do
      {
	rv = RegSetValueEx (key, "rootdir", 0, REG_SZ,
			    (BYTE *) get_root_dir ().c_str (),
			    get_root_dir ().size () + 1);
	if (rv != ERROR_ACCESS_DENIED || kr != HKEY_LOCAL_MACHINE)
	  break;
	Log (LOG_PLAIN) << "Access denied trying to create rootdir registry value"
			<< endLog;
	kr = HKEY_CURRENT_USER;
      }
    while (rv == ERROR_ACCESS_DENIED);
  if (rv != ERROR_SUCCESS)
    mbox (NULL, "Couldn't create registry key\n"
		      "to store installation path",
		"Cygwin Setup", MB_OK | MB_ICONWARNING);
  RegCloseKey (key);

  // The mount table is already in the right shape at this point.
  // Reading it again is not necessary.
  //read_mounts (std::string ());
}

inline char *
unconvert_slashes (char *in_name)
{
  char *name = in_name;
  while ((name = strchr (name, '/')) != NULL)
    *name++ = '\\';
  return in_name;
}

inline char *
skip_ws (char *in)
{
  while (*in == ' ' || *in == '\t')
    ++in;
  return in;
}

inline char *
find_ws (char *in)
{
  while (*in && *in != ' ' && *in != '\t')
    ++in;
  return in;
}

inline char *
conv_fstab_spaces (char *field)
{
  register char *sp = field;
  while ((sp = strstr (sp, "\\040")))
    {
      *sp++ = ' ';
      memmove (sp, sp + 3, strlen (sp + 3) + 1);
    }
  return field;
}

static bool got_usr_bin;
static bool got_usr_lib;

static bool
from_fstab_line (mnt *m, char *line)
{
  char *native_path, *posix_path, *fs_type;

  /* First field: Native path. */
  char *c = skip_ws (line);
  if (!*c || *c == '#')
    return false;
  char *cend = find_ws (c);
  *cend = '\0';
  native_path = conv_fstab_spaces (c);
  /* Second field: POSIX path. */
  c = skip_ws (cend + 1);
  if (!*c)
    return false;
  cend = find_ws (c);
  *cend = '\0';
  posix_path = conv_fstab_spaces (c);
  /* Third field: FS type. */
  c = skip_ws (cend + 1);
  if (!*c)
    return false;
  cend = find_ws (c);
  *cend = '\0';
  fs_type = c;

  if (strcmp (fs_type, "cygdrive"))
    {
      for (mnt *sm = mount_table; sm < m; ++sm)
	if (sm->posix == std::string (posix_path))
	  {
	    sm->native = std::string (unconvert_slashes (native_path));
	    return false;
	  }
      m->posix = std::string (posix_path);
      m->native = std::string (unconvert_slashes (native_path));
      if (!strcmp (posix_path, "/usr/bin"))
	got_usr_bin = true;
      else if (!strcmp (posix_path, "/usr/lib"))
	got_usr_lib = true;
    }
  return true;
}

#define BUFSIZE 65536
#define LFSTAB L"\\etc\\fstab"

static bool
from_fstab (mnt *m, const std::string& in_path)
{
  char buf[BUFSIZE];
  WCHAR path[in_path.size () + sizeof (LFSTAB)];

  mklongpath (path, in_path.c_str (), sizeof (path) / sizeof (WCHAR));
  wcscat (path, LFSTAB);
  HANDLE h = CreateFileW (path, GENERIC_READ, FILE_SHARE_READ, NULL,
                          OPEN_EXISTING,
			  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS,
			  NULL);
  if (h == INVALID_HANDLE_VALUE)
    return false;
  char *got = buf;
  DWORD len = 0;
  /* Using BUFSIZE-2 leaves space to append two \0. */
  while (ReadFile (h, got, BUFSIZE - 2 - (got - buf), &len, NULL))
    {
      char *end;

      /* Set end marker. */
      got[len] = got[len + 1] = '\0';
      /* Set len to the absolute len of bytes in buf. */
      len += got - buf;
      /* Reset got to start reading at the start of the buffer again. */
      got = buf;
      while (got < buf + len && (end = strchr (got, '\n')))
        {
          end[end[-1] == '\r' ? -1 : 0] = '\0';
          if (from_fstab_line (m, got))
            ++m;
          got = end + 1;
        }
      if (len < BUFSIZE - 1)
        break;
      /* We have to read once more.  Move remaining bytes to the start of
         the buffer and reposition got so that it points to the end of
         the remaining bytes. */
      len = buf + len - got;
      memmove (buf, got, len);
      got = buf + len;
      buf[len] = buf[len + 1] = '\0';
    }
  if (got > buf && from_fstab_line (m, got))
    ++m;
  CloseHandle (h);
  return true;
}

static void
add_usr_mnts (struct mnt *m)
{
  /* Set default /usr/bin and /usr/lib */
  if (!got_usr_bin)
    {
      m->posix = "/usr/bin";
      m->native = root_here->native + "\\bin";
      ++m;
    }
  if (!got_usr_lib)
    {
      m->posix = "/usr/lib";
      m->native = root_here->native + "\\lib";
    }
}

void
read_mounts (const std::string val)
{
  DWORD posix_path_size;
  struct mnt *m = mount_table;
  char buf[10000];

  root_here = NULL;
  for (mnt * m1 = mount_table; m1->posix.size (); m1++)
    {
      m1->posix.clear();
      m1->native.clear();
    }
  got_usr_bin = got_usr_lib = false;

  root_scope = (nt_sec.isRunAsAdmin ())? IDC_ROOT_SYSTEM : IDC_ROOT_USER;

  if (val.size ())
    {
      /* Cygwin rootdir always < MAX_PATH. */
      char rootdir[MAX_PATH + 1];

      if (GetFullPathName (val.c_str (), MAX_PATH + 1, rootdir, NULL))
	{
	  m->native = rootdir;
	  m->posix = "/";
	  root_here = m;
	  add_usr_mnts (++m);
	}
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
	  if (RegOpenKeyEx (key, buf, 0, KEY_ALL_ACCESS | SETUP_KEY_WOW64,
			    &key) != ERROR_SUCCESS)
	    continue;
	  DWORD type;
	  /* Cygwin rootdir always < MAX_PATH. */
	  char aBuffer[MAX_PATH + 1];
	  posix_path_size = MAX_PATH;
	  if (RegQueryValueEx
	      (key, "rootdir", 0, &type, (BYTE *) aBuffer,
	       &posix_path_size) == ERROR_SUCCESS)
	    {
	      m->native = std::string (aBuffer);
	      m->posix = "/";
	      root_scope = isuser ? IDC_ROOT_USER : IDC_ROOT_SYSTEM;
	      root_here = m++;
	      from_fstab (m, root_here->native);
	      add_usr_mnts (m);
	      break;
	    }
	  RegCloseKey (key);
	}
    }

  if (!root_here)
    {
      /* Affected path always < MAX_PATH. */
      char windir[MAX_PATH];
      GetSystemWindowsDirectory (windir, sizeof (windir));
      windir[2] = 0;
      m->native = std::string (windir) + (is_64bit ? "\\cygwin64" : "\\cygwin");
      m->posix = "/";
      root_here = m;
      add_usr_mnts (++m);
    }
}

void
set_root_dir (const std::string val)
{
  read_mounts (val);
}

static std::string empty;

const std::string &
get_root_dir ()
{
  return root_here ? root_here->native : empty;
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
