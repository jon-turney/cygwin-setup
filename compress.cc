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

/* Archive IO operations
 */

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include "win32.h"
#include <stdio.h>
#include "log.h"

#include "io_stream.h"
#include "compress.h"
#include "zlib/zlib.h"
#include "compress_gz.h"
#include "compress_bz.h"

/* In case you are wondering why the file magic is not in one place:
 * It could be. But there is little (any?) benefit.
 * What is important is that the file magic required for any _task_ is centralised.
 * One such task is identifying compresss
 *
 * to federate into each class one might add a magic parameter to the constructor, which
 * the class could test itself. 
 */

#define longest_magic 3

io_stream *
compress::decompress (io_stream * original)
{
  if (!original)
    return NULL;
  char magic[longest_magic];
  if (original->peek (magic, longest_magic) > 0)
    {
      if (memcmp (magic, "\037\213", 2) == 0)
	{
	  /* tar */
	  compress_gz *rv = new compress_gz (original);
	  if (!rv->error ())
	    return rv;
	  return NULL;
	}
      else if (memcmp (magic, "BZh", 3) == 0)
	{
	  compress_bz *rv = new compress_bz (original);
	  if (!rv->error ())
	    return rv;
	  return NULL;
	}
    }
  return NULL;
}

ssize_t
compress::read (void *buffer, size_t len)
{
  log (LOG_TIMESTAMP, "compress::read called");
  return 0;
}

ssize_t
compress::write (void *buffer, size_t len)
{
  log (LOG_TIMESTAMP, "compress::write called");
  return 0;
}

ssize_t
compress::peek (void *buffer, size_t len)
{
  log (LOG_TIMESTAMP, "compress::peek called");
  return 0;
}

long
compress::tell ()
{
  log (LOG_TIMESTAMP, "bz::tell called");
  return 0;
}

int
compress::error ()
{
  log (LOG_TIMESTAMP, "compress::error called");
  return 0;
}

const char *
compress::next_file_name ()
{
  log (LOG_TIMESTAMP, "compress::next_file_name called");
  return NULL;
}

compress::~compress ()
{
  log (LOG_BABBLE, "compress::~compress called");
  return;
}
