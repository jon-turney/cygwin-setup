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

#include "MD5++.h"

String
md5::print() const
{
  char d1[33];
  d1[32] = '\0';
  for (int i=0; i < 16; ++i)
    {
      char tc = (digest[i] & 0xf0) >> 4;
      d1[i * 2] = tc < 10 ? tc + '0' : tc + 'a' - 10;
      tc = digest[i] & 0x0f;
      d1[i * 2 + 1] = tc < 10 ? tc + '0' : tc + 'a' - 10;
    }
  return String(d1);
}

bool
md5::operator == (md5 const &rhs) const
{
  for (int i=0; i < 16; ++i)
    if (digest[i] != rhs.digest[i])
      return false;
  return true;
}

bool
md5::operator != (md5 const &rhs) const
{
  return !(*this == rhs);
}
