/*
 * Copyright (c) 2000, 2001, Red Hat, Inc.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by DJ Delorie <dj@cygnus.com>
 *
 */

/* Built-in tar functionality.  See tar.h for usage. */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <errno.h>

#include "io_stream.h"
#include "win32.h"
#include "archive.h"
#include "archive_tar.h"
#include "mkdir.h"
#include "LogFile.h"
#include "filemanip.h"

static int err;
static char buf[512];

int _tar_verbose = 0;

archive_tar::archive_tar (io_stream * original)
{
  archive_children = 0;

  if (_tar_verbose)
    LogBabblePrintf("tar: open `%p'\n", original);

  if (!original)
    {
      state.lasterr = EBADF;
      return;
    }
  state.parent = original;

  if (sizeof (state.tar_header) != 512)
    {
      /* drastic, but important */
      Log (LOG_TIMESTAMP) << "compilation error: tar header struct not 512"
			  << " bytes (it's " << sizeof (state.tar_header)
			  << ")" << endLog;
      Logger ().exit (1);
    }
}

ssize_t
archive_tar::read (void *buffer, size_t len)
{
  return -1;
}

ssize_t
archive_tar::write (const void *buffer, size_t len)
{
  return 0;
}

ssize_t
archive_tar::peek (void *buffer, size_t len)
{
  return 0;
}

int
archive_tar::error ()
{
  return state.lasterr;
}

off_t
archive_tar::tell ()
{
  return state.file_offset;
}

off_t
archive_tar::seek (off_t where, io_stream_seek_t whence)
{
  /* Because the parent stream is compressed, we can only easily support
     seek()-ing to rewind to the start */
  if ((whence == IO_SEEK_SET) && (where == 0))
    {
      state.header_read = 0;
      return state.parent->seek(where, whence);
    }

  return -1;
}

int
archive_tar::skip_file ()
{
  while (state.file_length > state.file_offset)
    {
      int len = state.parent->read (buf, 512);
      state.file_offset += 512;
      if (len != 512)
	return 1;
    }
  state.file_length = 0;
  state.file_offset = 0;
  state.header_read = 0;
  return 0;
}

const std::string
archive_tar::next_file_name ()
{
  char *c;

  if (state.header_read)
    {
      if (strlen (state.filename))
        return state.filename;
      else
        /* End of tar */
        return std::string();
    }

  int r = state.parent->read (&state.tar_header, 512);

  /* See if we're at end of file */
  if (r != 512)
    return std::string();

  /* See if the header is all zeros (i.e. last block) */
  int n = 0;
  for (r = 512 / sizeof (int); r; r--)
    n |= ((int *) &state.tar_header)[r - 1];
  if (n == 0)
    return std::string();

  if (!state.have_longname)
    {
      memcpy (state.filename, state.tar_header.name, 100);
      state.filename[100] = 0;
    }

  if (!state.have_longlink)
    {
      memcpy (state.linkname, state.tar_header.linkname, 100);
      state.linkname[100] = 0;
    }

  /* typeflag for any 'real' file consumes the longname/longlink state from
     previous blocks */
  if ((state.tar_header.typeflag != 'K') &&
      (state.tar_header.typeflag != 'L'))
    {
      state.have_longname = 0;
      state.have_longlink = 0;
    }

  state.file_length = strtoll (state.tar_header.size, NULL, 8);
  state.file_offset = 0;

  if (_tar_verbose)
    LogBabblePrintf ("%c %9d %s\n", state.tar_header.typeflag,
                     state.file_length, state.filename);

  switch (state.tar_header.typeflag)
    {
    case 'K':			/* GNU tar long link extension */
    case 'L':			/* GNU tar long name extension */
      if (strcmp(state.tar_header.name, "././@LongLink") != 0)
        LogBabblePrintf("tar: unexpected filename %s in file type %c header\n", state.tar_header.name, state.tar_header.typeflag);

      /* we read the 'file' into the long filename/linkname, then recursively
       * call ourselves to handle the following block.
       */
      if (state.file_length > CYG_PATH_MAX)
	{
	  skip_file ();
	  LogPlainPrintf( "error: long file name exceeds %d characters\n",
                          CYG_PATH_MAX);
	  err++;
	  state.parent->read (&state.tar_header, 512);
	  state.file_length = strtoll (state.tar_header.size, NULL, 8);
	  state.file_offset = 0;
	  skip_file ();
	  return next_file_name ();
	}

      if (state.tar_header.typeflag == 'L')
        {
          c = state.filename;
          state.have_longname = 1;
        }
      else
        {
          c = state.linkname;
          state.have_longlink = 1;
        }

      /* FIXME: this should be a single read() call */
      while (state.file_length > state.file_offset)
	{
	  int need =
	    state.file_length - state.file_offset >
	    512 ? 512 : state.file_length - state.file_offset;
	  if (state.parent->read (buf, 512) < 512)
            {
              LogPlainPrintf( "error: error reading long name\n");
              return "";
            }
	  memcpy (c, buf, need);
	  c += need;
	  state.file_offset += need;
	}
      *c = 0;

      return next_file_name ();

    case '3':			/* char */
    case '4':			/* block */
    case '6':			/* fifo */
      LogPlainPrintf ("warning: not extracting special file %s\n",
                      state.filename);
      err++;
      return next_file_name ();

    case '0':			/* regular file */
    case 0:			/* regular file also */
    case '2':			/* symbolic link */
    case '5':			/* directory */
    case '7':			/* contiguous file */
      state.header_read = 1;
      return state.filename;

    case '1':			/* hard link, we just copy */
      state.header_read = 1;
      return state.filename;

    default:
      LogPlainPrintf ("error: unknown (or unsupported) file type `%c'\n",
                      state.tar_header.typeflag);
      err++;
      /* fall through */
    case 'g':			/* POSIX.1-2001 global extended header */
    case 'x':			/* POSIX.1-2001 extended header */
      skip_file ();
      return next_file_name ();
    }
  return std::string();
}

archive_tar::~archive_tar ()
{
  if (state.parent)
    delete state.parent;
}

# if 0
static void
fix_time_stamp (char *path)
{
  int mtime;
  long long ftimev;
  FILETIME ftime;
  HANDLE h;

  sscanf (tar_header.mtime, "%o", &mtime);
  ftimev = mtime * NSPERSEC + FACTOR;
  ftime.dwHighDateTime = ftimev >> 32;
  ftime.dwLowDateTime = ftimev;
  h = CreateFileA (path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
		   0, OPEN_EXISTING,
		   FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS, 0);
  if (h)
    {
      SetFileTime (h, 0, 0, &ftime);
      CloseHandle (h);
    }
}
#endif
archive_file_t
archive_tar::next_file_type ()
{
  switch (state.tar_header.typeflag)
    {
      /* regular files */
    case '0':
    case 0:
    case '7':
      return ARCHIVE_FILE_REGULAR;
    case '1':
      return ARCHIVE_FILE_HARDLINK;
    case '5':
      return ARCHIVE_FILE_DIRECTORY;
    case '2':
      return ARCHIVE_FILE_SYMLINK;
    default:
      return ARCHIVE_FILE_INVALID;
    }
}

const std::string
archive_tar::linktarget ()
{
  /* TODO: consider .. path traversal issues */
  if (next_file_type () == ARCHIVE_FILE_SYMLINK ||
      next_file_type () == ARCHIVE_FILE_HARDLINK)
    return state.linkname;
  return std::string();
}

io_stream *
archive_tar::extract_file ()
{
  if (archive_children)
    return NULL;
  archive_tar_file *rv = new archive_tar_file (state);
  return rv;
}

time_t
archive_tar::get_mtime ()
{
  if (state.parent)
    return state.parent->get_mtime ();
  return 0;
}

mode_t
archive_tar::get_mode ()
{
  if (state.parent)
    return state.parent->get_mode ();
  return 0;
}
