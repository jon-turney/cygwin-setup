/*
 * Copyright (c) 2000, Red Hat, Inc.
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

/* The purpose of this file is to coordinate the various access
   methods known to setup.  To add a new method, create a pair of
   nio-*.[ch] files and add the logic to NetIO::open here */

#include "win32.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "resource.h"
#include "state.h"
#include "msg.h"
#include "netio.h"
#include "nio-file.h"
#include "nio-ie5.h"

#include "port.h"

NetIO::NetIO (char *Purl)
{
  char *bp, *ep, c;

  file_size = 0;
  url = _strdup (Purl);
  proto = 0;
  host = 0;
  port = 0;
  path = 0;

  bp = url;
  ep = strstr (bp, "://");
  if (!ep)
    {
      path = url;
      return;
    }

  *ep = 0;
  proto = _strdup (bp);
  *ep = ':';
  bp = ep+3;

  ep += strcspn (bp, ":/");
  c = *ep;
  *ep = 0;
  host = _strdup (bp);
  *ep = c;

  if (*ep == ':')
    {
      port = atoi (ep+1);
      ep = strchr (ep, '/');
    }

  if (*ep)
    path = _strdup (ep);
}

NetIO::~NetIO ()
{
  if (url)
    free (url);
  if (proto)
    free (proto);
  if (host)
    free (host);
  if (path)
    free (path);
}

int
NetIO::ok ()
{
  return 0;
}

int
NetIO::read (char *buf, int nbytes)
{
  return 0;
}

NetIO *
NetIO::open (char *url)
{
  NetIO *rv = 0;
  enum {http, ftp, file} proto;
  if (strncmp (url, "http://", 7) == 0)
    proto = http;
  else if (strncmp (url, "ftp://", 6) == 0)
    proto = ftp;
  else
    proto = file;

  if (proto == file)
    rv = new NetIO_File (url);
  else if (net_method == IDC_NET_IE5)
    rv = new NetIO_IE5 (url);
#if 0
  else if (net_method == IDC_NET_DIRECT)
    rv = new NetIO_Direct (url);
  else if (net_method == IDC_NET_PROXY)
    rv = new NetIO_Proxy (url);
#endif

  if (!rv->ok ())
    {
      delete rv;
      return 0;
    }

  return rv;
}
