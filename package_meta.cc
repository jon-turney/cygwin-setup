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

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include "concat.h"

#include "io_stream.h"
#include "compress.h"

#include "filemanip.h"

#include "package.h"
#include "cygpackage.h"
#include "package_meta.h"

void
packagemeta::add_version (genericpackage & thepkg)
{
  if (versionspace == versioncount)
    {
      genericpackage **newversions =
	(genericpackage **) realloc (versions,
				     sizeof (genericpackage *) *
				     (versionspace + 5));
      if (!newversions)
	{
	  //die badly
	  exit (101);
	}
      versions = newversions;
      versionspace += 5;
    }
  versions[versioncount] = &thepkg;
  versioncount++;
}

/* assumption: package thepkg is already in the metadata list. */
void
packagemeta::set_installed (genericpackage & thepkg)
{
  for (size_t n = 0; n < versioncount; n++)
    {
      if (versions[n] == &thepkg)
	{
	  installed = &thepkg;
	  return;
	}
    }
}
