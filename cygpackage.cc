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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include "concat.h"

#include "io_stream.h"
#include "compress.h"

#include "package_version.h"
#include "cygpackage.h"

/* this constructor creates an invalid package - further details MUST be provided */
cygpackage::cygpackage (const char *pkgname):
vendor (0),
packagev (0),
status (package_installed),
type (package_binary),
listdata (0),
listfile (0)
{
  name = strdup (pkgname);

  /* FIXME: query the install database for the currently installed 
   * version details
   */
}

/* create a package given explicit details - perhaps should be modified to take the
   filename and do it's own parsing? */
cygpackage::cygpackage (const char *pkgname, const char *filename, size_t fs,
			const char *version, package_status_t newstatus,
			package_type_t newtype):
sdesc (0),
ldesc (0),
status (newstatus),
type (newtype),
listdata (0),
listfile (0),
filesize (fs)
{
  name = strdup (pkgname);
  fn = strdup (fn);

  set_canonical_version (version);
}

/* tell the version */
void
cygpackage::set_canonical_version (char const *version)
{
  char *curr = strchr (version, '-');
  canonical = strdup (version);
  if (curr)
    {
      char *next;
      while ((next = strchr (curr + 1, '-')))
	curr = next;
      /* curr = last - in the version string */
      packagev = strdup (curr + 1);
      vendor = strdup (version);
      vendor[curr - version] = '\0';
    }
  else
    {
      packagev = 0;
      vendor = strdup (version);
    }
}

cygpackage::~cygpackage ()
{
  destroy ();
}

/* helper functions */

void
cygpackage::destroy ()
{

  if (name)
    free (name);
  if (vendor)
    free (vendor);
  if (packagev)
    free (packagev);
  if (canonical)
    free (canonical);
  if (fn)
    free (fn);
  if (listdata)
    delete listdata;
  if (sdesc)
    delete sdesc;
  if (ldesc)
    delete ldesc;
}

const char *
cygpackage::getfirstfile ()
{
  if (listdata)
    delete listdata;
  listfile =
    io_stream::open (concat ("cygfile:///etc/setup/", name, ".lst.gz", 0),
		     "rb");
  listdata = compress::decompress (listfile);
  if (!listdata)
    return 0;
  return listdata->gets (fn, sizeof (fn));
}

const char *
cygpackage::getnextfile ()
{
  if (listdata)
    return listdata->gets (fn, sizeof (fn));
  return 0;
}

void
cygpackage::uninstall ()
{
  if (listdata)
    delete listdata;
  listdata = 0;
  io_stream::remove (concat ("cygfile:///etc/setup/", name, ".lst.gz", 0));
}

const char *
cygpackage::Name ()
{
  return name;
}

const char *
cygpackage::Vendor_version ()
{
  return vendor;
}

const char *
cygpackage::Package_version ()
{
  return packagev;
}

const char *
cygpackage::Canonical_version ()
{
  return canonical;
}

void
cygpackage::set_sdesc (char const *desc)
{
  if (sdesc)
    delete sdesc;
  sdesc = new char[strlen (desc) + 1];
  strcpy (sdesc, desc);
}

void
cygpackage::set_ldesc (char const *desc)
{
  if (ldesc)
    delete ldesc;
  ldesc = new char[strlen (desc) + 1];
  strcpy (ldesc, desc);
}

#if 0
package_stability_t
cygpackage::Stability ()
{
  return stability;
}
#endif
