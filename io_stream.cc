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
#include "LogSingleton.h"
#include "port.h"

#include "io_stream.h"
#include "io_stream_file.h"
#include "io_stream_cygfile.h"
#include "mkdir.h"
#include "String++.h"

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
  log (LOG_TIMESTAMP) <<  "io_stream::factory has been called" << endLog;
  return NULL;
}

io_stream *
io_stream::open (String const &name, String const &mode)
{
  if (name.size() < 7 ||
      mode.size() == 0)
    return NULL;
  /* iterate through the known url prefix's */
  if (!name.casecompare ("file://", 7))
    {
      io_stream_file *rv = new io_stream_file (&name.cstr_oneuse()[7], mode.cstr_oneuse());
      if (!rv->error ())
	return rv;
      delete rv;
      return NULL;
    }
  if (!name.casecompare ("cygfile://", 10))
    {
      io_stream_cygfile *rv = new io_stream_cygfile (&name.cstr_oneuse()[10], mode.cstr_oneuse());
      if (!rv->error ())
	return rv;
      delete rv;
      return NULL;
    }
  return NULL;
}

int
io_stream::mkpath_p (path_type_t isadir, String const &name)
{
  if (name.size() < 7)
    return 1;
  /* iterate through the known url prefix's */
  if (!name.casecompare ("file://", 7))
    {
      return mkdir_p (isadir == PATH_TO_DIR ? 1 : 0, &name.cstr_oneuse()[7]);
    }
  if (!name.casecompare ("cygfile://", 10))
    {
      return cygmkdir_p (isadir, &name.cstr_oneuse()[10]);
    }
  return 1;
}

/* remove a file or directory. */
int
io_stream::remove (String const &name)
{
  if (!name.size())
    return 1;
  /* iterate through the known url prefix's */
  if (!name.casecompare ("file://", 7))
    return io_stream_file::remove (&name.cstr_oneuse()[7]);
  if (!name.casecompare ("cygfile://", 10))
    return io_stream_cygfile::remove (&name.cstr_oneuse()[10]);
  return 1;
}

int
io_stream::mklink (String const &from, String const &to,
		   io_stream_link_t linktype)
{
  log (LOG_BABBLE) << "io_stream::mklink (" << from << "->" << to << ")"
    << endLog;
  if (!from.size() || !to.size())
    {
      log (LOG_TIMESTAMP) << "invalid string in from or to parameters to mklink"
	<< endLog;
      return 1;
    }
  /* iterate through the known url prefixes */
  if (!from.casecompare ("file://", 7))
    {
      /* file urls can symlink or hardlink to file url's. */
      /* TODO: allow linking to cygfile url's */
      if (!to.casecompare ("file://", 7))
	return io_stream_file::mklink (&from.cstr_oneuse()[7], &to.cstr_oneuse()[7], linktype);
      log (LOG_TIMESTAMP) << "Attempt to link across url providers" << endLog;
      return 1;
    }
  if (!from.casecompare ("cygfile://", 10))
    {
      /* cygfile urls can symlink or hardlink to cygfile urls's. */
      /* TODO: allow -> file urls */
      if (!to.casecompare ("cygfile://", 10))
	return io_stream_cygfile::mklink (&from.cstr_oneuse()[10], &to.cstr_oneuse()[10], linktype);
      log (LOG_TIMESTAMP) << "Attempt to link across url providers" << endLog;
      return 1;
    }
#if 0
  if (!strmcasecmp ("http://", from, 7))
    {
      /* http urls can symlink to http or ftp url's */
    }
#endif
  log (LOG_TIMESTAMP) << "Unsupported url providers for " << from << endLog;
  return 1;
}

int
io_stream::move_copy (String const &from, String const &to)
{
  /* parameters are ok - checked before calling us, and we are private */
  io_stream *in = io_stream::open (to, "wb");
  io_stream *out = io_stream::open (from, "rb");
  if (io_stream::copy (in, out))
    {
      log (LOG_TIMESTAMP) << "Failed copy of " << from << " to " << to
	<< endLog;
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

ssize_t io_stream::copy (io_stream * in, io_stream * out)
{
  if (!in || !out)
    return -1;
  char
    buffer[16384];
  ssize_t
    countin,
    countout;
  while ((countin = in->read (buffer, 16384)) > 0)
    {
      countout = out->write (buffer, countin);
      if (countout != countin)
	{
	  log (LOG_TIMESTAMP) << "io_stream::copy failed to write "
	    << countin << " bytes" << endLog;
	  return countout ? countout : -1;
	}
    }
  // XXX FIXME: What if countin < 0? return an error

  /* TODO:
     out->set_mtime (in->get_mtime ());
   */
  return 0;
}

int
io_stream::move (String const &from, String const &to)
{
  if (!from.size() || !to.size())
    {
      log (LOG_TIMESTAMP) << "invalid string in from or to parameters to move"
	<< endLog;
      return 1;
    }
  /* iterate through the known url prefixes */
  if (!from.casecompare ("file://", 7))
    {
      /* TODO: allow 'move' to cygfile url's */
      if (to.casecompare ("file://", 7))
	return io_stream::move_copy (from, to);
      return io_stream_file::move (&from.cstr_oneuse()[7], &to.cstr_oneuse()[7]);
    }
  if (!from.casecompare ("cygfile://", 10))
    {
      /* TODO: allow -> file urls */
      if (to.casecompare ("cygfile://", 10))
	return io_stream::move_copy (from, to);
      return io_stream_cygfile::move (&from.cstr_oneuse()[10], &to.cstr_oneuse()[10]);
    }
#if 0
  if (!strmcasecmp ("http://", from, 7))
    {
      /* http urls can symlink to http or ftp url's */
    }
#endif
  log (LOG_TIMESTAMP) << "Unsupported url providers for " << from << endLog;
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
	  --pos; /* end of line, remove from buffer */
	  if (pos > buffer && *(pos - 1) == '\r')
	    --pos;
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
io_stream::exists (String const &name)
{
  if (!name.size())
    return 1;
  /* iterate through the known url prefix's */
  if (!name.casecompare ("file://", 7))
    return io_stream_file::exists (&name.cstr_oneuse()[7]);
  if (!name.casecompare ("cygfile://", 10))
    return io_stream_cygfile::exists (&name.cstr_oneuse()[10]);
  return 1;
}

/* virtual members */

io_stream::~io_stream ()
{
  if (!destroyed)
    log (LOG_TIMESTAMP) << "io_stream::~io_stream: It looks like a class "
      << "hasn't overriden the destructor!" << endLog;
  return;
}
