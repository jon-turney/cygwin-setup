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
#include "log.h"
#include "io_stream.h"
#include "io_stream_memory.h"
#include "state.h"
#include "diskfull.h"
#include "mount.h"

#include "threebar.h"
extern ThreeBarProgressPage Progress;

static int max_bytes = 0;
static int is_local_install = 0;

int total_download_bytes = 0;
int total_download_bytes_sofar = 0;

static DWORD start_tics;

static void
init_dialog (char const *url, int length, HWND owner)
{
  if (is_local_install)
    return;

  char const *sl = url;
  char const *cp;
  for (cp = url; *cp; cp++)
    if (*cp == '/' || *cp == '\\' || *cp == ':')
      sl = cp + 1;
  max_bytes = length;
  Progress.SetText1("Downloading...");
  Progress.SetText2(sl);
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
      sprintf (buf, "%3d %%  (%dk/%dk)  %2.1f kb/s\n",
	       perc, bytes / 1000, max_bytes / 1000, kbps);
      if (total_download_bytes > 0)
	{
      Progress.SetBar2(total_download_bytes_sofar + bytes, total_download_bytes);
	}
    }
  else
    sprintf (buf, "%d  %2.1f kb/s\n", bytes, kbps);

  Progress.SetText3(buf);
}

io_stream *
get_url_to_membuf (char const *_url, HWND owner)
{
	log (LOG_BABBLE, "get_url_to_membuf %s", _url);
  is_local_install = (source == IDC_SOURCE_CWD);
  init_dialog (_url, 0, owner);
  NetIO *n = NetIO::open (_url);
  if (!n || !n->ok ())
    {
      delete n;
      log (LOG_BABBLE, "get_url_to_membuf failed!");
      return 0;
    }

  if (n->file_size)
    max_bytes = n->file_size;

  io_stream_memory *membuf = new io_stream_memory ();

  int total_bytes = 0;
  progress (0);
  while (1)
    {
      char buf[2048];
      ssize_t rlen, wlen;
      rlen = n->read (buf, 2048);
      if (rlen > 0)
	{
	  wlen = membuf->write (buf, rlen);
	  if (wlen != rlen)
	    /* FIXME: Show an error message */
	    break;
	  total_bytes += rlen;
	  progress (total_bytes);
	}
      else
	break;
    }

  if (membuf->seek (0, IO_SEEK_SET))
    {
      if (n)
	delete n;
      if (membuf)
	delete membuf;
      log (LOG_BABBLE, "get_url_to_membuf(): seek (0) failed for membuf!");
      return 0;
    }

  if (n)
    delete n;
  return membuf;
}

char *
get_url_to_string (char const *_url, HWND owner)
{
  io_stream *stream = get_url_to_membuf (_url, owner);
  if (!stream)
    return 0;
  size_t bytes = stream->get_size ();
  if (!bytes)
    {
      /* zero length, or error retrieving length */
      delete stream;
      log (LOG_BABBLE, "get_url_to_string(): couldn't retrieve buffer size, or zero length buffer");
      return 0;
    }
  char *rv = new char [bytes + 1];
  if (!rv)
    {
      delete stream;
      log (LOG_BABBLE, "get_url_to_string(): new failed for rv!");
      return 0;
    }
  /* membufs are quite safe */
  stream->read (rv, bytes);
  rv [bytes] = '\0';
  delete stream;
  return rv;
}

int
get_url_to_file (char *_url, char *_filename, int expected_length,
		 HWND owner, BOOL allow_ftp_auth)
{
  log (LOG_BABBLE, "get_url_to_file %s %s", _url, _filename);
  if (total_download_bytes > 0)
    {
      int df = diskfull (get_root_dir ());
	  Progress.SetBar3(df);
    }
  init_dialog (_url, expected_length, owner);

  remove (_filename);		/* but ignore errors */

  NetIO *n = NetIO::open (_url, allow_ftp_auth);
  if (!n || !n->ok ())
    {
      delete n;
      log (LOG_BABBLE, "get_url_to_file failed!");
      return 1;
    }

  FILE *f = fopen (_filename, "wb");
  if (!f)
    {
      const char *err = strerror (errno);
      if (!err)
	err = "(unknown error)";
      fatal (owner, IDS_ERR_OPEN_WRITE, _filename, err);
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
      int df = diskfull (get_root_dir ());
	  Progress.SetBar3(df);
    }

  return 0;
}

