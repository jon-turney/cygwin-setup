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

static void
get_root_dir_now ()
{
  if (get_root_dir ())
    return;
  read_mounts ();
}

io_stream_cygfile::io_stream_cygfile (const char *name, const char *mode)
{
  fname = NULL;
  fp = NULL;
  errno = 0;
  if (!name || IsBadStringPtr (name, MAX_PATH) || !name[0] ||
      !mode || IsBadStringPtr (mode, 5) || !mode[0])
    return;

  /* do this every time because the mount points may change due to fwd/back button use...
   * TODO: make this less...manual
   */
  get_root_dir_now ();
  if (!get_root_dir ())
    /* TODO: assign a errno for "no mount table :} " */
    return;

  fname = cygpath (name, 0);
  lmode = strdup (mode);
  fp = fopen (fname, mode);
  if (!fp)
    lasterr = errno;
}

io_stream_cygfile::~io_stream_cygfile ()
{
  if (lmode)
    free (lmode);
  if (fname)
    free (fname);
  if (fp)
    fclose (fp);
}

/* Static members */
int
io_stream_cygfile::exists (const char *path)
{
  get_root_dir_now ();
  if (get_root_dir () && _access (cygpath (path, 0), 0) == 0)
    return 1;
  return 0;
}

int
io_stream_cygfile::remove (const char *path)
{
  if (!path)
    return 1;
  get_root_dir_now ();
  if (!get_root_dir ())
    /* TODO: assign a errno for "no mount table :} " */
    return 1;

  unsigned long w = GetFileAttributes (cygpath (path, 0));
  if (w != 0xffffffff && w & FILE_ATTRIBUTE_DIRECTORY)
    {
      char *tmp = (char *) malloc (strlen (cygpath (path, 0)) + 10);
      int i = 0;
      do
	{
	  i++;
	  sprintf (tmp, "%s.old-%d", cygpath (path, 0), i);
	}
      while (GetFileAttributes (tmp) != 0xffffffff);
      fprintf (stderr, "warning: moving directory \"%s\" out of the way.\n",
	       path);
      MoveFile (cygpath (path, 0), tmp);
      free (tmp);
    }
  return !DeleteFileA (cygpath (path, 0));
}

int
io_stream_cygfile::mklink (const char *from, const char *to,
			   io_stream_link_t linktype)
{
  /* FIXME: badstring check */
  if (!from || !to)
    return 1;
  switch (linktype)
    {
    case IO_STREAM_SYMLINK:
      return mkcygsymlink (cygpath (from, 0), to);
    case IO_STREAM_HARDLINK:
      {
	/* For now, just copy */
	/* textmode alert: should we translate when linking from an binmode to a
	   text mode mount and vice verca?
	 */
	io_stream *in = io_stream::open (cygpath (to, 0), "rb");
	if (!in)
	  {
	    log (LOG_TIMESTAMP, "could not open %s for reading in mklink",
		 to);
	    return 1;
	  }
	io_stream *out = io_stream::open (cygpath (from, 0), "wb");
	if (!out)
	  {
	    log (LOG_TIMESTAMP, "could not open %s for writing in mklink",
		 from);
	    delete in;
	    return 1;
	  }

	ssize_t len;
	char buf[16384];
	while ((len = in->read (buf, 16384)) > 0)
	  {
	    ssize_t wrote = out->write (buf, len);
	    if (wrote != len)
	      {
		log (LOG_TIMESTAMP, "error writing to %s in mklink", from);
		delete in;
		delete out;
		return 1;
	      }
	  }
	delete in;
	delete out;
	if (len == 0)
	  return 0;
	log (LOG_TIMESTAMP,
	     "read error reading %s while copying to %s (emulated hardlink)",
	     to, from);
	return 1;
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
  log (LOG_TIMESTAMP, "io_stream_cygfile::peek called");
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
cygmkdir_p (int isadir, const char *name)
{
  if (!name || IsBadStringPtr (name, MAX_PATH) || !name[0])
    return 1;
  get_root_dir_now ();
  if (!get_root_dir ())
    /* TODO: assign a errno for "no mount table :} " */
    return 1;
  return mkdir_p (isadir, cygpath (name, 0));
}

int
io_stream_cygfile::set_mtime (int mtime)
{
  if (!fname)
    return 1;
  if (fp)
    fclose (fp);
  long long ftimev = mtime * NSPERSEC + FACTOR;
  FILETIME ftime;
  ftime.dwHighDateTime = ftimev >> 32;
  ftime.dwLowDateTime = ftimev;
  HANDLE h =
    CreateFileA (fname, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
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
io_stream_cygfile::move (char const *from, char const *to)
{
  if (!from || IsBadStringPtr (from, MAX_PATH) || !from[0] ||
      !to || IsBadStringPtr (to, MAX_PATH) || !to[0])
    return 1;
  get_root_dir_now ();
  if (!get_root_dir ())
    /* TODO: assign a errno for "no mount table :} " */
    return 1;
  return rename (cygpath (from, 0), cygpath (to, 0));
}
