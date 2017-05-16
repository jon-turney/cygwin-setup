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

#include "csu_util/version_compare.h"

#include "setup_version.h"

#include "IniParseFeedback.h"
#include "package_db.h"
#include "package_meta.h"
#include "package_version.h"
#include "cygpackage.h"
#include "ini.h"
// for strtoul
#include <string.h>
#include "LogSingleton.h"
#include "PackageSpecification.h"
#include <algorithm>

using namespace std;

IniDBBuilderPackage::IniDBBuilderPackage (IniParseFeedback const &aFeedback) :
cp (0), cbpv (), cspv (), currentSpec (0), currentNodeList (0), trust (0), _feedback (aFeedback){}

IniDBBuilderPackage::~IniDBBuilderPackage()
{
}

void
IniDBBuilderPackage::buildTimestamp (const std::string& time)
{
  timestamp = strtoul (time.c_str(), 0, 0);
}

void
IniDBBuilderPackage::buildVersion (const std::string& aVersion)
{
  version = aVersion;
  if (version.size())
    {
      if (version_compare(setup_version, version) < 0)
	{
	  char old_vers[256];
	  snprintf (old_vers, sizeof old_vers,
	    "The current ini file is from a newer version of setup-%s.exe. "
	    "If you have any trouble installing, please download a fresh "
	    "version from http://www.cygwin.com/setup-%s.exe",
	    is_64bit ? "x86_64" : "x86",
	    is_64bit ? "x86_64" : "x86");
	  _feedback.warning(old_vers);
	}
    }
}

void
IniDBBuilderPackage::buildPackage (const std::string& name)
{
#if DEBUG
  if (cp)
    {
      Log (LOG_BABBLE) << "Finished with package " << cp->name << endLog;
      if (cbpv)
	{
	  Log (LOG_BABBLE) << "Version " << cbpv.Canonical_version() << endLog;
	  Log (LOG_BABBLE) << "Depends:";
	  dumpPackageDepends (cbpv.depends(), Log (LOG_BABBLE));
	  Log (LOG_BABBLE) << endLog;
	}
    }
#endif
  packagedb db;
  cp = db.findBinary (PackageSpecification(name));
  if (!cp)
    {
      cp = new packagemeta (name);
      db.packages.insert (packagedb::packagecollection::value_type(cp->name,cp));
    }
  cbpv = cygpackage::createInstance (name, package_binary);
  cspv = packageversion ();
  currentSpec = NULL;
  currentNodeList = NULL;
  trust = TRUST_CURR;
#if DEBUG
  Log (LOG_BABBLE) << "Created package " << name << endLog;
#endif
}

void
IniDBBuilderPackage::buildPackageVersion (const std::string& version)
{
  cbpv.setCanonicalVersion (version);
  add_correct_version();
}

void
IniDBBuilderPackage::buildPackageSDesc (const std::string& theDesc)
{
  cbpv.set_sdesc(theDesc);
}

void
IniDBBuilderPackage::buildPackageLDesc (const std::string& theDesc)
{
  cbpv.set_ldesc(theDesc);
}

void
IniDBBuilderPackage::buildPackageInstall (const std::string& path)
{
  process_src (*cbpv.source(), path);
}

void
IniDBBuilderPackage::buildPackageSource (const std::string& path,
                                         const std::string& size)
{
  packagedb db;
  /* get an appropriate metadata */
  csp = db.findSource (PackageSpecification (cbpv.Name()));
  if (!csp)
    {
      /* Copy the existing meta data to a new source package */
      csp = new packagemeta (*cp);
      /* delete versions information */
      csp->versions.clear();
      csp->desired = packageversion();
      csp->installed = packageversion();
      csp->curr = packageversion();
      csp->exp = packageversion();
      db.sourcePackages.insert (packagedb::packagecollection::value_type(csp->name,csp));
    }
  /* create a source packageversion */
  cspv = cygpackage::createInstance (cbpv.Name(), package_source);
  cspv.setCanonicalVersion (cbpv.Canonical_version());
  set<packageversion>::iterator i=find (csp->versions.begin(),
    csp->versions.end(), cspv);
  if (i == csp->versions.end())
    {
      csp->add_version (cspv);
    }
  else
    cspv = *i;

  if (!cspv.source()->Canonical())
    cspv.source()->set_canonical (path.c_str());
  cspv.source()->sites.push_back(site(parse_mirror));

  /* creates the relationship between binary and source packageversions */
  cbpv.setSourcePackageSpecification (PackageSpecification (cspv.Name()));
  PackageSpecification &spec = cbpv.sourcePackageSpecification();
  spec.setOperator (PackageSpecification::Equals);
  spec.setVersion (cbpv.Canonical_version());

  setSourceSize (*cspv.source(), size);
}

void
IniDBBuilderPackage::buildPackageTrust (int newtrust)
{
  trust = newtrust;
  if (newtrust != TRUST_UNKNOWN)
    {
      cbpv = cygpackage::createInstance (cp->name, package_binary);
      cspv = packageversion ();
    }
}

void
IniDBBuilderPackage::buildPackageCategory (const std::string& name)
{
  cp->add_category (name);
}

void
IniDBBuilderPackage::buildBeginDepends ()
{
#if DEBUG
  Log (LOG_BABBLE) << "Beginning of a depends statement for " << cp->name
    << endLog;
  dumpPackageDepends (currentNodeList, Log (LOG_BABBLE));
#endif
  currentSpec = NULL;
  currentNodeList = cbpv.depends();
}

void
IniDBBuilderPackage::buildInstallSize (const std::string &size)
{
  setSourceSize (*cbpv.source(), size);
}

void
IniDBBuilderPackage::buildInstallSHA512 (unsigned char const *sha512)
{
  if (sha512 && !cbpv.source()->sha512_isSet) {
    memcpy (cbpv.source()->sha512sum, sha512, sizeof cbpv.source()->sha512sum);
    cbpv.source()->sha512_isSet = true;
  }
}

void
IniDBBuilderPackage::buildSourceSHA512 (unsigned char const *sha512)
{
  if (sha512 && !cspv.source()->sha512_isSet) {
    memcpy (cspv.source()->sha512sum, sha512, sizeof cspv.source()->sha512sum);
    cspv.source()->sha512_isSet = true;
  }
}

void
IniDBBuilderPackage::buildInstallMD5 (unsigned char const *md5)
{
  if (md5 && !cbpv.source()->md5.isSet())
    cbpv.source()->md5.set(md5);
}

void
IniDBBuilderPackage::buildSourceMD5 (unsigned char const *md5)
{
  if (md5 && !cspv.source()->md5.isSet())
    cspv.source()->md5.set(md5);
}

void
IniDBBuilderPackage::buildBeginBuildDepends ()
{
#if DEBUG
  Log (LOG_BABBLE) << "Beginning of a Build-Depends statement" << endLog;
#endif
  currentSpec = NULL;
  currentNodeList = NULL; /* there is currently nowhere to store Build-Depends information */
}

void
IniDBBuilderPackage::buildSourceName (const std::string& name)
{
  if (cbpv)
    {
      cbpv.setSourcePackageSpecification (PackageSpecification (name));
#if DEBUG
      Log (LOG_BABBLE) << "\"" << cbpv.sourcePackageSpecification() <<
	"\" is the source package for " << cp->name << "." << endLog;
#endif
    }
  else
      _feedback.warning ((std::string ("Attempt to set source for package")
                          + std::string(cp->name)
			  + "before creation of a version.").c_str());
}

void
IniDBBuilderPackage::buildSourceNameVersion (const std::string& version)
{
  if (cbpv)
    {
      cbpv.sourcePackageSpecification().setOperator (PackageSpecification::Equals);
      cbpv.sourcePackageSpecification().setVersion (version);
#if DEBUG
      Log (LOG_BABBLE) << "The source version needed for " << cp->name <<
	" is " << version << "." << endLog;
#endif
    }
  else
      _feedback.warning ((std::string ("Attempt to set source version for package")
                          + std::string(cp->name)
			  + "before creation of a version.").c_str());
}

void
IniDBBuilderPackage::buildPackageListNode (const std::string & name)
{
  if (currentNodeList)
    {
#if DEBUG
      Log (LOG_BABBLE) << "New node '" << name << "' for package list" << endLog;
#endif
      currentSpec = new PackageSpecification (name);
      currentNodeList->push_back (currentSpec);
    }
}

void
IniDBBuilderPackage::buildPackageListOperator (PackageSpecification::_operators const &_operator)
{
  if (currentSpec)
    {
      currentSpec->setOperator (_operator);
#if DEBUG
      Log (LOG_BABBLE) << "Current specification is " << *currentSpec << "." <<
	endLog;
#endif
    }
  else
    _feedback.warning ((std::string ("Attempt to set an operator for package ")
                        + std::string(cp->name)
		       + " with no current specification.").c_str());
}


void
IniDBBuilderPackage::buildPackageListOperatorVersion (const std::string& aVersion)
{
  if (currentSpec)
    {
      currentSpec->setVersion (aVersion);
#if DEBUG
      Log (LOG_BABBLE) << "Current specification is " << *currentSpec << "." <<
	endLog;
#endif
    }
  else
      _feedback.warning ((std::string ("Attempt to set an operator version for package ")
                          + std::string(cp->name)
			  + " with no current specification.").c_str());
}

/* privates */

void
IniDBBuilderPackage::add_correct_version()
{
  if (currentNodeList)
    *cbpv.depends() = *currentNodeList;

  int merged = 0;
  for (set<packageversion>::iterator n = cp->versions.begin();
       !merged && n != cp->versions.end(); ++n)
    if (*n == cbpv )
      {
	packageversion ver = *n;
        /* ASSUMPTIONS:
           categories and requires are consistent for the same version across
           all mirrors
           */
        /*
          XXX: if the versions are equal but the size/md5sum are different,
          we should alert the user, as they may not be getting what they expect...
        */
        /* Copy the binary mirror across if this site claims to have an install */
        if (cbpv.source()->sites.size() )
          ver.source()->sites.push_back(site (cbpv.source()->sites.begin()->key));
        /* Copy the descriptions across */
        if (cbpv.SDesc ().size() && !n->SDesc ().size())
          ver.set_sdesc (cbpv.SDesc ());
        if (cbpv.LDesc ().size() && !n->LDesc ().size())
          ver.set_ldesc (cbpv.LDesc ());
	if (cbpv.depends()->size() && !ver.depends ()->size())
	  *ver.depends() = *cbpv.depends();
	/* TODO: other package lists */
	/* Prevent dangling references */
	currentNodeList = NULL;
	currentSpec = NULL;
        cbpv = *n;
        merged = 1;
#if DEBUG
        Log (LOG_BABBLE) << cp->name << " merged with an existing version " << cbpv.Canonical_version() << endLog;
#endif
      }

  if (!merged)
    {
      cp->add_version (cbpv);
#if DEBUG
      Log (LOG_BABBLE) << cp->name << " version " << cbpv.Canonical_version() << " added" << endLog;
#endif
    }

  /*
    Should this version be the one selected for this package at a given
    stability/trust setting?  After merging potentially multiple package
    databases, we should pick the one with the highest version number.
  */
  packageversion *v = NULL;
  switch (trust)
  {
    case TRUST_CURR:
      v = &(cp->curr);
    break;
    case TRUST_TEST:
      v = &(cp->exp);
    break;
  }

  if (v)
    {
      int comparison = packageversion::compareVersions(cbpv, *v);

      if ((bool)(*v))
        Log (LOG_BABBLE) << "package " << cp->name << " comparing versions " << cbpv.Canonical_version() << " and " << v->Canonical_version() << ", result was " << comparison << endLog;

      if (comparison > 0)
        {
          *v = cbpv;
        }
    }
}

void
IniDBBuilderPackage::process_src (packagesource &src, const std::string& path)
{
  if (!src.Canonical())
    src.set_canonical (path.c_str());
  src.sites.push_back(site(parse_mirror));
}

void
IniDBBuilderPackage::setSourceSize (packagesource &src, const std::string& size)
{
  if (!src.size)
    src.size = atoi(size.c_str());
}

void
IniDBBuilderPackage::buildMessage (const std::string& message_id, const std::string& message)
{
  cp->set_message (message_id, message);
}
