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

/* see mkdir.h */

#include "win32.h"
#include "ntdll.h"

#include <sys/stat.h>

#include "mkdir.h"
#include "filemanip.h"
#include "LogSingleton.h"

/* Return 0 on success.
   A mode of 0 means no POSIX perms. */
int
mkdir_p (int isadir, const char *in_path, mode_t mode)
{
  char saved_char, *slash = 0;
  char *c;
  const size_t len = strlen (in_path) + 1;
  char path[len];
  DWORD d, gse;
  WCHAR wpath[len + 6];

  strcpy (path, in_path);
  mklongpath (wpath, path, len + 6);

  d = GetFileAttributesW (wpath);
  if (d != INVALID_FILE_ATTRIBUTES && d & FILE_ATTRIBUTE_DIRECTORY)
    return 0;

  if (isadir)
    {
      NTSTATUS status;
      HANDLE dir;
      UNICODE_STRING upath;
      OBJECT_ATTRIBUTES attr;
      IO_STATUS_BLOCK io;
      SECURITY_DESCRIPTOR sd;
      acl_t acl;

      wpath[1] = '?';
      upath.Length = wcslen (wpath) * sizeof (WCHAR);
      upath.MaximumLength = upath.Length + sizeof (WCHAR);
      upath.Buffer = wpath;
      InitializeObjectAttributes (&attr, &upath, OBJ_CASE_INSENSITIVE, NULL,
				  mode == 0 ? NULL
				  : nt_sec.GetPosixPerms (path, NULL, NULL,
							  S_IFDIR | mode,
							  sd, acl));
      status = NtCreateFile (&dir, SYNCHRONIZE | READ_CONTROL
			     | FILE_LIST_DIRECTORY,
			     &attr, &io, NULL, FILE_ATTRIBUTE_DIRECTORY,
			     FILE_SHARE_VALID_FLAGS, FILE_CREATE,
			     FILE_DIRECTORY_FILE
			     | FILE_SYNCHRONOUS_IO_NONALERT
			     | FILE_OPEN_FOR_BACKUP_INTENT, NULL, 0);
      if (NT_SUCCESS (status))
	{
	  NtClose (dir);
	  return 0;
	}
      else
	SetLastError (RtlNtStatusToDosError (status));
      gse = GetLastError ();
      if (gse != ERROR_PATH_NOT_FOUND && gse != ERROR_FILE_NOT_FOUND)
	{
	  if (gse == ERROR_ALREADY_EXISTS)
	    {
              Log (LOG_TIMESTAMP) << "warning: deleting \"" << path
                                  << "\" so I can make a directory there" << endLog;
	      if (DeleteFileW (wpath))
		return mkdir_p (isadir, path, mode ? 0755 : 0);
	    }
	  return 1;
	}
    }

  for (c = path; *c; c++)
    {
      if (*c == '/' || *c == '\\')
	slash = c;
    }

  if (!slash)
    return 0;

  // Trying to create a drive... It's time to give up.
  if (((slash - path) == 2) && (path[1] == ':'))
    return 1;

  saved_char = *slash;
  *slash = 0;
  if (mkdir_p (1, path, mode ? 0755 : 0))
    {
      *slash = saved_char;
      return 1;
    }
  
  *slash = saved_char;

  if (!isadir)
    return 0;

  return mkdir_p (isadir, path, mode);
}
