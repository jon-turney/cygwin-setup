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

/* The purpose of this file is to act as a pretty interface to
   netio.cc.  We add a progress dialog and some convenience functions
   (like collect to string or file */

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include "win32.h"
#include "commctrl.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "dialog.h"
#include "geturl.h"
#include "resource.h"
#include "netio.h"
#include "msg.h"
#include "io_stream.h"
#include "io_stream_memory.h"
#include "state.h"
#include "diskfull.h"
#include "mount.h"

#include "threebar.h"

#include "String++.h"  

#include "Exception.h"

#include "LogSingleton.h"

extern ThreeBarProgressPage Progress;

static int max_bytes = 0;
static int is_local_install = 0;

int total_download_bytes = 0;
int total_download_bytes_sofar = 0;

static DWORD start_tics;

static void
init_dialog (String const &url, int length, HWND owner)
{
  if (is_local_install)
    return;

  char const *temp = url.cstr();
  char const *sl = temp;
  char const *cp;
  for (cp = sl; *cp; cp++)
    if (*cp == '/' || *cp == '\\' || *cp == ':')
      sl = cp + 1;
  max_bytes = length;
  Progress.SetText1("Downloading...");
  Progress.SetText2(sl);
  Progress.SetText3("Connecting...");
  Progress.SetBar1(0);
  start_tics = GetTickCount ();
  delete[] temp;
}


static void
progress (int bytes)
{
  if (is_local_install)
    return;
  static char buf[100];
  double kbps;
  static unsigned int last_tics = 0;
  DWORD tics = GetTickCount ();
  if (tics == start_tics)	// to prevent division by zero
    return;
  if (tics < last_tics + 200)	// to prevent flickering updates
    return;
  last_tics = tics;

  kbps = ((double)bytes) / (double)(tics - start_tics);
  if (max_bytes > 0)
    {
      int perc = (int)(100.0 * ((double)bytes) / (double)max_bytes);
      Progress.SetBar1(bytes, max_bytes);
      sprintf (buf, "%d %%  (%dk/%dk)  %03.1f kb/s\n",
	       perc, bytes / 1000, max_bytes / 1000, kbps);
      if (total_download_bytes > 0)
     	  Progress.SetBar2(total_download_bytes_sofar + bytes,
			   total_download_bytes);
    }
  else
    sprintf (buf, "%d  %2.1f kb/s\n", bytes, kbps);

  Progress.SetText3(buf);
}

static void
getUrlToStream (String const &_url, HWND owner, io_stream *output)
{
  log (LOG_BABBLE) << "getUrlToStream " << _url << endLog;
  is_local_install = (source == IDC_SOURCE_CWD);
  init_dialog (_url, 0, owner);
  NetIO *n = NetIO::open (_url.cstr_oneuse());
  if (!n || !n->ok ())
    {
      delete n;
      log (LOG_BABBLE) <<  "getUrlToStream failed!" << endLog;
      throw new Exception (TOSTRING(__LINE__) " " __FILE__, "Error opening url",  APPERR_IO_ERROR);
    }

  if (n->file_size)
    max_bytes = n->file_size;

  int total_bytes = 0;
  progress (0);
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
	  progress (total_bytes);
	}
      else
	break;
    }
  if (n)
    delete (n);
  /* reseeking is up to the recipient if desired */
}

io_stream *
get_url_to_membuf (String const &_url, HWND owner)
{
  io_stream_memory *membuf = new io_stream_memory ();
  try 
    {
      log (LOG_BABBLE) << "get_url_to_membuf " << _url << endLog;
      getUrlToStream (_url, owner, membuf);
      
      if (membuf->seek (0, IO_SEEK_SET))
    	{
    	  if (membuf)
      	      delete membuf;
    	  log (LOG_BABBLE) << "get_url_to_membuf(): seek (0) failed for membuf!" << endLog;
    	  return 0;
	}
      return membuf;
    }
  catch (Exception *e)
    {
      if (e->errNo() != APPERR_IO_ERROR)
	throw e;
      log (LOG_BABBLE) << "get_url_to_membuf failed!" << endLog;
      delete membuf;
      return 0;
    }
}

// predicate: url has no '\0''s in it.
String
get_url_to_string (String const &_url, HWND owner)
{
  io_stream *stream = get_url_to_membuf (_url, owner);
  if (!stream)
    return String();
  size_t bytes = stream->get_size ();
  if (!bytes)
    {
      /* zero length, or error retrieving length */
      delete stream;
      log (LOG_BABBLE) << "get_url_to_string(): couldn't retrieve buffer size, or zero length buffer" << endLog;
      return String();
    }
  char temp [bytes + 1];
  /* membufs are quite safe */
  stream->read (temp, bytes);
  temp [bytes] = '\0';
  delete stream;
  return String(temp);
}

int
get_url_to_file (String const &_url,
                 String const &_filename,
                 int expected_length,
		 HWND owner)
{
  log (LOG_BABBLE) << "get_url_to_file " << _url << " " << _filename << endLog;
  if (total_download_bytes > 0)
    {
      int df = diskfull (get_root_dir ().cstr_oneuse());
      Progress.SetBar3(df);
    }
  init_dialog (_url, expected_length, owner);

  remove (_filename.cstr_oneuse());		/* but ignore errors */

  NetIO *n = NetIO::open (_url.cstr_oneuse());
  if (!n || !n->ok ())
    {
      delete n;
      log (LOG_BABBLE) <<  "get_url_to_file failed!" << endLog;
      return 1;
    }

  FILE *f = fopen (_filename.cstr_oneuse(), "wb");
  if (!f)
    {
      const char *err = strerror (errno);
      if (!err)
	err = "(unknown error)";
      fatal (owner, IDS_ERR_OPEN_WRITE, _filename.cstr_oneuse(), err);
    }

  if (n->file_size)
    max_bytes = n->file_size;

  int total_bytes = 0;
  progress (0);
  while (1)
    {
      char buf[8192];
      int count;
      count = n->read (buf, sizeof (buf));
      if (count <= 0)
	break;
      fwrite (buf, 1, count, f);
      total_bytes += count;
      progress (total_bytes);
    }

  total_download_bytes_sofar += total_bytes;

  fclose (f);
  if (n)
    delete n;

  if (total_download_bytes > 0)
    {
      int df = diskfull (get_root_dir ().cstr_oneuse());
	  Progress.SetBar3(df);
    }

  return 0;
}

