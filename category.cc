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

/* categories for packages */

#include <stdlib.h>
#include <string.h>

#include "category.h"

/* normal members */

Category::Category ():next (0), name (0)
{
}

Category::Category (const char *categoryname):
next (0)
{
  /* FIXME: search the global category list for name, and reuse that pointer */
  name = strdup (categoryname);
}
