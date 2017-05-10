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
#include "filemanip.h"

#include "threebar.h"

#include "Exception.h"

#include "LogSingleton.h"

using namespace std;

extern ThreeBarProgressPage Progress;

static int max_bytes = 0;
static int is_local_install = 0;

long long int total_download_bytes = 0;
long long int total_download_bytes_sofar = 0;

static DWORD start_tics;

static void
init_dialog (const string &url, int length)
{
  if (is_local_install)
    return;

  string::size_type divide = url.find_last_of('/');
  max_bytes = length;
  Progress.SetText1("Downloading...");
  Progress.SetText2((url.substr(divide + 1) + " from "
                     + url.substr(0, divide)).c_str());
  Progress.SetText3("Connecting...");
  Progress.SetBar1(0);
  start_tics = GetTickCount ();
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
      sprintf (buf, "%d %%  (%dk/%dk)  %03.1f kB/s",
	       perc, bytes / 1000, max_bytes / 1000, kbps);
      if (total_download_bytes > 0)
     	  Progress.SetBar2(total_download_bytes_sofar + bytes,
			   total_download_bytes);
    }
  else
    sprintf (buf, "%d  %2.1f kB/s", bytes, kbps);

  Progress.SetText3(buf);
}

static void
getUrlToStream (const string &_url, io_stream *output)
{
  is_local_install = (source == IDC_SOURCE_LOCALDIR);
  init_dialog (_url, 0);
  NetIO *n = NetIO::open (_url.c_str(), true);
  if (!n || !n->ok ())
    {
      delete n;
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

  Log (LOG_BABBLE) << "Fetched URL: " << _url << endLog;
}

io_stream *
get_url_to_membuf (const string &_url, HWND owner)
{
  io_stream_memory *membuf = new io_stream_memory ();
  try 
    {
      getUrlToStream (_url, membuf);
      
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
string
get_url_to_string (const string &_url, HWND owner)
{
  io_stream *stream = get_url_to_membuf (_url, owner);
  if (!stream)
    return string();
  size_t bytes = stream->get_size ();
  if (!bytes)
    {
      /* zero length, or error retrieving length */
      delete stream;
      Log (LOG_BABBLE) << "get_url_to_string(): couldn't retrieve buffer size, or zero length buffer" << endLog;
      return string();
    }
  char temp [bytes + 1];
  /* membufs are quite safe */
  stream->read (temp, bytes);
  temp [bytes] = '\0';
  delete stream;
  return string(temp);
}

int
get_url_to_file (const string &_url,
                 const string &_filename,
                 int expected_length,
		 HWND owner)
{
  Log (LOG_BABBLE) << "get_url_to_file " << _url << " " << _filename << endLog;
  if (total_download_bytes > 0)
    {
      int df = diskfull (get_root_dir ().c_str());
      Progress.SetBar3(df);
    }
  init_dialog (_url, expected_length);

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
      fatal (owner, IDS_ERR_OPEN_WRITE, _filename.c_str(), err);
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
      int df = diskfull (get_root_dir ().c_str());
	  Progress.SetBar3(df);
    }

  return 0;
}

