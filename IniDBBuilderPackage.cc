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
#include "ini.h"
// for strtoul
#include <string.h>
#include "LogSingleton.h"
#include "PackageSpecification.h"
#include <algorithm>

using namespace std;

IniDBBuilderPackage::IniDBBuilderPackage (IniParseFeedback const &aFeedback) :
currentSpec (0), _feedback (aFeedback){}

IniDBBuilderPackage::~IniDBBuilderPackage()
{
  process();
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
	    "version from https://cygwin.com/setup-%s.exe",
	    is_64bit ? "x86_64" : "x86",
	    is_64bit ? "x86_64" : "x86");
	  _feedback.warning(old_vers);
	}
    }
}

const std::string
IniDBBuilderPackage::buildMinimumVersion (const std::string& minimum)
{
  if (version_compare(setup_version, minimum) < 0)
    {
      char min_vers[256];
      snprintf (min_vers, sizeof(min_vers),
                "The current ini file requires at least version %s of setup.\n"
                "Please download a newer version from https://cygwin.com/setup-%s.exe",
                minimum.c_str(),
                is_64bit ? "x86_64" : "x86");
      return min_vers;
    }
  return "";
}

void
IniDBBuilderPackage::buildPackage (const std::string& _name)
{
  process();

  /* Reset for next package */
  name = _name;
  message_id = "";
  message_string = "";
  categories.clear();
  replace_versions.clear();

  cbpv.reponame = release;
  cbpv.version = "";
  cbpv.vendor = release;
  cbpv.sdesc = "";
  cbpv.ldesc = "";
  cbpv.stability = TRUST_CURR;
  cbpv.type = package_binary;
  cbpv.spkg = PackageSpecification();
  cbpv.spkg_id = packageversion();
  cbpv.requires = NULL;
  cbpv.obsoletes = NULL;
  cbpv.provides = NULL;
  cbpv.conflicts = NULL;
  cbpv.archive = packagesource();

  currentSpec = NULL;
  currentNodeList = NULL;
  dependsNodeList = PackageDepends();
  obsoletesNodeList = PackageDepends();
  providesNodeList = PackageDepends();
  conflictsNodeList = PackageDepends();
#if DEBUG
  Log (LOG_BABBLE) << "Created package " << name << endLog;
#endif
}

void
IniDBBuilderPackage::buildPackageVersion (const std::string& version)
{
  cbpv.version = version;
}

void
IniDBBuilderPackage::buildPackageSDesc (const std::string& theDesc)
{
  cbpv.sdesc = theDesc;
}

void
IniDBBuilderPackage::buildPackageLDesc (const std::string& theDesc)
{
  cbpv.ldesc = theDesc;
}

void
IniDBBuilderPackage::buildPackageInstall (const std::string& path,
                                          const std::string& size,
                                          char *hash,
                                          hashType type)
{
  // set archive path, size, mirror, hash
  cbpv.archive.set_canonical(path.c_str());
  cbpv.archive.size = atoi(size.c_str());
  cbpv.archive.sites.push_back(site(parse_mirror));

  switch (type) {
  case hashType::sha512:
    if (hash && !cbpv.archive.sha512_isSet)
      {
        memcpy (cbpv.archive.sha512sum, hash, sizeof(cbpv.archive.sha512sum));
        cbpv.archive.sha512_isSet = true;
      }
    break;

  case hashType::md5:
    if (hash && !cbpv.archive.md5.isSet())
      cbpv.archive.md5.set((unsigned char *)hash);
    break;

  case hashType::none:
    break;
  }
}

void
IniDBBuilderPackage::buildPackageSource (const std::string& path,
                                         const std::string& size,
                                         char *hash,
                                         hashType type)
{
  /* When there is a source: line, we invent a package to contain the source,
     and make it the source package for this package. */

  /* create a source package version */
  SolverPool::addPackageData cspv = cbpv;
  cspv.type = package_source;
  cspv.requires = NULL;
  cspv.obsoletes = NULL;
  cspv.provides = NULL;
  cspv.conflicts = NULL;

  /* set archive path, size, mirror, hash */
  cspv.archive = packagesource();
  cspv.archive.set_canonical(path.c_str());
  cspv.archive.size = atoi(size.c_str());
  cspv.archive.sites.push_back(site(parse_mirror));

  switch (type) {
  case hashType::sha512:
    if (hash && !cspv.archive.sha512_isSet)
      {
        memcpy (cspv.archive.sha512sum, hash, sizeof(cspv.archive.sha512sum));
        cspv.archive.sha512_isSet = true;
      }
    break;

  case hashType::md5:
    if (hash && !cspv.archive.md5.isSet())
      cspv.archive.md5.set((unsigned char *)hash);
    break;

  case hashType::none:
    break;
  }

  packagedb db;
  packageversion spkg_id = db.addSource (name + "-src", cspv);

  /* create relationship between binary and source packageversions */
  cbpv.spkg = PackageSpecification(name + "-src");
  cbpv.spkg_id = spkg_id;
}

void
IniDBBuilderPackage::buildPackageTrust (trusts newtrust)
{
  process();
  cbpv.stability = newtrust;
}

void
IniDBBuilderPackage::buildPackageCategory (const std::string& name)
{
  categories.insert(name);
}

void
IniDBBuilderPackage::buildBeginDepends ()
{
#if DEBUG
  Log (LOG_BABBLE) << "Beginning of a depends statement " << endLog;
#endif
  currentSpec = NULL;
  dependsNodeList = PackageDepends();
  currentNodeList = &dependsNodeList;
  cbpv.requires = &dependsNodeList;
}

void
IniDBBuilderPackage::buildBeginBuildDepends ()
{
#if DEBUG
  Log (LOG_BABBLE) << "Beginning of a Build-Depends statement" << endLog;
#endif
  currentSpec = NULL;
  currentNodeList = NULL;
  /* there is currently nowhere to store Build-Depends information */
}

void
IniDBBuilderPackage::buildBeginObsoletes ()
{
#if DEBUG
  Log (LOG_BABBLE) << "Beginning of an obsoletes statement" << endLog;
#endif
  currentSpec = NULL;
  obsoletesNodeList = PackageDepends();
  currentNodeList = &obsoletesNodeList;
  cbpv.obsoletes = &obsoletesNodeList;
}

void
IniDBBuilderPackage::buildBeginProvides ()
{
#if DEBUG
  Log (LOG_BABBLE) << "Beginning of a provides statement" << endLog;
#endif
  currentSpec = NULL;
  providesNodeList = PackageDepends();
  currentNodeList = &providesNodeList;
  cbpv.provides = &providesNodeList;
}

void
IniDBBuilderPackage::buildBeginConflicts ()
{
#if DEBUG
  Log (LOG_BABBLE) << "Beginning of a conflicts statement" << endLog;
#endif
  currentSpec = NULL;
  conflictsNodeList = PackageDepends();
  currentNodeList = &conflictsNodeList;
  cbpv.conflicts = &conflictsNodeList;
}

void
IniDBBuilderPackage::buildSourceName (const std::string& _name)
{
  // When there is a Source: line, that names a real source package
  packagedb db;
  cbpv.spkg = PackageSpecification(_name);
  cbpv.spkg_id = db.findSourceVersion (PackageSpecification(_name, cbpv.version));
#if DEBUG
  Log (LOG_BABBLE) << "\"" << _name << "\" is the source package for " << name << "." << endLog;
#endif
}

void
IniDBBuilderPackage::buildSourceNameVersion (const std::string& version)
{
  // XXX: should be stored as sourceevr
}

void
IniDBBuilderPackage::buildPackageListNode (const std::string & name)
{
#if DEBUG
  Log (LOG_BABBLE) << "New node '" << name << "' for package list" << endLog;
#endif
  currentSpec = new PackageSpecification (name);
  if (currentNodeList)
    currentNodeList->push_back (currentSpec);
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
}

void
IniDBBuilderPackage::buildMessage (const std::string& _message_id, const std::string& _message_string)
{
  message_id = _message_id;
  message_string = _message_string;
}

void
IniDBBuilderPackage::buildPackageReplaceVersionsList (const std::string& version)
{
  replace_versions.insert(version);
}

/* privates */
void
IniDBBuilderPackage::process ()
{
  if (!name.size())
    return;

  if (cbpv.version.empty())
    return;

#if DEBUG
  Log (LOG_BABBLE) << "Finished with package " << name << endLog;
  Log (LOG_BABBLE) << "Version " << cbpv.version << endLog;
#endif

  /* Transfer the accumulated package information to packagedb */
  packagedb db;
  packagemeta *pkg = db.addBinary (name, cbpv);

  // For no good historical reason, some data lives in packagemeta rather than
  // the packageversion
  for (auto i = categories.begin(); i != categories.end(); i++)
    {
      pkg->add_category(*i);
    }
  pkg->set_message(message_id, message_string);
  pkg->set_version_blacklist(replace_versions);

  // Reset for next version
  cbpv.version = "";
  cbpv.type = package_binary;
  cbpv.spkg = PackageSpecification();
  cbpv.spkg_id = packageversion();
  cbpv.archive = packagesource();
  obsoletesNodeList = PackageDepends();
  providesNodeList = PackageDepends();
  conflictsNodeList = PackageDepends();
}
