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

/* Archive IO operations for bz2 files.
   derived from the fd convenience functions in the libbz2 package.
 */

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include <algorithm>
#include "win32.h"
#include <stdio.h>
#include <errno.h>
#include "log.h"
#include "port.h"

#include "io_stream.h"
#include "compress.h"
#include "compress_bz.h"
#undef _WIN32
#include "bzlib.h"

compress_bz::compress_bz (io_stream * parent) : peeklen (0), position (0)
{
  /* read only via this constructor */
  original = 0;
  lasterr = 0;
  if (!parent || parent->error ())
    {
      lasterr = EBADF;
      return;
    }
  original = parent;

  initialisedOk = 0;
  bufN = 0;
  writing = 0;
  strm.bzalloc = 0;
  strm.bzfree = 0;
  strm.opaque = 0;
  int ret = BZ2_bzDecompressInit (&(strm), 0, 0);
  if (ret)
    {
      lasterr = ret;
      return;
    }
  strm.avail_in = 0;
  strm.next_in = 0;
  initialisedOk = 1;
}

ssize_t
compress_bz::read (void *buffer, size_t len)
{
  if (!initialisedOk || writing)
    return EBADF;
  if (len == 0)
    return 0;

  if (peeklen)
  {
    ssize_t tmplen = std::min (peeklen, len);
    peeklen -= tmplen;
    memcpy (buffer, peekbuf, tmplen);
    memmove (peekbuf, peekbuf + tmplen, tmplen);
    ssize_t tmpread = read (&((char *) buffer)[tmplen], len - tmplen);
    if (tmpread >= 0)
        return tmpread + tmplen;
    else
        return tmpread;
  }
  
  strm.avail_out = len;
  strm.next_out = (char *) buffer;
  int
    rlen = 1;
  while (1)
    {
      if (original->error ())
	{
	  lasterr = original->error ();
	  return -1;
	}
      if (strm.avail_in == 0 && rlen > 0)
	{
	  rlen = original->read (buf, 4096);
	  if (rlen < 0)
	    {
	      lasterr = rlen;
	      return -1;
	    }
	  bufN = rlen;
	  strm.avail_in = rlen;
	  strm.next_in = buf;
	}
      int
	ret = BZ2_bzDecompress (&strm);
      if (ret != BZ_OK && ret != BZ_STREAM_END)
	{
	  lasterr = ret;
	  return -1;
	}
      if (ret == BZ_OK && rlen == 0 && strm.avail_out)
	{
	  /* unexpected end of file */
	  lasterr = EIO;
	  return -1;
	}
      if (ret == BZ_STREAM_END)
	{
	  position += len - strm.avail_out;
	  return len - strm.avail_out;
	}
      if (strm.avail_out == 0)
	{
	  position += len;
	  return len;
	}
    }

  /* not reached */
  return 0;
}

ssize_t compress_bz::write (const void *buffer, size_t len)
{
  log (LOG_TIMESTAMP, "compress_bz::write called");
  return 0;
}

ssize_t compress_bz::peek (void *buffer, size_t len)
{
  if (writing)
  {
    lasterr = EBADF;
      return -1;
  }
  /* can only peek 512 bytes */
  if (len > 512)
      return ENOMEM;

  if (len > peeklen)
      {
	    size_t want = len - peeklen;
	        ssize_t got = read (&peekbuf[peeklen], want);
		    if (got >= 0)
		            peeklen += got;
		        else
			        /* error */
			        return got;
			    /* we may have read less than requested. */
			    memcpy (buffer, peekbuf, peeklen);
			        return peeklen;
				  }
  else
      {
	    memcpy (buffer, peekbuf, len);
	        return len;
		  }
  return 0;
}

long
compress_bz::tell ()
{
  if (writing)
    {
      log (LOG_TIMESTAMP, "compress_bz::tell called for writing mode");
      return 0;
    }
  return position;
}

int
compress_bz::seek (long where, io_stream_seek_t whence)
{
    log (LOG_TIMESTAMP, "compress_bz::seek called");
    return -1;
}

int
compress_bz::error ()
{
  log (LOG_TIMESTAMP, "compress_bz::error called");
  return 0;
}

int
compress_bz::set_mtime (int time)
{
  if (original)
    return original->set_mtime (time);
  return 1;
}

int
compress_bz::get_mtime ()
{
  if (original)
    return original->get_mtime ();
  return 0;
}

compress_bz::~compress_bz ()
{
  if (initialisedOk)
    BZ2_bzDecompressEnd (&strm);
  destroyed = 1;
  return;
}
