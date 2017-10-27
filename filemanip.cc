/*
 * Copyright (c) 2000, 2001, Red Hat, Inc.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Robert Collins <rbtcollins@redhat.com>
 *
 */

/* The purpose of this file is to put all general purpose file manipulation
   code in one place. */

#include <string.h>
#include <wchar.h>
#include <stdlib.h>
#include "filemanip.h"
#include "io_stream.h"
#include "String++.h"
#include "win32.h"
#include "ntdll.h"
#include "io.h"
#include "fcntl.h"

using namespace std;

/* legacy wrapper.
 * Clients should use io_stream.get_size() */
size_t
get_file_size (const std::string& name)
{
  io_stream *theFile = io_stream::open (name, "", 0);
  if (!theFile)
    /* To consider: throw an exception ? */
    return 0;
  ssize_t rv = theFile->get_size();
  delete theFile;
  return rv;
}

/* returns the number of characters of path that
 * precede the extension
 */
int
find_tar_ext (const char *path)
{
  char *p = strchr (path, '\0') - 9;
  if (p <= path)
    return 0;
  if ((p = strstr (p, ".tar")) != NULL)
    return p - path;
  else
    return 0;
}

/* Parse a filename into package, version, and extension components. */
int
parse_filename (const string &fn, fileparse & f)
{
  char *p, *ver;
  int n;

  if (!(n = find_tar_ext (fn.c_str ())))
    return 0;

  f.pkg = "";
  f.what = "";

  f.tail = fn.substr (n, string::npos);

  p = new_cstr_char_array (fn.substr (0, n));
  char const *ext;
  /* TODO: make const and non-const trail variant. */
  if ((ext = trail (p, "-src")))
    {
      f.what = "-src";
      *(char *)ext = '\0';
    }
  else if ((ext = trail (p, "-patch")))
    {
      f.what = "-patch";
      *(char *)ext = '\0';
    }
  for (ver = p; *ver; ver++)
    if (*ver == '-')
      {
        if (isdigit (ver[1]))
          {
            *ver++ = 0;
            f.pkg = p;
            break;
          }
        else if (strcasecmp (ver, "-src") == 0 ||
  	         strcasecmp (ver, "-patch") == 0)
  	  {
            *ver++ = 0;
            f.pkg = p;
            f.what = strlwr (ver);
            ver = strchr (ver, '\0');
            break;
  	  }
      }

  if (!f.pkg.size())
    f.pkg = p;

  if (!f.what.size())
    {
      int n;
      char *p1 = strchr (ver, '\0');
      if ((p1 -= 4) >= ver && strcasecmp (p1, "-src") == 0)
	n = 4;
      else if ((p1 -= 2) >= ver && strcasecmp (p1, "-patch") == 0)
	n = 6;
      else
	n = 0;
      if (n)
	{
	  // get the 'src' or 'patch'.
	  f.what = p1 + 1;
	}
    }

  f.ver = *ver ? ver : "0.0";
  delete[] p;
  return 1;
}

const char *
trail (const char *haystack, const char *needle)
{
  /* See if the path ends with a specific suffix.
     Just return if it doesn't. */
  unsigned len = strlen (haystack);
  int prefix_len = len - strlen (needle);
  if (prefix_len < 0
      || strcasecmp (haystack += prefix_len, needle) != 0)
    return NULL;
  return haystack;
}

std::string
backslash(const std::string& s)
{
  std::string rv(s);
  
  for (std::string::iterator it = rv.begin(); it != rv.end(); ++it)
    if (*it == '/')
      *it = '\\';
    
  return rv;
}

wchar_t tfx_chars[] = {
      0, 0xf001, 0xf002, 0xf003, 0xf004, 0xf005, 0xf006, 0xf007,
 0xf008, 0xf009, 0xf00a, 0xf00b, 0xf00c, 0xf00d, 0xf00e, 0xf00f,
 0xf010, 0xf011, 0xf012, 0xf013, 0xf014, 0xf015, 0xf016, 0xf017,
 0xf018, 0xf019, 0xf01a, 0xf01b, 0xf01c, 0xf01d, 0xf01e, 0xf01f,
 ' ', '!', 0xf000 | '"', '#', '$', '%', '&',  39,
 '(', ')', 0xf000 | '*', '+', ',', '-', '.', '\\',
 '0', '1', '2', '3', '4', '5', '6', '7', 
 '8', '9', 0xf000 | ':', ';', 0xf000 | '<', '=', 0xf000 | '>', 0xf000 | '?',
 '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
 'X', 'Y', 'Z', '[',  '\\', ']', '^', '_',
 '`', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
 'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
 'x', 'y', 'z', '{', 0xf000 | '|', '}', '~', 127
};

int
mklongpath (wchar_t *tgt, const char *src, size_t len)
{
  wchar_t *tp, *ts;
  size_t ret, n;
  mbstate_t mb;

  wcscpy (tgt, L"\\\\?\\");
  tp = tgt + 4;
  len -= 4;
  if (isdirsep (src[0]) && isdirsep (src[1]))
    {
      wcscpy (tp, L"UNC");
      tp += 3;
      len -= 3;
      ts = tp;
      ++src;		/* Skip one leading backslash. */
    }
  else
    ts = tp + 2;	/* Skip colon in leading drive specifier. */
  ret = tp - tgt;
  memset (&mb, 0, sizeof mb);
  while (len > 0)
    {
      n = mbrtowc (tp, src, 6, &mb);
      if (n == (size_t) -1 || n == (size_t) -2)
      	return -1;
      if (n == 0)
      	break;
      src += n;
      /* Transform char according to Cygwin rules. */
      if (tp >= ts && *tp < 128)
	*tp = tfx_chars[*tp];
      /* Skip multiple backslashes. */
      if (*tp == L'\\' && tp[-1] == L'\\')
	continue;
      /* Skip "." and ".." path components.  They result in annoying error
	 messages. */
      if (*tp == L'.' && tp[-1] == L'\\')
      	{
	  if (!src[0])
	    continue;
	  if (isdirsep (src[0]))
	    {
	      ++src;
	      continue;
	    }
	  if (src[0] == '.' && isdirsep (src[1]))
	    {
	      src += 2;
	      /* Set tp back to start of previous path component. */
	      if (tp > ts + 1)
	      	do
		  {
		    --tp;
		    --ret;
		    ++len;
		  }
		while (tp > ts && tp[-1] != L'\\');
	      continue;
	    }
	}
      ++ret;
      ++tp;
      --len;
    }
  if (len == 0)
    return -1;
  /* Always remove trailing backslash. */
  if (tgt[ret - 1] == L'\\')
    tgt[--ret] = L'\0';
  return 0;
}

/* Replacement functions for Win32 API functions.  The joke here is that the
   replacement functions always use the FILE_OPEN_FOR_BACKUP_INTENT flag. */

extern "C" DWORD WINAPI
GetFileAttributesW (LPCWSTR wpath)
{
  NTSTATUS status;
  HANDLE h;
  IO_STATUS_BLOCK io;
  UNICODE_STRING uname;
  OBJECT_ATTRIBUTES attr;
  DWORD ret = INVALID_FILE_ATTRIBUTES;

  PWCHAR wname = (PWCHAR) wpath;
  wname[1] = L'?';
  RtlInitUnicodeString (&uname, wname);
  InitializeObjectAttributes (&attr, &uname, OBJ_CASE_INSENSITIVE, NULL, NULL);
  status = NtOpenFile (&h, READ_CONTROL | FILE_READ_ATTRIBUTES, &attr, &io,
		       FILE_SHARE_VALID_FLAGS,
                       FILE_OPEN_FOR_BACKUP_INTENT | FILE_OPEN_REPARSE_POINT);
  wname[1] = L'\\';
  if (NT_SUCCESS (status))
    {
      FILE_BASIC_INFORMATION fbi;

      status = NtQueryInformationFile (h, &io, &fbi, sizeof fbi,
				       FileBasicInformation);
      if (NT_SUCCESS (status))
	ret = fbi.FileAttributes;
      NtClose (h);
    }
  if (!NT_SUCCESS (status))
    SetLastError (RtlNtStatusToDosError (status));
  return ret;
}

extern "C" BOOL WINAPI
SetFileAttributesW (LPCWSTR wpath, DWORD attribs)
{
  NTSTATUS status;
  HANDLE h;
  IO_STATUS_BLOCK io;
  UNICODE_STRING uname;
  OBJECT_ATTRIBUTES attr;

  PWCHAR wname = (PWCHAR) wpath;
  wname[1] = L'?';
  RtlInitUnicodeString (&uname, wname);
  InitializeObjectAttributes (&attr, &uname, OBJ_CASE_INSENSITIVE, NULL, NULL);
  status = NtOpenFile (&h, READ_CONTROL | FILE_WRITE_ATTRIBUTES, &attr, &io,
		       FILE_SHARE_VALID_FLAGS,
                       FILE_OPEN_FOR_BACKUP_INTENT | FILE_OPEN_REPARSE_POINT);
  wname[1] = L'\\';
  if (NT_SUCCESS (status))
    {
      FILE_BASIC_INFORMATION fbi;

      memset (&fbi, 0, sizeof fbi);
      fbi.FileAttributes = attribs ?: FILE_ATTRIBUTE_NORMAL;
      status = NtSetInformationFile (h, &io, &fbi, sizeof fbi,
				     FileBasicInformation);
      NtClose (h);
    }
  if (!NT_SUCCESS (status))
    SetLastError (RtlNtStatusToDosError (status));
  return NT_SUCCESS (status);
}

extern "C" BOOL WINAPI
MoveFileW (LPCWSTR from, LPCWSTR to)
{
  NTSTATUS status;
  HANDLE h;
  IO_STATUS_BLOCK io;
  UNICODE_STRING uname;
  OBJECT_ATTRIBUTES attr;

  PWCHAR wfrom = (PWCHAR) from;
  wfrom[1] = L'?';
  RtlInitUnicodeString (&uname, wfrom);
  InitializeObjectAttributes (&attr, &uname, OBJ_CASE_INSENSITIVE, NULL, NULL);
  status = NtOpenFile (&h, READ_CONTROL | DELETE,
		       &attr, &io, FILE_SHARE_VALID_FLAGS,
		       FILE_OPEN_FOR_BACKUP_INTENT | FILE_OPEN_REPARSE_POINT);
  wfrom[1] = L'\\';
  if (NT_SUCCESS (status))
    {
      size_t len = wcslen (to) * sizeof (WCHAR);
      PFILE_RENAME_INFORMATION pfri = (PFILE_RENAME_INFORMATION)
	malloc (sizeof (FILE_RENAME_INFORMATION) + len);
      pfri->ReplaceIfExists = TRUE;
      pfri->RootDirectory = NULL;
      pfri->FileNameLength = len;
      memcpy (pfri->FileName, to, len);
      pfri->FileName[1] = L'?';
      status = NtSetInformationFile(h, &io, pfri,
				    sizeof (FILE_RENAME_INFORMATION) + len,
				    FileRenameInformation);
      free (pfri);
      NtClose (h);
    }
  if (!NT_SUCCESS (status))
    SetLastError (RtlNtStatusToDosError (status));
  return NT_SUCCESS (status);
}

static BOOL
unlink (LPCWSTR wpath, ULONG file_attr)
{
  NTSTATUS status;
  HANDLE h;
  IO_STATUS_BLOCK io;
  UNICODE_STRING uname;
  OBJECT_ATTRIBUTES attr;

  PWCHAR wname = (PWCHAR) wpath;
  wname[1] = L'?';
  RtlInitUnicodeString (&uname, wname);
  InitializeObjectAttributes (&attr, &uname, OBJ_CASE_INSENSITIVE, NULL, NULL);
  status = NtOpenFile (&h, READ_CONTROL | DELETE,
		       &attr, &io, FILE_SHARE_VALID_FLAGS,
		       FILE_OPEN_FOR_BACKUP_INTENT
		       | FILE_OPEN_REPARSE_POINT
		       | file_attr);
  wname[1] = L'\\';
  if (NT_SUCCESS (status))
    {
      FILE_DISPOSITION_INFORMATION fdi = { TRUE };
      status = NtSetInformationFile (h, &io, &fdi, sizeof fdi,
				     FileDispositionInformation);
      NtClose (h);
    }
  if (!NT_SUCCESS (status))
    SetLastError (RtlNtStatusToDosError (status));
  return NT_SUCCESS (status);
}

extern "C" BOOL WINAPI
DeleteFileW (LPCWSTR wpath)
{
  return unlink (wpath, FILE_NON_DIRECTORY_FILE);
}

extern "C" BOOL WINAPI
RemoveDirectoryW (LPCWSTR wpath)
{
  return unlink (wpath, FILE_DIRECTORY_FILE);
}

/* Perms of 0 means no POSIX perms. */
FILE *
nt_wfopen (const wchar_t *wpath, const char *mode, mode_t perms)
{
  NTSTATUS status;
  HANDLE h;
  IO_STATUS_BLOCK io;
  UNICODE_STRING uname;
  OBJECT_ATTRIBUTES attr;
  SECURITY_DESCRIPTOR sd;
  acl_t acl;
  const char *c;
  ULONG access, disp;
  int oflags = 0;

  switch (mode[0])
    {
    case 'r':
      access = STANDARD_RIGHTS_READ | GENERIC_READ;
      disp = FILE_OPEN;
      break;
    case 'w':
      access = STANDARD_RIGHTS_WRITE | GENERIC_WRITE;
      disp = FILE_OVERWRITE_IF;
      break;
    case 'a':
      access = STANDARD_RIGHTS_WRITE | GENERIC_WRITE;
      disp = FILE_OPEN_IF;
      oflags = _O_APPEND;
      break;
    default:
      errno = EINVAL;
      return NULL;
    }
  for (c = mode + 1; *c; ++c)
    switch (*c)
      {
      case '+':
	access = STANDARD_RIGHTS_WRITE | GENERIC_READ | GENERIC_WRITE;
	break;
      case 't':
      	oflags |= _O_TEXT;
	break;
      case 'b':
	oflags |= _O_BINARY;
	break;
      default:
	errno = EINVAL;
	return NULL;
      }
  switch (access)
    {
    case GENERIC_READ:
      oflags |= _O_RDONLY;
      break;
    case GENERIC_WRITE:
      oflags |= _O_WRONLY;
      break;
    case GENERIC_READ | GENERIC_WRITE:
      oflags |= _O_RDWR;
      break;
    }
  PWCHAR wname = (PWCHAR) wpath;
  wname[1] = L'?';
  RtlInitUnicodeString (&uname, wname);
  InitializeObjectAttributes (&attr, &uname, OBJ_CASE_INSENSITIVE, NULL,
			      disp == FILE_OPEN || perms == 0
			      ? NULL
			      : nt_sec.GetPosixPerms ("", NULL, NULL,
						      perms, sd, acl));
  status = NtCreateFile (&h, access | SYNCHRONIZE, &attr, &io, NULL,
			 FILE_ATTRIBUTE_NORMAL, FILE_SHARE_VALID_FLAGS, disp, 
			 FILE_OPEN_FOR_BACKUP_INTENT
			 | FILE_OPEN_REPARSE_POINT
			 | FILE_SYNCHRONOUS_IO_NONALERT,
			 NULL, 0);
  if (!NT_SUCCESS (status))
    {
      if (status == STATUS_OBJECT_NAME_NOT_FOUND
	  || status == STATUS_OBJECT_PATH_NOT_FOUND
	  || status == STATUS_NO_SUCH_FILE)
      	errno = ENOENT;
      else if (status == STATUS_OBJECT_NAME_INVALID)
      	errno = EINVAL;
      else
      	errno = EACCES;
      wname[1] = L'\\';
      return NULL;
    }
  wname[1] = L'\\';
  int fd = _open_osfhandle ((intptr_t) h, oflags);
  if (fd < 0)
    return NULL;
  return _fdopen (fd, mode);
}

FILE *
nt_fopen (const char *path, const char *mode, mode_t perms)
{
  size_t len = strlen (path) + 8;
  WCHAR wpath[len];
  mklongpath (wpath, path, len);
  return nt_wfopen (wpath, mode, perms);
}

/* Override C file io functions for the sake of the download side of setup.
   These overrides eliminate the problem that the ANSI file API limits the
   filename length to MAX_PATH. */
extern "C" int
remove (const char *path)
{
  size_t len = strlen (path) + 8;
  WCHAR wpath[len];
  mklongpath (wpath, path, len);
  return unlink (wpath, 0) ? 0 : -1;
}

extern "C" int
rename (const char *oldpath, const char *newpath)
{
  size_t len = strlen (oldpath) + 8;
  WCHAR woldpath[len];
  mklongpath (woldpath, oldpath, len);

  len = strlen (newpath) + 8;
  WCHAR wnewpath[len];
  mklongpath (wnewpath, newpath, len);

  return MoveFileW (woldpath, wnewpath) ? 0 : -1;
}

extern "C" int
_access (const char *path, int /* ignored */)
{
  size_t len = strlen (path) + 8;
  WCHAR wpath[len];
  mklongpath (wpath, path, len);
  return (GetFileAttributesW (wpath) != INVALID_FILE_ATTRIBUTES) ? 0 : -1;
}
