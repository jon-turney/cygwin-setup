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

//#include "win32.h"
//#include <stdio.h>
//#include <stdlib.h>
#include "LogSingleton.h"
//#include "port.h"
#include "String++.h"

#include "io_stream.h"
#include "archive.h"
#include "archive_tar.h"

/* In case you are wondering why the file magic is not in one place:
 * It could be. But there is little (any?) benefit.
 * What is important is that the file magic required for any _task_ is centralised.
 * One such task is identifying archives
 *
 * to federate into each class one might add a magic parameter to the constructor, which
 * the class could test itself. 
 */

/* GNU TAR:
 * offset 257     string  ustar\040\040\0
 */


#define longest_magic 265

archive *
archive::extract (io_stream * original)
{
  if (!original)
    return NULL;
  char magic[longest_magic];
  if (original->peek (magic, longest_magic) > 0)
    {
      if (memcmp (&magic[257], "ustar\040\040\0", 8) == 0)
	{
	  /* tar */
	  archive_tar *rv = new archive_tar (original);
	  if (!rv->error ())
	    return rv;
	  return NULL;
	}
#if 0
      else if (memcmp (magic, "BZh", 3) == 0)
	{
	  archive_bz *rv = new archive_bz (original);
	  if (!rv->error ())
	    return rv;
	  return NULL;
	}
#endif
    }
  return NULL;
}

int
archive::extract_file (archive * source, String const &prefixURL, String const &prefixPath, String suffix)
{
  if (!source)
    return 1;
  String const destfilename = prefixURL+prefixPath+ source->next_file_name ()+ suffix;
  switch (source->next_file_type ())
    {
    case ARCHIVE_FILE_REGULAR:
      {

	/* TODO: remove in-the-way directories via mkpath_p */
	if (io_stream::mkpath_p (PATH_TO_FILE, destfilename))
	{
	  log (LOG_TIMESTAMP) << "Failed to make the path for " << destfilename
	  		      << endLog;
	  return 1;
	}
	io_stream::remove (destfilename);
	io_stream *tmp = io_stream::open (destfilename, "wb");
	if (!tmp)
	{
	  log (LOG_TIMESTAMP) << "Failed to open " << destfilename;
	  log (LOG_TIMESTAMP) << " for writing." << endLog;
	  return 1;
	}
	io_stream *in = source->extract_file ();
	if (!in)
	  {
	    delete tmp;
	    log (LOG_TIMESTAMP) << "Failed to extract the file "
	     			<< destfilename << " from the archive" 
				<< endLog;
	    return 1;
	  }
	if (io_stream::copy (in, tmp))
	  {
	    log (LOG_TIMESTAMP) << "Failed to output " << destfilename
	      			<< endLog;
	    delete tmp;
	    io_stream::remove (destfilename);
	    return 1;
	  }
	tmp->set_mtime (in->get_mtime ());
	delete in;
	delete tmp;
      }
      break;
    case ARCHIVE_FILE_SYMLINK:
      {
	if (io_stream::mkpath_p (PATH_TO_FILE, destfilename))
	{
	  log (LOG_TIMESTAMP) << "Failed to make the path for %s" 
	    		      << destfilename << endLog;
	  return 1;}
	io_stream::remove (destfilename);
	int ok =
	  io_stream::mklink (destfilename,
	      prefixURL+ source->linktarget (),
			     IO_STREAM_SYMLINK);
	/* FIXME: check what tar's filelength is set to for symlinks */
	source->skip_file ();
	return ok;
      }
    case ARCHIVE_FILE_HARDLINK:
      {
	if (io_stream::mkpath_p (PATH_TO_FILE, destfilename))
	{
	  log (LOG_TIMESTAMP) << "Failed to make the path for %s"
	    		      << destfilename << endLog;
	  return 1;
	}
	io_stream::remove (destfilename);
	int ok =
	  io_stream::mklink (destfilename,
	      prefixURL + source->linktarget (),
			     IO_STREAM_HARDLINK);
	/* FIXME: check what tar's filelength is set to for hardlinks */
	source->skip_file ();
	return ok;
      }
    case ARCHIVE_FILE_DIRECTORY:
      {
	char *path = (char *) alloca (destfilename.size());
	strcpy (path, destfilename.cstr_oneuse());
	while (path[0] && path[strlen (path) - 1] == '/')
	  path[strlen (path) - 1] = 0;
	source->skip_file ();
	return io_stream::mkpath_p (PATH_TO_DIR, path);
      }
    case ARCHIVE_FILE_INVALID:
      source->skip_file ();
      break;
    }
  return 0;
}

#if 0
ssize_t archive::read (void *buffer, size_t len)
{
  log (LOG_TIMESTAMP, "archive::read called");
  return 0;
}

ssize_t archive::write (void *buffer, size_t len)
{
  log (LOG_TIMESTAMP, "archive::write called");
  return 0;
}

ssize_t archive::peek (void *buffer, size_t len)
{
  log (LOG_TIMESTAMP, "archive::peek called");
  return 0;
}

long
archive::tell ()
{
  log (LOG_TIMESTAMP, "bz::tell called");
  return 0;
}

int
archive::error ()
{
  log (LOG_TIMESTAMP, "archive::error called");
  return 0;
}

const char *
archive::next_file_name ()
{
  log (LOG_TIMESTAMP, "archive::next_file_name called");
  return NULL;
}

archive::~archive ()
{
  log (LOG_TIMESTAMP, "archive::~archive called");
  return;
}
#endif
