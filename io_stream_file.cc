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

#if defined(WIN32) && !defined (_CYGWIN_)
#include "win32.h"
#include "mklink2.h"
#endif

#include "mkdir.h"
  
  
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#include <stdexcept>
  
#include "io_stream_file.h"
#include "IOStreamProvider.h"

using namespace std;

/* completely private iostream registration class */
class FileProvider : public IOStreamProvider
{
public:
  int exists (const std::string& path) const
    {return io_stream_file::exists(path);}
  int remove (const std::string& path) const
    {return io_stream_file::remove(path);}
  int mklink (const std::string& a , const std::string& b, io_stream_link_t c) const
    {return io_stream_file::mklink(a,b,c);}
  io_stream *open (const std::string& a,const std::string& b) const
    {return new io_stream_file (a, b);}
  ~FileProvider (){}
  int move (const std::string& a,const std::string& b) const
    {return io_stream_file::move (a, b);}
  int mkdir_p (path_type_t isadir, const std::string& path) const
    {
      return ::mkdir_p (isadir == PATH_TO_DIR ? 1 : 0, path.c_str());
    }
protected:
  FileProvider() // no creating this
    {
      io_stream::registerProvider (theInstance, "file://");
    }
  FileProvider(FileProvider const &); // no copying
  FileProvider &operator=(FileProvider const &); // no assignment
private:
  static FileProvider theInstance;
};
FileProvider FileProvider::theInstance = FileProvider();
  

/* for set_mtime */
#define FACTOR (0x19db1ded53ea710LL)
#define NSPERSEC 10000000LL

io_stream_file::io_stream_file (const std::string& name, const std::string& mode):
fp(), fname(name),lmode (mode)
{
  errno = 0;
  if (!name.size() || !mode.size())
    return;
  fp = fopen (name.c_str(), mode.c_str());
  if (!fp)
    lasterr = errno;
}

io_stream_file::~io_stream_file ()
{
  if (fp)
    fclose (fp);
}

int
io_stream_file::exists (const std::string& path)
{
#if defined(WIN32) && !defined (_CYGWIN_)
  if (_access (path.c_str(), 0) == 0)
#else
  if (access (path.c_str(), F_OK) == 0)
#endif
    return 1;
  return 0;
}

int
io_stream_file::remove (const std::string& path)
{
  if (!path.size())
    return 1;
#if defined(WIN32) && !defined (_CYGWIN_)
  unsigned long w = GetFileAttributes (path.c_str());
  if (w != 0xffffffff && w & FILE_ATTRIBUTE_DIRECTORY)
    {
      char *tmp = new char [path.size() + 10];
      int i = 0;
      do
	{
	  i++;
	  sprintf (tmp, "%s.old-%d", path.c_str(), i);
	}
      while (GetFileAttributes (tmp) != 0xffffffff);
      fprintf (stderr, "warning: moving directory \"%s\" out of the way.\n",
	       path.c_str());
      MoveFile (path.c_str(), tmp);
      delete[] tmp;
    }
  SetFileAttributes (path.c_str(), w & ~FILE_ATTRIBUTE_READONLY);
  return !DeleteFileA (path.c_str());
#else
  // FIXME: try rmdir if unlink fails - remove the dir
  return unlink (path.c_str());
#endif
}

int
io_stream_file::mklink (const std::string& from, const std::string& to,
			io_stream_link_t linktype)
{
  if (!from.size() || !to.size())
    return 1;
#if defined(WIN32) && !defined (_CYGWIN_)
  switch (linktype)
    {
    case IO_STREAM_SYMLINK:
      return mkcygsymlink (from.c_str(), to.c_str());
    case IO_STREAM_HARDLINK:
      return 1;
    }
#else
  switch (linktype)
    {
    case IO_STREAM_SYMLINK:
      return symlink (to.c_str(), from.c_str());
    case IO_STREAM_HARDLINK:
      return link (to.c_str(), from.c_str());
    }
#endif
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
#if defined(WIN32) && !defined (_CYGWIN_)
  long long ftimev = mtime * NSPERSEC + FACTOR;
  FILETIME ftime;
  ftime.dwHighDateTime = ftimev >> 32;
  ftime.dwLowDateTime = ftimev;
  HANDLE h =
    CreateFileA (fname.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
		 0, OPEN_EXISTING,
		 FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS, 0);
  if (h)
    {
      SetFileTime (h, 0, 0, &ftime);
      CloseHandle (h);
      return 0;
    }
#else
  throw new runtime_error ("set_mtime not supported on posix yet.");
#endif
  return 1;
}

int
io_stream_file::move (const std::string& from, const std::string& to)
{
  if (!from.size()|| !to.size())
    return 1;
  return rename (from.c_str(), to.c_str());
}

size_t
io_stream_file::get_size ()
{
  if (!fname.size())
    return 0;
#if defined(WIN32) && !defined (_CYGWIN_)
  HANDLE h;
  WIN32_FIND_DATA buf;
  DWORD ret = 0;

  h = FindFirstFileA (fname.c_str(), &buf);
  if (h != INVALID_HANDLE_VALUE)
    {
      if (buf.nFileSizeHigh == 0)
	ret = buf.nFileSizeLow;
      FindClose (h);
    }
  return ret;
#else
  struct stat buf;
  if (stat(fname.c_str(), &buf))
    throw new runtime_error ("Failed to stat file - has it been deleted?");
  return buf.st_size;
#endif
}
