/*
 * Copyright (c) 2002, Robert Collins.
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

#include "IniDBBuilderPackage.h"
#include "package_db.h"
#include "package_meta.h"
#include "package_version.h"
#include "cygpackage.h"
#include "filemanip.h"
// for strtoul
#include <string.h>

void
IniDBBuilderPackage::buildTimestamp (String const &time)
{
  timestamp = strtoul (time.cstr_oneuse(), 0, 0);
}

void
IniDBBuilderPackage::buildVersion (String const &aVersion)
{
  version = aVersion;
}

void
IniDBBuilderPackage::buildPackage (String const &name)
{
  packagedb db;
  cp = &db.packages.registerbykey(name);
  cpv = new cygpackage (name);
  trust = TRUST_CURR;
}

void
IniDBBuilderPackage::buildPackageVersion (String const &version)
{
  cpv->set_canonical_version (version);
  add_correct_version();
}

void
IniDBBuilderPackage::buildPackageSDesc (String const &theDesc)
{
  cpv->set_sdesc(theDesc);
}

void
IniDBBuilderPackage::buildPackageLDesc (String const &theDesc)
{
  cpv->set_ldesc(theDesc);
}

void
IniDBBuilderPackage::buildPackageInstall (String const &path,
					  String const &size, String const &md5)
{
  process_src (cpv->bin, path, size, md5);
}
void
IniDBBuilderPackage::buildPackageSource (String const &path,
  					 String const &size, String const &md5)
{
  process_src (cpv->src, path, size, md5);
}

void
IniDBBuilderPackage::buildPackageTrust (int newtrust)
{
  trust = newtrust;
  if (newtrust != TRUST_UNKNOWN)
    cpv = new cygpackage (cp->name);
}

void
IniDBBuilderPackage::buildPackageRequirement (String const &name)
{
  cpv->new_requirement(name);
}

void
IniDBBuilderPackage::buildPackageCategory (String const &name)
{
  packagedb db;
  cp->add_category (db.categories.registerbykey (name));
}

void
IniDBBuilderPackage::add_correct_version()
{
  int merged = 0;
  for (size_t n = 1; !merged && n <= cp->versions.number (); n++)
      if (!cp->versions[n]->Canonical_version().casecompare(cpv->Canonical_version()))
      {
        /* ASSUMPTIONS:
           categories and requires are consistent for the same version across
           all mirrors
           */
        /* Copy the binary mirror across if this site claims to have an install */
        if (cpv->bin.sites.number ())
          cp->versions[n]->bin.sites.registerbykey (cpv->bin.sites[1]->key);
        /* Ditto for src */
        if (cpv->src.sites.number ())
          cp->versions[n]->src.sites.registerbykey (cpv->src.sites[1]->key);
        /* Copy the descriptions across */
        if (cpv->SDesc ().size() && !cp->versions[n]->SDesc ().size())
          cp->versions[n]->set_sdesc (cpv->SDesc ());
        if (cpv->LDesc ().size() && !cp->versions[n]->LDesc ().size())
          cp->versions[n]->set_ldesc (cpv->LDesc ());
        cpv = (cygpackage *)cp->versions[n];
        merged = 1;
      }
    if (!merged)
    cp->add_version (*cpv);
  /* trust setting */
  switch (trust)
  {
    case TRUST_CURR:
      if (cp->currtimestamp < timestamp)
      {
        cp->currtimestamp = timestamp;
        cp->curr = cpv;
      }
    break;
    case TRUST_PREV:
    if (cp->prevtimestamp < timestamp)
    {
        cp->prevtimestamp = timestamp;
        cp->prev = cpv;
    }
    break;
    case TRUST_TEST:
    if (cp->exptimestamp < timestamp)
    {
        cp->exptimestamp = timestamp;
        cp->exp = cpv;
    }
    break;
  }
}

void
IniDBBuilderPackage::process_src (packagesource &src, 
	     String const &path, String const &size, String const& md5)
{
  if (!cpv->Canonical_version ().size())
    {
      fileparse f;
      if (parse_filename (path, f))
	{
	  cpv->set_canonical_version (f.ver);
	  add_correct_version ();
	}
    }

  if (!src.size)
    {
      src.size = atoi(size.cstr_oneuse());
      src.set_canonical (path.cstr_oneuse());
    }
  if (md5.size() && !src.md5.isSet())
    src.md5.set((unsigned char *)md5.cstr_oneuse());
  src.sites.registerbykey (parse_mirror);
}
