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

/* this is the parent class for all IO operations. It's flexable enough to be cover for
 * HTTP access, local file access, and files being extracted from archives.
 * It also encapsulates the idea of an archive, and all non-archives become the special 
 * case.
 */

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include "win32.h"
#include <stdio.h>
#include "log.h"
#include "port.h"

#include "io_stream.h"
#include "io_stream_file.h"
#include "io_stream_cygfile.h"
#include "mkdir.h"

/* Static members */
io_stream *
io_stream::factory (io_stream * parent)
{
  /* something like,  
   * if !next_file_name 
   *   return NULL
   * switch (magic_id(peek (parent), max_magic_length))
   * case io_stream * foo = new tar
   * case io_stream * foo = new bz2
   * return foo
   */
  log (LOG_TIMESTAMP, "io_stream::factory has been called");
  return NULL;
}

io_stream *
io_stream::open (const char *name, const char *mode)
{
  if (!name || IsBadStringPtr (name, MAX_PATH) || !name[0] ||
      !mode || IsBadStringPtr (mode, 5) || !mode[0])
    return NULL;
  /* iterate through the known url prefix's */
  if (!strncasecmp ("file://", name, 7))
    {
      io_stream_file *rv = new io_stream_file (&name[7], mode);
      if (!rv->error ())
	return rv;
      delete rv;
      return NULL;
    }
  if (!strncasecmp ("cygfile://", name, 10))
    {
      io_stream_cygfile *rv = new io_stream_cygfile (&name[10], mode);
      if (!rv->error ())
	return rv;
      delete rv;
      return NULL;
    }
  return NULL;
}

int
io_stream::mkpath_p (path_type_t isadir, const char *name)
{
  if (!name || IsBadStringPtr (name, MAX_PATH) || !name[0])
    return 1;
  /* iterate through the known url prefix's */
  if (!strncasecmp ("file://", name, 7))
    {
      return mkdir_p (isadir == PATH_TO_DIR ? 1 : 0, &name[7]);
    }
  if (!strncasecmp ("cygfile://", name, 10))
    {
      return cygmkdir_p (isadir == PATH_TO_DIR ? 1 : 0, &name[10]);
    }
  return 1;
}

/* remove a file or directory. */
int
io_stream::remove (const char *name)
{
  if (!name || IsBadStringPtr (name, MAX_PATH) || !name[0])
    return 1;
  /* iterate through the known url prefix's */
  if (!strncasecmp ("file://", name, 7))
    return io_stream_file::remove (&name[7]);
  if (!strncasecmp ("cygfile://", name, 10))
    return io_stream_cygfile::remove (&name[10]);
  return 1;
}

int
io_stream::mklink (const char *from, const char *to,
		   io_stream_link_t linktype)
{
  if (!from || IsBadStringPtr (from, MAX_PATH) ||
      !to || IsBadStringPtr (to, MAX_PATH))
    {
      log (LOG_TIMESTAMP,
	   "invalid string in from or to parameters to mklink");
      return 1;
    }
  /* iterate through the known url prefixes */
  if (!strncasecmp ("file://", from, 7))
    {
      /* file urls can symlink or hardlink to file url's. */
      /* TODO: allow linking to cygfile url's */
      if (!strncasecmp ("file://", to, 7))
	return io_stream_file::mklink (&from[7], &to[7], linktype);
      log (LOG_TIMESTAMP, "Attempt to link across url providers");
      return 1;
    }
  if (!strncasecmp ("cygfile://", from, 10))
    {
      /* cygfile urls can symlink or hardlink to cygfile urls's. */
      /* TODO: allow -> file urls */
      if (!strncasecmp ("cygfile://", to, 10))
	return io_stream_cygfile::mklink (&from[10], &to[10], linktype);
      log (LOG_TIMESTAMP, "Attempt to link across url providers");
      return 1;
    }
#if 0
  if (!strmcasecmp ("http://", from, 7))
    {
      /* http urls can symlink to http or ftp url's */
    }
#endif
  log (LOG_TIMESTAMP, "Unsupported url providers for %s", from);
  return 1;
}

int
io_stream::move_copy (const char *from, const char *to)
{
  /* parameters are ok - checked before calling us, and we are private */
  io_stream *in = io_stream::open (to, "wb");
  io_stream *out = io_stream::open (from, "rb");
  if (io_stream::copy (in, out))
    {
      log (LOG_TIMESTAMP, "Failed copy of %s to %s", from, to);
      delete out;
      io_stream::remove (to);
      delete in;
      return 1;
    }
  /* TODO:
     out->set_mtime (in->get_mtime ());
   */
  delete in;
  delete out;
  io_stream::remove (from);
  return 0;
}

ssize_t
io_stream::copy (io_stream *in, io_stream *out)
{
  if (!in || !out)
    return -1;
  char buffer[16384];
  ssize_t countin, countout;
  while ((countin = in->read (buffer, 16384)) > 0)
    {
      countout = out->write (buffer, countin);
      if (countout != countin)
        {
          log (LOG_TIMESTAMP, "io_stream::copy failed to write %ld bytes", countin);
          return countout ? countout : -1;
        }
    }

  /* TODO:
     out->set_mtime (in->get_mtime ());
   */
  return 0;
}

int
io_stream::move (const char *from, const char *to)
{
  if (!from || IsBadStringPtr (from, MAX_PATH) ||
      !to || IsBadStringPtr (to, MAX_PATH))
    {
      log (LOG_TIMESTAMP, "invalid string in from or to parameters to move");
      return 1;
    }
  /* iterate through the known url prefixes */
  if (!strncasecmp ("file://", from, 7))
    {
      /* TODO: allow 'move' to cygfile url's */
      if (strncasecmp ("file://", to, 7))
	return io_stream::move_copy (from, to);
      return io_stream_file::move (&from[7], &to[7]);
    }
  if (!strncasecmp ("cygfile://", from, 10))
    {
      /* TODO: allow -> file urls */
      if (strncasecmp ("cygfile://", to, 10))
	return io_stream::move_copy (from, to);
      return io_stream_cygfile::move (&from[10], &to[10]);
    }
#if 0
  if (!strmcasecmp ("http://", from, 7))
    {
      /* http urls can symlink to http or ftp url's */
    }
#endif
  log (LOG_TIMESTAMP, "Unsupported url providers for %s", from);
  return 1;
}

char *
io_stream::gets (char *buffer, size_t length)
{
  char *pos = buffer;
  size_t count = 0;
  while (count + 1 < length && read (pos, 1) == 1)
    {
      count++;
      pos++;
      if (*(pos - 1) == '\n')
	{
	  /* end of line */
	  /* TODO: remove the \r if it is present depending on the 
	   * file's mode 
	   */
	  break;
	}
    }
  if (count == 0 || error ())
    /* EOF when no chars found, or an error */
    return NULL;
  *pos = '\0';
  return buffer;
}

int
io_stream::exists (const char *name)
{
  if (!name || IsBadStringPtr (name, MAX_PATH) || !name[0])
    return 1;
  /* iterate through the known url prefix's */
  if (!strncasecmp ("file://", name, 7))
    return io_stream_file::exists (&name[7]);
  if (!strncasecmp ("cygfile://", name, 10))
    return io_stream_cygfile::exists (&name[10]);
  return 1;
}

/* virtual members */

io_stream::~io_stream ()
{
  log (LOG_TIMESTAMP, "io_stream::~io_stream called");
  return;
}
