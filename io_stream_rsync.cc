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

#include "io_stream_rsync.h"
#include "IOStreamProvider.h"
#include "rsync/rsync.h"


/* completely private iostream registration class */
class RSyncProvider : public IOStreamProvider
{
public:
  int exists (String const &path) const
    {
      return io_stream_rsync::exists(path);
    }
  int remove (String const &path) const
    {
      /* Not currently supported */
      return 1;
    }
  int mklink (String const &a , String const &b, io_stream_link_t c) const
    {
      return io_stream_rsync::mklink(a,b,c);
    }
  io_stream *open (String const &a,String const &b) const
    {
      return new io_stream_rsync (a, b);
    }
  ~RSyncProvider (){}
  int move (String const &a,String const &b) const
    {
      /* Not supported yet */
      return 0;
    }
  int mkdir_p (enum path_type_t isadir, String const &path) const
    {
      /* Not supported yet */
      return 0;
    }
protected:
  RSyncProvider() // no creating this
    {
      io_stream::registerProvider (theInstance, "rsync://");
    }
  RSyncProvider(RSyncProvider const &); // no copying
  RSyncProvider &operator=(RSyncProvider const &); // no assignment
private:
  static RSyncProvider theInstance;
};
RSyncProvider RSyncProvider::theInstance = RSyncProvider();
  
io_stream_rsync::io_stream_rsync (String const &name, String const &mode):
fp(), fname(name),lmode (mode)
{
  errno = 0;
  if (!name.size() || !mode.size())
    return;
  fp = fopen (name.cstr_oneuse(), mode.cstr_oneuse());
  if (!fp)
    lasterr = errno;
}

io_stream_rsync::~io_stream_rsync ()
{
  if (fp)
    fclose (fp);
  destroyed = 1;
}

int
io_stream_rsync::exists (String const &path)
{
#if defined(WIN32) && !defined (_CYGWIN_)
  if (_access (path.cstr_oneuse(), 0) == 0)
#else
  if (access (path.cstr_oneuse(), F_OK) == 0)
#endif
    return 1;
  return 0;
}

int
io_stream_rsync::remove (String const &path)
{
  if (!path.size())
    return 1;
#if defined(WIN32) && !defined (_CYGWIN_)
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
#else
  // FIXME: try rmdir if unlink fails - remove the dir
  return unlink (path.cstr_oneuse());
#endif
}

int
io_stream_rsync::mklink (String const &from, String const &to,
			io_stream_link_t linktype)
{
  if (!from.size() || !to.size())
    return 1;
#if defined(WIN32) && !defined (_CYGWIN_)
  switch (linktype)
    {
    case IO_STREAM_SYMLINK:
      return mkcygsymlink (from.cstr_oneuse(), to.cstr_oneuse());
    case IO_STREAM_HARDLINK:
      return 1;
    }
#else
  switch (linktype)
    {
    case IO_STREAM_SYMLINK:
      return symlink (to.cstr_oneuse(), from.cstr_oneuse());
    case IO_STREAM_HARDLINK:
      return link (to.cstr_oneuse(), from.cstr_oneuse());
    }
#endif
  return 1;
}

/* virtuals */


ssize_t
io_stream_rsync::read (void *buffer, size_t len)
{
  if (fp)
    return fread (buffer, 1, len, fp);
  return 0;
}

ssize_t
io_stream_rsync::write (const void *buffer, size_t len)
{
  if (fp)
    return fwrite (buffer, 1, len, fp);
  return 0;
}

ssize_t
io_stream_rsync::peek (void *buffer, size_t len)
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
io_stream_rsync::tell ()
{
  if (fp)
    {
      return ftell (fp);
    }
  return 0;
}

int
io_stream_rsync::seek (long where, io_stream_seek_t whence)
{
  if (fp)
    {
      return fseek (fp, where, (int) whence);
    }
  lasterr = EBADF;
  return -1;
}

int
io_stream_rsync::error ()
{
  if (fp)
    return ferror (fp);
  return lasterr;
}

int
io_stream_rsync::set_mtime (int mtime)
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
    CreateFileA (fname.cstr_oneuse(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
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
io_stream_rsync::move (String const &from, String const &to)
{
  if (!from.size()|| !to.size())
    return 1;
  return rename (from.cstr_oneuse(), to.cstr_oneuse());
}

size_t
io_stream_rsync::get_size ()
{
  if (!fname.size())
    return 0;
#if defined(WIN32) && !defined (_CYGWIN_)
  HANDLE h;
  WIN32_FIND_DATA buf;
  DWORD ret = 0;

  h = FindFirstFileA (fname.cstr_oneuse(), &buf);
  if (h != INVALID_HANDLE_VALUE)
    {
      if (buf.nFileSizeHigh == 0)
	ret = buf.nFileSizeLow;
      FindClose (h);
    }
  return ret;
#else
  struct stat buf;
  if (stat(fname.cstr_oneuse(), &buf))
    throw new runtime_error ("Failed to stat file - has it been deleted?");
  return buf.st_size;
#endif
}
