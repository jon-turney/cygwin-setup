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

/* this is the parent class for all package operations. 
 */

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include "package_version.h"
packageversion::packageversion():sources(0),sourcecount(0),sourcespace(0),srcsources(0),srcsourcecount(0),srcsourcespace(0)
{
};
