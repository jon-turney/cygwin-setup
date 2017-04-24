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

#include "win32.h"
#include "mklink2.h"
#include "filemanip.h"
#include "mkdir.h"
  
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
  
#include "io_stream_file.h"
#include "IOStreamProvider.h"
#include "LogSingleton.h"

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
  io_stream *open (const std::string& a,const std::string& b, mode_t m) const
    {return new io_stream_file (a, b, m);}
  ~FileProvider (){}
  int move (const std::string& a,const std::string& b) const
    {return io_stream_file::move (a, b);}
  int mkdir_p (path_type_t isadir, const std::string& path, mode_t mode) const
    {
      return ::mkdir_p (isadir == PATH_TO_DIR ? 1 : 0, path.c_str(), mode);
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
  
wchar_t *
io_stream_file::w_str ()
{
  if (!wname)
    {
      wname = new wchar_t [fname.size () + 7];
      if (wname)
	mklongpath (wname, fname.c_str (), fname.size () + 7);
    }
  return wname;
}

io_stream_file::io_stream_file (const std::string& name, const std::string& mode, mode_t perms) : fp(), lasterr (0), fname(name), wname (NULL)
{
  errno = 0;
  if (!name.size())
    return;
  if (mode.size ())
    {
      fp = nt_wfopen (w_str (), mode.c_str (), perms);
      if (!fp)
	lasterr = errno;
    }
}

io_stream_file::~io_stream_file ()
{
  if (fp)
    fclose (fp);
  if (wname)
    delete [] wname;
}

int
io_stream_file::exists (const std::string& path)
{
  DWORD attr;
  size_t len = 2 * path.size () + 7;
  WCHAR wname[len];
  mklongpath (wname, path.c_str (), len);
  attr = GetFileAttributesW (wname);
  return attr != INVALID_FILE_ATTRIBUTES
	 && !(attr & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_DEVICE));
}

int
io_stream_file::remove (const std::string& path)
{
  if (!path.size())
    return 1;
  size_t len = path.size () + 7;
  WCHAR wpath[len];
  mklongpath (wpath, path.c_str (), len);

  unsigned long w = GetFileAttributesW (wpath);
  if (w == INVALID_FILE_ATTRIBUTES)
    return 0;
  if (w & FILE_ATTRIBUTE_DIRECTORY)
    {
      len = wcslen (wpath);
      WCHAR tmp[len + 10];
      wcscpy (tmp, wpath);
      int i = 0;
      do
	{
	  i++;
	  swprintf (tmp + len, L"old-%d", i);
	}
      while (GetFileAttributesW (tmp) != INVALID_FILE_ATTRIBUTES);
      Log (LOG_TIMESTAMP) << "warning: moving directory \"" << path.c_str()
                          << "\" out of the way." << endLog;
      MoveFileW (wpath, tmp);
    }
  SetFileAttributesW (wpath, w & ~FILE_ATTRIBUTE_READONLY);
  return !DeleteFileW (wpath);
}

int
io_stream_file::mklink (const std::string& from, const std::string& to,
			io_stream_link_t linktype)
{
  if (!from.size() || !to.size())
    return 1;
  switch (linktype)
    {
    case IO_STREAM_SYMLINK:
      return mkcygsymlink (from.c_str(), to.c_str());
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
io_stream_file::set_mtime (time_t mtime)
{
  if (!fname.size())
    return 1;
  if (fp)
    fclose (fp);
  long long ftimev = mtime * NSPERSEC + FACTOR;
  FILETIME ftime;
  ftime.dwHighDateTime = ftimev >> 32;
  ftime.dwLowDateTime = ftimev;
  HANDLE h;
  h = CreateFileW (w_str(), GENERIC_WRITE,
		   FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
		   FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS, 0);
  if (h != INVALID_HANDLE_VALUE)
    {
      SetFileTime (h, 0, 0, &ftime);
      CloseHandle (h);
      return 0;
    }
  return 1;
}

int
io_stream_file::move (const std::string& from, const std::string& to)
{
  if (!from.size()|| !to.size())
    return 1;
  return rename (from.c_str(), to.c_str());
}

/* This only handles file < 2GB.  The chance that files in the distro
   get bigger is rather small, though... */
size_t
io_stream_file::get_size ()
{
  if (!fname.size())
    return 0;
  HANDLE h;
  DWORD ret = 0;
  h = CreateFileW (w_str (), GENERIC_READ,
		   FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
		   FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS, 0);
  if (h != INVALID_HANDLE_VALUE)
    {
      ret = GetFileSize (h, NULL);
      CloseHandle (h);
    }
  return ret;
}
