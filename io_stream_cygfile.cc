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
#include "log.h"
#include "port.h"
#include "mount.h"
#include "mkdir.h"
#include "mklink2.h"
#include <unistd.h>

#include "io_stream.h"
#include "io_stream_cygfile.h"

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
  
  if (unixpath.cstr_oneuse()[0]=='/')
    {
      // rooted path
      path = unixpath.cstr();
      tempout = unixpath.cstr(); // paths only shrink.
    }
  else
    {
      path = (cwd + unixpath).cstr();
      tempout = (cwd + unixpath).cstr(); //paths only shrink.
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
    return;

  /* do this every time because the mount points may change due to fwd/back button use...
   * TODO: make this less...manual
   */
  get_root_dir_now ();
  if (!get_root_dir ().size())
    /* TODO: assign a errno for "no mount table :} " */
    return;

  fname = cygpath (normalise(name));
  lmode = mode;
  fp = fopen (fname.cstr_oneuse(), mode.cstr_oneuse());
  if (!fp)
    lasterr = errno;
}

io_stream_cygfile::~io_stream_cygfile ()
{
  if (fp)
    fclose (fp);
  destroyed = 1;
}

/* Static members */
int
io_stream_cygfile::exists (String const &path)
{
  get_root_dir_now ();
  if (get_root_dir ().size() && _access (cygpath (normalise(path)).cstr_oneuse(), 0) == 0)
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

  unsigned long w = GetFileAttributes (cygpath (normalise(path)).cstr_oneuse());
  if (w != 0xffffffff && w & FILE_ATTRIBUTE_DIRECTORY)
    {
      char tmp[cygpath (normalise(path)).size() + 10];
      int i = 0;
      do
	{
	  ++i;
	  sprintf (tmp, "%s.old-%d", cygpath (normalise(path)).cstr_oneuse(), i);
	}
      while (GetFileAttributes (tmp) != 0xffffffff);
      fprintf (stderr, "warning: moving directory \"%s\" out of the way.\n",
	       normalise(path).cstr_oneuse());
      MoveFile (cygpath (normalise(path)).cstr_oneuse(), tmp);
    }
  return !DeleteFileA (cygpath (normalise(path)).cstr_oneuse());
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
      return mkcygsymlink (cygpath (from).cstr_oneuse(), _to.cstr_oneuse());
    case IO_STREAM_HARDLINK:
      {
	/* For now, just copy */
	/* textmode alert: should we translate when linking from an binmode to a
	   text mode mount and vice verca?
	 */
	io_stream *in = io_stream::open (String ("cygfile://") + to, "rb");
	if (!in)
	  {
	    log (LOG_TIMESTAMP, String("could not open ") + to +" for reading in mklink");
	    return 1;
	  }
	io_stream *out = io_stream::open (String ("cygfile://") + from, "wb");
	if (!out)
	  {
	    log (LOG_TIMESTAMP, String("could not open ") + from + " for writing in mklink");
	    delete in;
	    return 1;
	  }

	if (io_stream::copy (in, out))
	  {
	    log (LOG_TIMESTAMP, String ("Failed to hardlink ")+ from + "->"  +to +
		" during file copy.");
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
cygmkdir_p (enum path_type_t isadir, String const &_name)
{
  if (!_name.size())
    return 1;
  String name(io_stream_cygfile::normalise(_name));
  get_root_dir_now ();
  if (!get_root_dir ().size())
    /* TODO: assign a errno for "no mount table :} " */
    return 1;
  return mkdir_p (isadir == PATH_TO_DIR ? 1 : 0, cygpath (name).cstr_oneuse());
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
    CreateFileA (fname.cstr_oneuse(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
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
  return rename (cygpath (from).cstr_oneuse(), cygpath (to).cstr_oneuse());
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
  /* Should this be lstat? */
  if (stat (fname.cstr_oneuse(), &buf))
    /* failed - should never happen in this version */
    /* Throw an exception? */
    return 0;
  return buf.off_t;
#endif
}
