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

/* The purpose of this file is to coordinate the various access
   methods known to setup.  To add a new method, create a pair of
   nio-*.[ch] files and add the logic to NetIO::open here */

#include "netio.h"

#include "LogFile.h"

#include <shlwapi.h>

#include "resource.h"
#include "nio-ie5.h"

int NetIO::net_method;
char *NetIO::net_proxy_host;
int NetIO::net_proxy_port;

char *NetIO::net_user;
char *NetIO::net_passwd;
char *NetIO::net_proxy_user;
char *NetIO::net_proxy_passwd;
char *NetIO::net_ftp_user;
char *NetIO::net_ftp_passwd;
GetNetAuth *NetIO::auth_getter;

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
NetIO::open (char const *url, bool cachable)
{
  NetIO *rv = 0;
  std::string file_url;

  enum
  { http, https, ftp, ftps, file }
  proto;
  if (strncmp (url, "http://", 7) == 0)
    proto = http;
  else if (strncmp (url, "https://", 8) == 0)
    proto = https;
  else if (strncmp (url, "ftp://", 6) == 0)
    proto = ftp;
  else if (strncmp (url, "ftps://", 7) == 0)
    proto = ftps;
  else if (strncmp (url, "file://", 7) == 0)
    {
      proto = file;

      // WinInet expects a 'legacy' file:// URL
      // (i.e. a windows path with "file://" prepended)
      // https://blogs.msdn.microsoft.com/freeassociations/2005/05/19/the-bizarre-and-unhappy-story-of-file-urls/
      char path[MAX_PATH];
      DWORD len = MAX_PATH;
      if (S_OK == PathCreateFromUrl(url, path, &len, 0))
        {
          file_url = std::string("file://") + path;
          url = file_url.c_str();
        }
    }
  else
    // treat everything else as a windows path
    {
      proto = file;
      file_url = std::string("file://") + url;
      url = file_url.c_str();
    }

  rv = new NetIO_IE5 (url, proto == file ? false : cachable);

  if (rv && !rv->ok ())
    {
      delete rv;
      return 0;
    }

  return rv;
}

const char *
NetIO::net_method_name ()
{
  switch (net_method)
    {
    case IDC_NET_PRECONFIG:
      return "Preconfig";
    case IDC_NET_DIRECT:
      return "Direct";
    case IDC_NET_PROXY:
      return "Proxy";
    default:
      return "Unknown";
    }
}
