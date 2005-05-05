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

#include "cygpackage.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "String++.h"

#include "io_stream.h"
#include "compress.h"

#include "package_version.h"
#include "cygpackage.h"

/* this constructor creates an invalid package - further details MUST be provided */
cygpackage::cygpackage ():
name (),
vendor (),
packagev (),
canonical (),
fn (),
sdesc (),
ldesc (),
status (package_installed),
type (package_binary),
listdata (),
listfile ()
{
  memset( getfilenamebuffer, '\0', MAX_PATH);

  /* FIXME: query the install database for the currently installed 
   * version details
   */
}

packageversion
cygpackage::createInstance (String const &pkgname)
{
  cygpackage *temp = new cygpackage;
  temp->name = pkgname;
  return packageversion(temp);
}

packageversion
cygpackage::createInstance (String const &pkgname, String const &filename,
			    size_t const fs, String const &version,
			    package_status_t const newstatus,
			    package_type_t const newtype)
{
  cygpackage *temp = new cygpackage;
  temp->name = pkgname;
  temp->fn = filename;
  temp->status = newstatus;
  temp->type = newtype;
  temp->filesize = fs;
  temp->setCanonicalVersion (version);
  return packageversion(temp);
}

/* tell the version */
void
cygpackage::setCanonicalVersion (String const &version)
{
  canonical = version;
  char *start = strchr (canonical.c_str(), '-');
  char*curr=start;
  if (curr)
    {
      char *next;
      while ((next = strchr (curr + 1, '-')))
	curr = next;
      /* curr = last - in the version string */
      packagev = curr + 1;
      char tvendor [version.size() +1];
      strcpy (tvendor, version.c_str());
      tvendor[curr - start] = '\0';
      vendor=tvendor;
    }
  else
    {
      packagev = 0;
      vendor = version;
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
}

String const
cygpackage::getfirstfile ()
{
  if (listdata)
    delete listdata;
  listfile =
    io_stream::open (String ("cygfile:///etc/setup/") + name + ".lst.gz", "rb");
  listdata = compress::decompress (listfile);
  if (!listdata)
    return String();
  return listdata->gets (getfilenamebuffer, sizeof (getfilenamebuffer));
}

String const
cygpackage::getnextfile ()
{
  if (listdata)
    return listdata->gets (getfilenamebuffer, sizeof (getfilenamebuffer));
  return String();
}

void
cygpackage::uninstall ()
{
  if (listdata)
    delete listdata;
  listdata = 0;
  io_stream::remove (String("cygfile:///etc/setup/") + name + ".lst.gz");
}

String const
cygpackage::Name ()
{
  return name;
}

String const
cygpackage::Vendor_version ()
{
  return vendor;
}

String const
cygpackage::Package_version ()
{
  return packagev;
}

String  const
cygpackage::Canonical_version ()
{
  return canonical;
}

void
cygpackage::set_sdesc (String const &desc)
{
  sdesc = desc;
}

void
cygpackage::set_ldesc (String const &desc)
{
  ldesc = desc;
}

#if 0
package_stability_t cygpackage::Stability ()
{
  return stability;
}
#endif
