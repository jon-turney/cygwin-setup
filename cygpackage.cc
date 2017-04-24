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

#include "cygpackage.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "io_stream.h"
#include "compress.h"

#include "package_version.h"
#include "cygpackage.h"
#include "LogSingleton.h"

/* this constructor creates an invalid package - further details MUST be provided */
cygpackage::cygpackage ():
name (),
vendor (),
packagev (),
canonical (),
sdesc (),
ldesc (),
type (package_binary)
{
  /* FIXME: query the install database for the currently installed 
   * version details
   */
}

packageversion
cygpackage::createInstance (const std::string& pkgname,
                            const package_type_t type)
{
  cygpackage *temp = new cygpackage;
  temp->name = pkgname;
  temp->type = type;
  return packageversion(temp);
}

packageversion
cygpackage::createInstance (const std::string& pkgname,
                            const std::string& version,
			    package_type_t const newtype)
{
  cygpackage *temp = new cygpackage;
  temp->name = pkgname;
  temp->type = newtype;
  temp->setCanonicalVersion (version);
  return packageversion(temp);
}

/* tell the version */
void
cygpackage::setCanonicalVersion (const std::string& version)
{
  canonical = version;

  const char *start = canonical.c_str();
  const char *curr = strchr(start, '-');

  if (curr)
    {
      const char *next;
      while ((next = strchr (curr + 1, '-')))
	curr = next;

      /* package version appears after the last '-' in the version string */
      packagev = curr + 1;
      /* vendor version is everything up to that last '-' */
      vendor.assign(canonical.c_str(), (size_t)(curr - start));
    }
  else
    {
      // FIXME: What's up with the "0"? It's probably a mistake, and should be
      // "". It used to be written as 0, and was subject to a bizarre implicit
      // conversion by the unwise String(int) constructor.
      packagev = "0";
      vendor = version;
    }
}

cygpackage::~cygpackage ()
{
}

const std::string
cygpackage::Name ()
{
  return name;
}

const std::string
cygpackage::Vendor_version ()
{
  return vendor;
}

const std::string
cygpackage::Package_version ()
{
  return packagev;
}

std::string  const
cygpackage::Canonical_version ()
{
  return canonical;
}

void
cygpackage::set_sdesc (const std::string& desc)
{
  sdesc = desc;
}

void
cygpackage::set_ldesc (const std::string& desc)
{
  ldesc = desc;
}
