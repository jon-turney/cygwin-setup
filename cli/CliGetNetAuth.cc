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
 */

/* Query user for auth information required */

#include "netio.h"
#include "CliGetNetAuth.h"

#include "LogFile.h"

static int
auth_common(const char *mode)
{
  Log (LOG_PLAIN) << mode << " not implemented" << endLog;
  Logger ().exit (1);
  return 1;
}

int
CliGetNetAuth::get_auth ()
{
  return auth_common("get_auth");
}

int
CliGetNetAuth::get_proxy_auth ()
{
  return auth_common("get_proxy_auth");
}

int
CliGetNetAuth::get_ftp_auth ()
{
  return auth_common("get_ftp_auth");
}
