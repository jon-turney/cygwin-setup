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

/* The purpose of this file is to manage internet downloads using the
   Internet Explorer version 5 DLLs.  To use this method, the user
   must already have installed and configured IE5.  This module is
   called from netio.cc, which is called from geturl.cc */

#include "win32.h"

#include "resource.h"
#include "state.h"
#include "dialog.h"
#include "msg.h"
#include "netio.h"
#include "nio-ie5.h"

static HINTERNET internet = 0;

NetIO_IE5::NetIO_IE5 (char *_url)
  : NetIO (_url)
{
  if (internet == 0)
    internet = InternetOpen ("Cygwin Setup", INTERNET_OPEN_TYPE_PRECONFIG,
			     NULL, NULL, 0);

  DWORD flags =
    INTERNET_FLAG_DONT_CACHE |
    INTERNET_FLAG_KEEP_CONNECTION |
    INTERNET_FLAG_PRAGMA_NOCACHE |
    INTERNET_FLAG_RELOAD |
    INTERNET_FLAG_EXISTING_CONNECT |
    INTERNET_FLAG_PASSIVE;

  if (net_proxy_user && net_proxy_passwd)
    {
      InternetSetOption (internet, INTERNET_OPTION_PROXY_USERNAME,
			 net_proxy_user, strlen (net_proxy_user));
      InternetSetOption (internet, INTERNET_OPTION_PROXY_PASSWORD,
			 net_proxy_passwd, strlen (net_proxy_passwd));
    }

  connection = InternetOpenUrl (internet, url, NULL, 0, flags, 0);

  if (!connection)
    {
      if (GetLastError () == ERROR_INTERNET_EXTENDED_ERROR)
	{
	  char buf[2000];
	  DWORD e, l=sizeof (buf);
	  InternetGetLastResponseInfo (&e, buf, &l);
	  MessageBox (0, buf, "Internet Error", 0);
	}
    }
}

NetIO_IE5::~NetIO_IE5 ()
{
  if (connection)
    InternetCloseHandle (connection);
}

int
NetIO_IE5::ok ()
{
  return (connection == NULL) ? 0 : 1;
}

int
NetIO_IE5::read (char *buf, int nbytes)
{
  DWORD actual;
  if (InternetReadFile (connection, buf, nbytes, &actual))
    return actual;
  return -1;
}
