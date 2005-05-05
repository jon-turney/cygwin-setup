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


#include "io_stream_cygfile.h"

#include "LogSingleton.h"

#include "win32.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "mount.h"
#include "mkdir.h"
#include "mklink2.h"
#include <unistd.h>

#include "IOStreamProvider.h"

/* completely private iostream registration class */
class CygFileProvider : public IOStreamProvider
{
public:
  int exists (String const &path) const
    {return io_stream_cygfile::exists(path);}
  int remove (String const &path) const
    {return io_stream_cygfile::remove(path);}
  int mklink (String const &a , String const &b, io_stream_link_t c) const
    {return io_stream_cygfile::mklink(a,b,c);}
  io_stream *open (String const &a,String const &b) const
    {return new io_stream_cygfile (a, b);}
  ~CygFileProvider (){}
  int move (String const &a,String const &b) const
    {return io_stream_cygfile::move (a, b);}
  int mkdir_p (path_type_t isadir, String const &path) const
    {return cygmkdir_p (isadir, path);}
protected:
  CygFileProvider() // no creating this
    {
      io_stream::registerProvider (theInstance, "cygfile://");
    }
  CygFileProvider(CygFileProvider const &); // no copying
  CygFileProvider &operator=(CygFileProvider const &); // no assignment
private:
  static CygFileProvider theInstance;
};
CygFileProvider CygFileProvider::theInstance = CygFileProvider();


/* For set mtime */
#define FACTOR (0x19db1ded53ea710LL)
#define NSPERSEC 10000000LL

String io_stream_cygfile::cwd("/");
  
// Normalise a unix style path relative to 
// cwd.
String
io_stream_cygfile::normalise (String const &unixpath)
{
  char *path,*tempout;
  
  if (unixpath.c_str()[0]=='/')
    {
      // rooted path
      path = new_cstr_char_array (unixpath);
      tempout = new_cstr_char_array (unixpath); // paths only shrink.
    }
  else
    {
      path = new_cstr_char_array (cwd + unixpath);
      tempout = new_cstr_char_array (cwd + unixpath); //paths only shrink.
    }
  
  // FIXME: handle .. depth tests to prevent / + ../foo/ stepping out
  // of the cygwin tree
  // FIXME: handle /./ sequences
  bool sawslash = false;
  char *outptr = tempout;
  for (char *ptr=path; *ptr; ++ptr)
    {
      if (*ptr == '/' && sawslash)
	--outptr;
      else if (*ptr == '/')
	sawslash=true;
      else
	sawslash=false;
      *outptr++ = *ptr;
    }
  String rv = tempout;
  delete[] path;
  delete[] tempout;
  return rv;
}

static void
get_root_dir_now ()
{
  if (get_root_dir ().size())
    return;
  read_mounts ();
}

io_stream_cygfile::io_stream_cygfile (String const &name, String const &mode) : fp(), fname()
{
  errno = 0;
  if (!name.size() || !mode.size())
  {
    log(LOG_TIMESTAMP) << "io_stream_cygfile: Bad parameters" << endLog;
    return;
  }

  /* do this every time because the mount points may change due to fwd/back button use...
   * TODO: make this less...manual
   */
  get_root_dir_now ();
  if (!get_root_dir ().size())
  {
    /* TODO: assign a errno for "no mount table :} " */
    log(LOG_TIMESTAMP) << "io_stream_cygfile: Error reading mounts" << endLog;
    return;
  }

  fname = cygpath (normalise(name));
  lmode = mode;
  fp = fopen (fname.c_str(), mode.c_str());
  if (!fp)
  {
    lasterr = errno;
    log(LOG_TIMESTAMP) << "io_stream_cygfile: fopen failed " << errno << " "
      << strerror(errno) << endLog;
  }
}

io_stream_cygfile::~io_stream_cygfile ()
{
  if (fp)
    fclose (fp);
}

/* Static members */
int
io_stream_cygfile::exists (String const &path)
{
  get_root_dir_now ();
  if (get_root_dir ().size() && _access (cygpath (normalise(path)).c_str(), 0) == 0)
    return 1;
  return 0;
}

int
io_stream_cygfile::remove (String const &path)
{
  if (!path.size())
    return 1;
  get_root_dir_now ();
  if (!get_root_dir ().size())
    /* TODO: assign a errno for "no mount table :} " */
    return 1;

  unsigned long w = GetFileAttributes (cygpath (normalise(path)).c_str());
  if (w != 0xffffffff && w & FILE_ATTRIBUTE_DIRECTORY)
    {
      char tmp[cygpath (normalise(path)).size() + 10];
      int i = 0;
      do
	{
	  ++i;
	  sprintf (tmp, "%s.old-%d", cygpath (normalise(path)).c_str(), i);
	}
      while (GetFileAttributes (tmp) != 0xffffffff);
      fprintf (stderr, "warning: moving directory \"%s\" out of the way.\n",
	       normalise(path).c_str());
      MoveFile (cygpath (normalise(path)).c_str(), tmp);
    }
  return io_stream::remove (String ("file://") + cygpath (normalise(path)).c_str());
}

int
io_stream_cygfile::mklink (String const &_from, String const &_to,
			   io_stream_link_t linktype)
{
  if (!_from.size() || !_to.size())
    return 1;
  String from(normalise(_from));
  String to (normalise(_to));
  switch (linktype)
    {
    case IO_STREAM_SYMLINK:
      // symlinks are arbitrary targets, can be anything, and are
      // not subject to translation
      return mkcygsymlink (cygpath (from).c_str(), _to.c_str());
    case IO_STREAM_HARDLINK:
      {
	/* For now, just copy */
	/* textmode alert: should we translate when linking from an binmode to a
	   text mode mount and vice verca?
	 */
	io_stream *in = io_stream::open (String ("cygfile://") + to, "rb");
	if (!in)
	  {
	    log (LOG_TIMESTAMP) << "could not open " << to
              << " for reading in mklink" << endLog;
	    return 1;
	  }
	io_stream *out = io_stream::open (String ("cygfile://") + from, "wb");
	if (!out)
	  {
	    log (LOG_TIMESTAMP) << "could not open " << from
              << " for writing in mklink" << endLog;
	    delete in;
	    return 1;
	  }

	if (io_stream::copy (in, out))
	  {
	    log (LOG_TIMESTAMP) << "Failed to hardlink " << from << "->"
              << to << " during file copy." << endLog;
	    delete in;
	    delete out;
	    return 1;
	  }
	delete in;
	delete out;
	return 0;
      }
    }
  return 1;
}


/* virtuals */

ssize_t
io_stream_cygfile::read (void *buffer, size_t len)
{
  if (fp)
    return fread (buffer, 1, len, fp);
  return 0;
}

ssize_t
io_stream_cygfile::write (const void *buffer, size_t len)
{
  if (fp)
    return fwrite (buffer, 1, len, fp);
  return 0;
}

ssize_t
io_stream_cygfile::peek (void *buffer, size_t len)
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
io_stream_cygfile::tell ()
{
  if (fp)
    {
      return ftell (fp);
    }
  return 0;
}

int
io_stream_cygfile::seek (long where, io_stream_seek_t whence)
{
  if (fp)
    {
      return fseek (fp, where, (int) whence);
    }
  lasterr = EBADF;
  return -1;
}

int
io_stream_cygfile::error ()
{
  if (fp)
    return ferror (fp);
  return lasterr;
}

int
cygmkdir_p (path_type_t isadir, String const &_name)
{
  if (!_name.size())
    return 1;
  String name(io_stream_cygfile::normalise(_name));
  get_root_dir_now ();
  if (!get_root_dir ().size())
    /* TODO: assign a errno for "no mount table :} " */
    return 1;
  return mkdir_p (isadir == PATH_TO_DIR ? 1 : 0, cygpath (name).c_str());
}

int
io_stream_cygfile::set_mtime (int mtime)
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
    CreateFileA (fname.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
		 0, OPEN_EXISTING,
		 FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS, 0);
  if (h)
    {
      SetFileTime (h, 0, 0, &ftime);
      CloseHandle (h);
#if 0
      fp = fopen (fname, lmode);
      if (!fp)
	lasterr = errno;
#endif
      return 0;
    }
#if 0
//  this results in truncated files.
//  also, semantically, it's nonsense, you cannot write to a file after setting the 
//  mtime without changing the mtime
  fp = fopen (fname, lmode);
  if (!fp)
    lasterr = errno;
#endif
  return 1;
}

int
io_stream_cygfile::move (String const &_from, String const &_to)
{
  if (!_from.size() || !_to.size())
    return 1;
  String from (normalise(_from));
  String to(normalise(_to));
  get_root_dir_now ();
  if (!get_root_dir ().size())
    /* TODO: assign a errno for "no mount table :} " */
    return 1;
  return rename (cygpath (from).c_str(), cygpath (to).c_str());
}

size_t
io_stream_cygfile::get_size ()
{
  if (!fname.size() )
    return 0;
#ifdef WIN32
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
  /* Should this be lstat? */
  if (stat (fname.c_str(), &buf))
    /* failed - should never happen in this version */
    /* Throw an exception? */
    return 0;
  return buf.off_t;
#endif
}
