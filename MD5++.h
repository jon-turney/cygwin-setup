/*
 * Copyright (c) 2001, Robert Collins.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Robert Collins  <rbtcollins@hotmail.com>
 *
 */

/* this is the parent class for all package source (not source code - installation
 * source as in http/ftp/disk file) operations.
 */

#ifndef _MD5___H_
#define _MD5___H_

/* required to parse this file */
#include "String++.h"

// trivial c++ support class for the md5 storage
class md5
{
public:
  md5(): _set(false){}
  bool isSet() const {return _set;}
  void set(unsigned char const hexdigest[16]){for (int i =0 ;i < 16; ++i)digest[i] = hexdigest[i]; _set = true;}
  unsigned char const * get() const {return digest;}
  String print() const;
  bool operator == (md5 const &rhs)const;
  bool operator != (md5 const &rhs)const;
private:
  unsigned char digest[16];
  bool _set;
};

#endif /* _MD5___H_ */
