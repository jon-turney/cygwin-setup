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

/* The purpose of this file is to act as a pretty interface to netio.cc.  We add
   a progress reporting and some convenience functions (like collect to string
   or file) */


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "dialog.h"
#include "geturl.h"
#include "resource.h"
#include "netio.h"
#include "io_stream.h"
#include "io_stream_memory.h"
#include "state.h"
#include "filemanip.h"
#include "String++.h"

#include "Exception.h"

#include "LogSingleton.h"
#include "Feedback.h"

static void
getUrlToStream (const std::string &_url, io_stream *output, Feedback &feedback)
{
  // we turn off this feedback for local files
  feedback.fetch_progress_disable((source == IDC_SOURCE_LOCALDIR));

  feedback.fetch_init (_url, 0);
  NetIO *n = NetIO::open (_url.c_str(), true);
  if (!n || !n->ok ())
    {
      delete n;
      throw new Exception (TOSTRING(__LINE__) " " __FILE__, "Error opening url",  APPERR_IO_ERROR);
    }

  if (n->file_size)
    feedback.fetch_set_length(n->file_size);

  int total_bytes = 0;
  feedback.fetch_progress (0);
  while (1)
    {
      char buf[2048];
      ssize_t rlen, wlen;
      rlen = n->read (buf, 2048);
      if (rlen > 0)
	{
	  wlen = output->write (buf, rlen);
	  if (wlen != rlen)
	    /* FIXME: Show an error message */
	    break;
	  total_bytes += rlen;
	  feedback.fetch_progress (total_bytes);
	}
      else
	break;
    }
  if (n)
    delete (n);
  /* reseeking is up to the recipient if desired */

  Log (LOG_BABBLE) << "Fetched URL: " << _url << endLog;
}

io_stream *
get_url_to_membuf (const std::string &_url, Feedback &feedback)
{
  io_stream_memory *membuf = new io_stream_memory ();
  try
    {
      getUrlToStream (_url, membuf, feedback);

      if (membuf->seek (0, IO_SEEK_SET))
    	{
    	  if (membuf)
      	      delete membuf;
    	  Log (LOG_BABBLE) << "get_url_to_membuf(): seek (0) failed for membuf!" << endLog;
    	  return 0;
	}
      return membuf;
    }
  catch (Exception *e)
    {
      if (e->errNo() != APPERR_IO_ERROR)
	throw e;
      delete membuf;
      return 0;
    }
}

// predicate: url has no '\0''s in it.
std::string
get_url_to_string (const std::string &_url, Feedback &feedback)
{
  io_stream *stream = get_url_to_membuf (_url, feedback);
  if (!stream)
    return std::string();
  size_t bytes = stream->get_size ();
  if (!bytes)
    {
      /* zero length, or error retrieving length */
      delete stream;
      Log (LOG_BABBLE) << "get_url_to_string(): couldn't retrieve buffer size, or zero length buffer" << endLog;
      return std::string();
    }
  char temp [bytes + 1];
  /* membufs are quite safe */
  stream->read (temp, bytes);
  temp [bytes] = '\0';
  delete stream;
  return std::string(temp);
}

int
get_url_to_file (const std::string &_url,
                 const std::string &_filename,
                 int expected_length,
                 Feedback &feedback)
{
  Log (LOG_BABBLE) << "get_url_to_file " << _url << " " << _filename << endLog;
  feedback.fetch_total_progress();
  feedback.fetch_init(_url, expected_length);

  remove (_filename.c_str());		/* but ignore errors */

  NetIO *n = NetIO::open (_url.c_str(), false);
  if (!n || !n->ok ())
    {
      delete n;
      return 1;
    }

  FILE *f = nt_fopen (_filename.c_str(), "wb");
  if (!f)
    {
      const char *err = strerror (errno);
      if (!err)
	err = "(unknown error)";
      feedback.fetch_fatal (_filename.c_str(), err);
    }

  if (n->file_size)
    feedback.fetch_set_length(n->file_size);

  int total_bytes = 0;
  feedback.fetch_progress(0);
  while (1)
    {
      char buf[8192];
      int count;
      count = n->read (buf, sizeof (buf));
      if (count <= 0)
	break;
      fwrite (buf, 1, count, f);
      total_bytes += count;
      feedback.fetch_progress (total_bytes);
    }

  fclose (f);
  if (n)
    delete n;

  feedback.fetch_total_progress();
  feedback.fetch_finish(total_bytes);

  return 0;
}
