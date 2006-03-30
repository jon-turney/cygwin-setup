/*
 * Copyright (c) 2003, Robert Collins <rbtcollins@hotmail.com>
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Robert Collins.
 *
 */

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include "ConnectionSetting.h"
#include "UserSettings.h"
#include "io_stream.h"
#include "netio.h"
#include "resource.h"
#include "String++.h"

void
ConnectionSetting::load()
{
  static int inited = 0;
  if (inited)
    return;
  io_stream *f = UserSettings::Instance().settingFileForLoad("last-connection");
  if (f)
    {
      char localdir[1000];
      char *fg_ret = f->gets (localdir, 1000);
      if (fg_ret)
        NetIO::net_method = typeFromString(fg_ret);
      fg_ret = f->gets (localdir, 1000);
      if (fg_ret)
        NetIO::net_proxy_host = strdup(fg_ret);
      fg_ret = f->gets (localdir, 1000);
      if (fg_ret)
        NetIO::net_proxy_port = atoi(fg_ret);
      delete f;
    }
  inited = 1;
}

void
ConnectionSetting::save()
{
  char port_str[20];
  
  io_stream *f = UserSettings::Instance().settingFileForSave("last-connection");
  if (f)
    {
      switch (NetIO::net_method) {
        case IDC_NET_DIRECT:
            f->write("Direct\n",7);
            break;
        case IDC_NET_IE5:
            f->write("IE\n",3);
            break;
        case IDC_NET_PROXY:
            f->write("Proxy\n",6);
            f->write(NetIO::net_proxy_host, strlen(NetIO::net_proxy_host));
            sprintf(port_str, "\n%d\n", NetIO::net_proxy_port);
            f->write(port_str, strlen(port_str));
            break;
        default:
            break;
      }
      delete f;
    }
}

int
ConnectionSetting::typeFromString(String const & aType)
{
  if (!casecompare(aType, "Direct"))
    return IDC_NET_DIRECT;
  if (!casecompare(aType, "IE"))
    return IDC_NET_IE5;
  if (!casecompare(aType, "Proxy"))
    return IDC_NET_PROXY;

  /* A sanish default */
  return IDC_NET_IE5;
}
