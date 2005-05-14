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
#include "filemanip.h"
// for strtoul
#include <string.h>
#include "LogSingleton.h"
#include "PackageSpecification.h"
#include <algorithm>

using namespace std;

IniDBBuilderPackage::IniDBBuilderPackage (IniParseFeedback const &aFeedback) :
cp (0), cbpv (), cspv (), currentSpec (0), currentOrList (0), currentAndList (0), trust (0), _feedback (aFeedback){}

inline bool lt_packagemeta(packagemeta *p1, packagemeta *p2) 
{ return p1->name.casecompare(p2->name) < 0; }

IniDBBuilderPackage::~IniDBBuilderPackage()
{
  packagedb db;
  sort (db.packages.begin(), db.packages.end(), lt_packagemeta);
}

void
IniDBBuilderPackage::buildTimestamp (String const &time)
{
  timestamp = strtoul (time.c_str(), 0, 0);
}

void
IniDBBuilderPackage::buildVersion (String const &aVersion)
{
  version = aVersion;
  if (version.size())
    {
      if (version_compare(setup_version, version) < 0)
	_feedback.warning("The current ini file is from a newer version of setup.exe. If you have any trouble installing, please download a fresh version from http://www.cygwin.com/setup.exe");
    }
}

void
IniDBBuilderPackage::buildPackage (String const &name)
{
#if DEBUG
  if (cp)
    {
      log (LOG_BABBLE) << "Finished with package " << cp->name << endLog;
      if (cbpv)
	{
	  log (LOG_BABBLE) << "Version " << cbpv.Canonical_version() << endLog;
	  log (LOG_BABBLE) << "Depends:" << endLog;
	  dumpAndList (cbpv.depends(), log(LOG_BABBLE));
	}
    }
#endif
  packagedb db;
  cp = db.findBinary (PackageSpecification(name));
  if (!cp)
    {
      cp = new packagemeta (name);
      db.packages.push_back (cp);
    }
  cbpv = cygpackage::createInstance (name, package_binary);
  cspv = packageversion ();
  currentSpec = NULL;
  currentOrList = NULL;
  currentAndList = NULL;
  trust = TRUST_CURR;
#if DEBUG
  log (LOG_BABBLE) << "Created package " << name << endLog;
#endif
}

void
IniDBBuilderPackage::buildPackageVersion (String const &version)
{
  cbpv.setCanonicalVersion (version);
  add_correct_version();
}

void
IniDBBuilderPackage::buildPackageSDesc (String const &theDesc)
{
  cbpv.set_sdesc(theDesc);
}

void
IniDBBuilderPackage::buildPackageLDesc (String const &theDesc)
{
  cbpv.set_ldesc(theDesc);
#if DEBUG
  _feedback.warning(theDesc.c_str());
#endif
}

void
IniDBBuilderPackage::buildPackageInstall (String const &path)
{
  process_src (*cbpv.source(), path);
}

void
IniDBBuilderPackage::buildPackageSource (String const &path, String const &size)
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
      csp->prev = packageversion();
      csp->curr = packageversion();
      csp->exp = packageversion();
      db.sourcePackages.push_back (csp);
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

  cbpv.setSourcePackageSpecification (PackageSpecification (cspv.Name()));

  // process_src (*cspv.source(), path);
  setSourceSize (*cspv.source(), size);
}

void
IniDBBuilderPackage::buildSourceFile (unsigned char const * md5, String const &size, String const &path)
{
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
IniDBBuilderPackage::buildPackageCategory (String const &name)
{
  cp->add_category (name);
}

void
IniDBBuilderPackage::buildBeginDepends ()
{
#if DEBUG
  log (LOG_BABBLE) << "Beginning of a depends statement for " << cp->name 
    << endLog;
  dumpAndList (currentAndList, log(LOG_BABBLE));
#endif
  currentSpec = NULL;
  currentOrList = NULL; /* set by the build AndListNode */
  currentAndList = cbpv.depends();
}

void
IniDBBuilderPackage::buildBeginPreDepends ()
{
#if DEBUG
  log (LOG_BABBLE) << "Beginning of a predepends statement" << endLog;
  dumpAndList (currentAndList, log(LOG_BABBLE));
#endif
  currentSpec = NULL;
  currentOrList = NULL; /* set by the build AndListNode */
  currentAndList = cbpv.predepends();
}

void
IniDBBuilderPackage::buildPriority (String const &priority)
{
  cp->priority = priority;
#if DEBUG
  log (LOG_BABBLE) << "Package " << cp->name << " is " << priority << endLog;
#endif
}

void
IniDBBuilderPackage::buildInstalledSize (String const &size)
{
  cbpv.source()->setInstalledSize (atoi(size.c_str()));
#if DEBUG
  log (LOG_BABBLE) << "Installed size for " << cp->name << " is " << cbpv.source()->installedSize() << endLog;
#endif
}

void
IniDBBuilderPackage::buildMaintainer (String const &){}

/* TODO: we can multiple arch's for a given package,
   and it may befor either source or binary, so we need to either set both
   or track a third current package that points to whether we altering source
   or binary at the moment
   */
void
IniDBBuilderPackage::buildArchitecture (String const &arch)
{
  cp->architecture = arch;
#if DEBUG
  log (LOG_BABBLE) << "Package " << cp->name << " is for " << arch << " architectures." << endLog;
#endif
}

void
IniDBBuilderPackage::buildInstallSize (String const &size)
{
  setSourceSize (*cbpv.source(), size);
}

void
IniDBBuilderPackage::buildInstallMD5 (unsigned char const * md5)
{
  if (md5 && !cbpv.source()->md5.isSet())
    cbpv.source()->md5.set(md5);
}

void
IniDBBuilderPackage::buildSourceMD5 (unsigned char const * md5)
{
  if (md5 && !cspv.source()->md5.isSet())
    cspv.source()->md5.set(md5);
}

void
IniDBBuilderPackage::buildBeginRecommends ()
{
#if DEBUG
  log (LOG_BABBLE) << "Beginning of a recommends statement" << endLog;
  dumpAndList (currentAndList, log(LOG_BABBLE));
#endif
  currentSpec = NULL;
  currentOrList = NULL; /* set by the build AndListNode */
  currentAndList = cbpv.recommends();
}

void
IniDBBuilderPackage::buildBeginSuggests ()
{
#if DEBUG
  log (LOG_BABBLE) << "Beginning of a suggests statement" << endLog;
  dumpAndList (currentAndList, log(LOG_BABBLE));
#endif
  currentSpec = NULL;
  currentOrList = NULL; /* set by the build AndListNode */
  currentAndList = cbpv.suggests();
}

void
IniDBBuilderPackage::buildBeginReplaces ()
{
#if DEBUG
  log (LOG_BABBLE) << "Beginning of a replaces statement" << endLog;
  dumpAndList (currentAndList, log(LOG_BABBLE));
#endif
  currentSpec = NULL;
  currentOrList = NULL; /* set by the build AndListNode */
  currentAndList = cbpv.replaces();
}

void
IniDBBuilderPackage::buildBeginConflicts ()
{
#if DEBUG
  log (LOG_BABBLE) << "Beginning of a conflicts statement" << endLog;
  dumpAndList (currentAndList, log(LOG_BABBLE));
#endif
  currentSpec = NULL;
  currentOrList = NULL; /* set by the build AndListNode */
  currentAndList = cbpv.conflicts();
}

void
IniDBBuilderPackage::buildBeginProvides ()
{
#if DEBUG
  log (LOG_BABBLE) << "Beginning of a provides statement" << endLog;
  dumpAndList (currentAndList, log(LOG_BABBLE));
#endif
  currentSpec = NULL;
  currentOrList = NULL; /* set by the build AndListNode */
  currentAndList = cbpv.provides();
}

void
IniDBBuilderPackage::buildBeginBuildDepends ()
{
#if DEBUG
  log (LOG_BABBLE) << "Beginning of a Build-Depends statement" << endLog;
  dumpAndList (currentAndList, log(LOG_BABBLE));
#endif
  currentSpec = NULL;
  currentOrList = NULL; /* set by the build AndListNode */
  currentAndList = cspv.depends ();
}

  
void
IniDBBuilderPackage::buildBeginBinary ()
{
#if DEBUG
  log (LOG_BABBLE) << "Beginning of a Binary statement" << endLog;
  dumpAndList (currentAndList, log(LOG_BABBLE));
#endif
  currentSpec = NULL;
  currentOrList = NULL; /* set by the build AndListNode */
  currentAndList = cspv.binaries ();
}

void
IniDBBuilderPackage::buildDescription (String const &descline)
{
  if (cbpv)
    {
      cbpv.set_ldesc(cbpv.LDesc() + descline + "\n");
#if DEBUG
      log (LOG_BABBLE) << "Description for " << cp->name << ": \"" << 
	descline << "\"." << endLog;
#endif
    }
  else
    _feedback.warning ((String ("Attempt to set description for package")
		       + cp->name 
		       + "before creation of a version.").c_str());
}

void 
IniDBBuilderPackage::buildSourceName (String const &name)
{
  if (cbpv)
    {
      cbpv.setSourcePackageSpecification (PackageSpecification (name));
#if DEBUG
      log (LOG_BABBLE) << "\"" << cbpv.sourcePackageSpecification() << 
	"\" is the source package for " << cp->name << "." << endLog;
#endif
    }
  else
      _feedback.warning ((String ("Attempt to set source for package")
			  + cp->name
			  + "before creation of a version.").c_str());
}

void
IniDBBuilderPackage::buildSourceNameVersion (String const &version)
{
  if (cbpv)
    {
      cbpv.sourcePackageSpecification().setOperator (PackageSpecification::Equals);
      cbpv.sourcePackageSpecification().setVersion (version);
#if DEBUG
      log (LOG_BABBLE) << "The source version needed for " << cp->name << 
	" is " << version << "." << endLog;
#endif
    }
  else
      _feedback.warning ((String ("Attempt to set source version for package")
			  + cp->name
			  + "before creation of a version.").c_str());
}

void
IniDBBuilderPackage::buildPackageListAndNode ()
{
  if (currentAndList)
    {
#if DEBUG
      log (LOG_BABBLE) << "New AND node for a package list" << endLog;
      if (currentOrList)
    	{
     	  ostream &os = log (LOG_BABBLE);
     	  os << "Current OR list is :";
     	  for (vector<PackageSpecification *>::const_iterator i= currentOrList->begin(); 
     	       i != currentOrList->end(); ++i)
     	      os << endl << **i;
     	  os << endLog;
    	}
#endif
      currentSpec = NULL;
      currentOrList = new vector<PackageSpecification *>;
      currentAndList->push_back (currentOrList);
    }
  else
    _feedback.warning ((String ("Attempt to add And node when no AndList"
				" present for package ")
			+ cp->name).c_str());
}

void
IniDBBuilderPackage::buildPackageListOrNode (String const &packageName)
{
  if (currentOrList)
    {
      currentSpec = new PackageSpecification (packageName);
      currentOrList->push_back (currentSpec);
#if DEBUG
      log (LOG_BABBLE) << "New OR node in a package list refers to \"" <<
        *currentSpec << "\"." << endLog;
#endif
    }
  else
    _feedback.warning ((String ("Attempt to set specification for package ")
			+ cp->name
			+ " before creation of a version.").c_str());
}

void
IniDBBuilderPackage::buildPackageListOperator (PackageSpecification::_operators const &_operator)
{
  if (currentSpec)
    {
      currentSpec->setOperator (_operator);
#if DEBUG
      log (LOG_BABBLE) << "Current specification is " << *currentSpec << "." <<
	endLog;
#endif
    }
  else
    _feedback.warning ((String ("Attempt to set an operator for package ")
		       + cp->name
		       + " with no current specification.").c_str());
}


void
IniDBBuilderPackage::buildPackageListOperatorVersion (String const &aVersion)
{
  if (currentSpec)
    {
      currentSpec->setVersion (aVersion);
#if DEBUG
      log (LOG_BABBLE) << "Current specification is " << *currentSpec << "." <<
	endLog;
#endif
    }
  else
      _feedback.warning ((String ("Attempt to set an operator version for package ")
			  + cp->name
			  + " with no current specification.").c_str());
}

/* privates */

void
IniDBBuilderPackage::add_correct_version()
{
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
	currentOrList = NULL;
	currentAndList = NULL;
	currentSpec = NULL;
        cbpv = *n;
        merged = 1;
      }
  if (!merged)
    cp->add_version (cbpv);
  /* trust setting */
  switch (trust)
  {
    case TRUST_CURR:
      if (cp->currtimestamp < timestamp)
      {
        cp->currtimestamp = timestamp;
        cp->curr = cbpv;
      }
    break;
    case TRUST_PREV:
    if (cp->prevtimestamp < timestamp)
    {
        cp->prevtimestamp = timestamp;
        cp->prev = cbpv;
    }
    break;
    case TRUST_TEST:
    if (cp->exptimestamp < timestamp)
    {
        cp->exptimestamp = timestamp;
        cp->exp = cbpv;
    }
    break;
  }
}

void
IniDBBuilderPackage::process_src (packagesource &src, String const &path)
{
  if (!src.Canonical())
    src.set_canonical (path.c_str());
  src.sites.push_back(site(parse_mirror));
  
  if (!cbpv.Canonical_version ().size())
    {
      fileparse f;
      if (parse_filename (path, f))
	{
	  cbpv.setCanonicalVersion (f.ver);
	  add_correct_version ();
	}
    }
}

void
IniDBBuilderPackage::setSourceSize (packagesource &src, String const &size)
{
  if (!src.size)
    src.size = atoi(size.c_str());
}
