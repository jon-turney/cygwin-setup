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
#include "IniParseFeedback.h"
#include "package_db.h"
#include "package_meta.h"
#include "package_version.h"
#include "cygpackage.h"
#include "filemanip.h"
#include "version.h"
// for strtoul
#include <string.h>
#include "LogSingleton.h"
#include "PackageSpecification.h"
#include <algorithm>

IniDBBuilderPackage::IniDBBuilderPackage (IniParseFeedback const &aFeedback) :
cp (0), cbpv (), cspv (), currentSpec (0), currentOrList (0), currentAndList (0), trust (0), _feedback (aFeedback){}

void
IniDBBuilderPackage::buildTimestamp (String const &time)
{
  timestamp = strtoul (time.cstr_oneuse(), 0, 0);
}

void
IniDBBuilderPackage::buildVersion (String const &aVersion)
{
  version = aVersion;
  if (version.size())
    {
      String ini_version = canonicalize_version (version);
      String our_version = canonicalize_version (::version);
      // XXX useversion < operator
      if (our_version.compare (ini_version) < 0)
	_feedback.warning("The current ini file is from a newer version of setup.exe. If you have any trouble installing, please download a fresh version from http://www.cygwin.com/setup.exe");
    }
}

void dumpAndList (vector<vector <PackageSpecification *> *> *currentAndList);

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
	  dumpAndList (cbpv.depends());
	}
    }
#endif
  packagedb db;
  cp = &db.packages.registerbykey(name);
  cbpv = cygpackage::createInstance (name);
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
  _feedback.warning(theDesc.cstr_oneuse());
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
  cspv = cygpackage::createInstance (cbpv.Name());
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
    cspv.source()->set_canonical (path.cstr_oneuse());
  cspv.source()->sites.push_back(site(parse_mirror));

  cbpv.setSourcePackageSpecification (PackageSpecification (cspv.Name()));

  // process_src (*cspv.source(), path);
  setSourceSize (*cspv.source(), size);
}

void
IniDBBuilderPackage::buildPackageTrust (int newtrust)
{
  trust = newtrust;
  if (newtrust != TRUST_UNKNOWN)
    {
      cbpv = cygpackage::createInstance (cp->name);
      cspv = packageversion ();
    }
}

void
IniDBBuilderPackage::buildPackageCategory (String const &name)
{
  cp->add_category (name);
}

void dumpAndList (vector<vector <PackageSpecification *> *> *currentAndList)
{
  if (currentAndList)
    {
      log (LOG_BABBLE) << "AND list is:" << endLog;
      for (vector<vector <PackageSpecification *> *>::const_iterator iAnd =
	   currentAndList->begin(); iAnd != currentAndList->end(); ++iAnd)
	{
    	  for (vector<PackageSpecification *>::const_iterator i= 
	       (*iAnd)->begin();
    	       i != (*iAnd)->end(); ++i)
   	      log(LOG_BABBLE) << **i << " |" << endLog;
	  log (LOG_BABBLE) << "end of OR list," << endLog;
	}
    }
}

void
IniDBBuilderPackage::buildBeginDepends ()
{
#if DEBUG
  log (LOG_BABBLE) << "Beginning of a depends statement for " << cp->name 
    << endLog;
  dumpAndList (currentAndList);
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
  dumpAndList (currentAndList);
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
  cbpv.source()->setInstalledSize (atoi(size.cstr_oneuse()));
#if DEBUG
  log (LOG_BABBLE) << "Installed size for " << cp->name << " is " << cpv->bin.installedSize() << endLog;
#endif
}

void
IniDBBuilderPackage::buildMaintainer (String const &){}

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
  dumpAndList (currentAndList);
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
  dumpAndList (currentAndList);
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
  dumpAndList (currentAndList);
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
  dumpAndList (currentAndList);
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
  dumpAndList (currentAndList);
#endif
  currentSpec = NULL;
  currentOrList = NULL; /* set by the build AndListNode */
  currentAndList = cbpv.provides();
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
		       + "before creation of a version.").cstr_oneuse());
}

void 
IniDBBuilderPackage::buildSourceName (String const &name)
{
  if (cbpv)
    {
      cbpv.setSourcePackageSpecification (PackageSpecification (name));
#if DEBUG
      log (LOG_BABBLE) << "\"" << cpv->sourcePackageSpecification() << 
	"\" is the source package for " << cp->name << "." << endLog;
#endif
    }
  else
      _feedback.warning ((String ("Attempt to set source for package")
			  + cp->name
			  + "before creation of a version.").cstr_oneuse());
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
			  + "before creation of a version.").cstr_oneuse());
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
			+ cp->name).cstr_oneuse());
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
			+ " before creation of a version.").cstr_oneuse());
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
		       + " with no current specification.").cstr_oneuse());
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
			  + " with no current specification.").cstr_oneuse());
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
    src.set_canonical (path.cstr_oneuse());
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
    src.size = atoi(size.cstr_oneuse());
}
