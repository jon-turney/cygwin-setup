/*
 * Copyright (c) 2001, Robert Collins.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Robert Collins  <rbtcollins@hotmail.com>
 *
 */

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include "win32.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include "log.h"
#include "port.h"
#include "mklink2.h"

#include "filemanip.h"

#include "io_stream.h"
#include "io_stream_file.h"

/* for set_mtime */
#define FACTOR (0x19db1ded53ea710LL)
#define NSPERSEC 10000000LL

io_stream_file::io_stream_file (String const &name, String const &mode):
fp(), fname(name),lmode (mode)
{
  errno = 0;
  if (!name.size() || !mode.size())
    return;
  fp = fopen (name.cstr_oneuse(), mode.cstr_oneuse());
  if (!fp)
    lasterr = errno;
}

io_stream_file::~io_stream_file ()
{
  if (fp)
    fclose (fp);
  destroyed = 1;
}

int
io_stream_file::exists (String const &path)
{
  if (_access (path.cstr_oneuse(), 0) == 0)
    return 1;
  return 0;
}

int
io_stream_file::remove (String const &path)
{
  if (!path.size())
    return 1;

  unsigned long w = GetFileAttributes (path.cstr_oneuse());
  if (w != 0xffffffff && w & FILE_ATTRIBUTE_DIRECTORY)
    {
      char *tmp = new char [path.size() + 10];
      int i = 0;
      do
	{
	  i++;
	  sprintf (tmp, "%s.old-%d", path.cstr_oneuse(), i);
	}
      while (GetFileAttributes (tmp) != 0xffffffff);
      fprintf (stderr, "warning: moving directory \"%s\" out of the way.\n",
	       path.cstr_oneuse());
      MoveFile (path.cstr_oneuse(), tmp);
      delete[] tmp;
    }
  return !DeleteFileA (path.cstr_oneuse());

}

int
io_stream_file::mklink (String const &from, String const &to,
			io_stream_link_t linktype)
{
  if (!from.size() || !to.size())
    return 1;
  switch (linktype)
    {
    case IO_STREAM_SYMLINK:
      return mkcygsymlink (from.cstr_oneuse(), to.cstr_oneuse());
    case IO_STREAM_HARDLINK:
      return 1;
    }
  return 1;
}

/* virtuals */


ssize_t
io_stream_file::read (void *buffer, size_t len)
{
  if (fp)
    return fread (buffer, 1, len, fp);
  return 0;
}

ssize_t
io_stream_file::write (const void *buffer, size_t len)
{
  if (fp)
    return fwrite (buffer, 1, len, fp);
  return 0;
}

ssize_t
io_stream_file::peek (void *buffer, size_t len)
{
  log (LOG_TIMESTAMP, "io_stream_file::peek called");
  if (fp)
    {
      int pos = ftell (fp);
      ssize_t rv = fread (buffer, 1, len, fp);
      fseek (fp, pos, SEEK_SET);
      return rv;
    }
  return 0;
}

long
io_stream_file::tell ()
{
  if (fp)
    {
      return ftell (fp);
    }
  return 0;
}

int
io_stream_file::seek (long where, io_stream_seek_t whence)
{
  if (fp)
    {
      return fseek (fp, where, (int) whence);
    }
  lasterr = EBADF;
  return -1;
}

int
io_stream_file::error ()
{
  if (fp)
    return ferror (fp);
  return lasterr;
}

int
io_stream_file::set_mtime (int mtime)
{
  if (!fname.size())
    return 1;
  if (fp)
    fclose (fp);
  long long ftimev = mtime * NSPERSEC + FACTOR;
  FILETIME ftime;
  ftime.dwHighDateTime = ftimev >> 32;
  ftime.dwLowDateTime = ftimev;
  HANDLE h =
    CreateFileA (fname.cstr_oneuse(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
		 0, OPEN_EXISTING,
		 FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS, 0);
  if (h)
    {
      SetFileTime (h, 0, 0, &ftime);
      CloseHandle (h);
      return 0;
    }
  return 1;
}

int
io_stream_file::move (String const &from, String const &to)
{
  if (!from.size()|| !to.size())
    return 1;
  return rename (from.cstr_oneuse(), to.cstr_oneuse());
}

size_t
io_stream_file::get_size ()
{
  if (!fname.size())
    return 0;
  return get_file_size (fname);
}
