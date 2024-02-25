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

#ifndef SETUP_GETNETAUTH_H
#define SETUP_GETNETAUTH_H

class GetNetAuth
{
public:
  /* Helper functions for http/ftp protocols.  Both return nonzero for
     "cancel", zero for "ok".  They set net_proxy_user, etc, in
     state.h */
  virtual int get_auth () = 0;
  virtual int get_proxy_auth () = 0;
  virtual int get_ftp_auth () = 0;
};

#endif /* SETUP_GETNETAUTH_H */
