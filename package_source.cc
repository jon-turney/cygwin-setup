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

#include "package_source.h"

site::site (const std::string& newkey) : key(newkey)
{
}

void
packagesource::set_canonical (char const *fn)
{
  canonical = fn;
}

void
packagesource::set_cached (const std::string& fp)
{
  cached = fp;
}
